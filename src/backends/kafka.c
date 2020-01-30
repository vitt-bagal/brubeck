#include "brubeck.h"
#include <jansson.h>
#include <librdkafka/rdkafka.h>
#include <string.h>

static char errstr[512]; /* librdkafka API error reporting buffer */

static int kafka_shutdown(void *backend) {
  struct brubeck_kafka *self = (struct brubeck_kafka *)backend;
  rd_kafka_resp_err_t err;

  self->connected = false;

  int i;
  for (i = 0; i < vector_size(self->documents); ++i) {
    if (self->documents[i] != NULL)
      json_decref(self->documents[i]->json);
  }

  /* Fatal error handling.
   *
   * When a fatal error is detected by the producer instance,
   * it will trigger an error_cb with ERR__FATAL set.
   * The application should use rd_kafka_fatal_error() to extract
   * the actual underlying error code and description, propagate it
   * to the user (for troubleshooting), and then terminate the
   * producer since it will no longer accept any new messages to
   * produce().
   *
   * Note:
   *   After a fatal error has been raised, rd_kafka_produce*() will
   *   fail with the original error code.
   *
   * Note:
   *   As an alternative to an error_cb, the application may call
   *   rd_kafka_fatal_error() at any time to check if a fatal error
   *   has occurred, typically after a failing rd_kafka_produce*() call.
   */

  err = rd_kafka_fatal_error(self->rk, errstr, sizeof(errstr));
  if (err)
    log_splunk("backend=kafka event=fatal_error reason=%s msg=\"%s\"",
               rd_kafka_err2name(err), errstr);

  /* Clean termination to get delivery results (from rd_kafka_flush())
   * for all outstanding/in-transit/queued messages. */
  log_splunk("backend=kafka event=flushing-outstanding-messages");
  rd_kafka_flush(self->rk, 10 * 1000 /* wait for max 10 seconds */);
  rd_kafka_destroy(self->rk);
  return err;
}

/**
 * @brief Message delivery report callback.
 *
 * This callback is called exactly once per message, indicating if
 * the message was succesfully delivered
 * (rkmessage->err == RD_KAFKA_RESP_ERR_NO_ERROR) or permanently
 * failed delivery (rkmessage->err != RD_KAFKA_RESP_ERR_NO_ERROR).
 *
 * The callback is triggered from rd_kafka_poll() and executes on
 * the application's thread.
 */
static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage,
                      void *opaque) {
  if (rkmessage->err)
    log_splunk("backend=kafka event=delivery_error msg=\"%s\"",
               rd_kafka_err2name(rkmessage->err));

  /* The rkmessage is destroyed automatically by librdkafka */
}

/**
 * @brief Generic error handling callback.
 *
 * This callback is triggered by rd_kafka_poll() or rd_kafka_flush()
 * for client instance-level errors, such as broker connection failures,
 * authentication issues, etc.
 *
 * These errors should generally be considered informational as
 * the underlying client will automatically try to recover from
 * any errors encountered, the application does not need to take
 * action on them.
 *
 * But with idempotence truly fatal errors can be raised when
 * the idempotence guarantees can't be satisfied, these errors
 * are identified by a the `RD_KAFKA_RESP_ERR__FATAL` error code.
 */
static void error_cb(rd_kafka_t *rk, int err, const char *reason,
                     void *opaque) {
  struct brubeck_kafka *self = (struct brubeck_kafka *)opaque;
  log_splunk("backend=kafka event=producer_error reason=%s "
             "msg=\"%s\"",
             rd_kafka_err2name(err), reason);

  if (err != RD_KAFKA_RESP_ERR__FATAL)
    return;

  if (kafka_shutdown(self))
    exit(1);
}

static bool kafka_is_connected(void *backend) {
  struct brubeck_kafka *self = (struct brubeck_kafka *)backend;
  return self->connected;
}

static int kafka_connect(void *backend) {
  struct brubeck_kafka *self = (struct brubeck_kafka *)backend;

  // Connection and recovery is meant to be transparent to the client
  if (self->connected)
    return 0;
  else
    return -1;
}

static void each_metric(const struct brubeck_metric *metric, const char *key,
                        value_t value, void *backend) {
  struct brubeck_kafka *self = (struct brubeck_kafka *)backend;

  uint32_t tag_index = 0;
  if (metric->tags != NULL)
    tag_index = metric->tags->index;
  struct brubeck_kafka_document *doc = vector_get(self->documents, tag_index);
  if (doc == NULL) {
    doc = malloc(sizeof(struct brubeck_kafka_document));
    doc->is_dirty = false;
    doc->json = json_object();
    vector_set(self->documents, tag_index, doc);
  }

  if (doc->is_dirty == false && metric->tags != NULL) {
    uint16_t i;
    for (i = 0; i < metric->tags->num_tags; ++i) {
      json_object_set_new_nocheck(doc->json, metric->tags->tags[i].key,
                                  json_string(metric->tags->tags[i].value));
    }
  }

  json_object_set_new_nocheck(doc->json, key, json_real(value));
  doc->is_dirty = true;
}

static void kafka_flush(void *backend) {
  struct brubeck_kafka *self = (struct brubeck_kafka *)backend;
  rd_kafka_resp_err_t err;
  char *buf;
  size_t len;
  int64_t epoch_ms;
  json_t *json_epoch_ms;

  epoch_ms = (int64_t)self->backend.tick_time;
  epoch_ms *= 1000;
  json_epoch_ms = json_integer(epoch_ms);

  int i;
  struct brubeck_kafka_document *doc;
  for (i = 0; i < vector_size(self->documents); ++i) {
    doc = self->documents[i];
    if (doc != NULL && doc->is_dirty) {
      json_object_set_nocheck(doc->json, "@timestamp", json_epoch_ms);
      buf = json_dumps(doc->json, JSON_COMPACT);
      len = strlen(buf);
      err = rd_kafka_producev(self->rk, RD_KAFKA_V_TOPIC(self->topic),
                              RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_FREE),
                              RD_KAFKA_V_VALUE(buf, len),
                              RD_KAFKA_V_OPAQUE(NULL), RD_KAFKA_V_END);
      if (err) {
        log_splunk("backend=kafka event=failed_to_enqueue msg=\"%s\"",
                   rd_kafka_err2str(err));
        free(buf);
      } else {
        self->bytes_sent += len;
      }
      json_object_clear(doc->json);
      doc->is_dirty = false;
      rd_kafka_poll(self->rk, 0);
    }
  }
  json_decref(json_epoch_ms);
}

static rd_kafka_conf_t *build_rdkafka_config(json_t *json) {
  rd_kafka_conf_t *conf;
  int retval;
  const char *key;
  json_t *value;

  conf = rd_kafka_conf_new();

  json_object_foreach(json, key, value) {
    retval = rd_kafka_conf_set(conf, key, json_string_value(value), errstr,
                               sizeof(errstr));
    if (retval != RD_KAFKA_CONF_OK) {
      log_splunk("backend=kafka event=conf_error key=%s code=%s msg=\"%s\"",
                 key, rd_kafka_err2name(retval), errstr);
      rd_kafka_conf_destroy(conf);
      exit(1);
    }
  }
  return conf;
}

struct brubeck_backend *brubeck_kafka_new(struct brubeck_server *server,
                                          json_t *settings, int shard_n) {
  struct brubeck_kafka *self = xcalloc(1, sizeof(struct brubeck_kafka));
  int frequency = 0;
  json_t *rdkafka_config;
  rd_kafka_conf_t *conf;

  json_unpack_or_die(settings, "{s:s, s:i, s:o}", "topic", &self->topic,
                     "frequency", &frequency, "rdkafka_config",
                     &rdkafka_config);
  conf = build_rdkafka_config(rdkafka_config);

  self->connected = true;
  self->backend.type = BRUBECK_BACKEND_KAFKA;
  self->backend.connect = &kafka_connect;
  self->backend.is_connected = &kafka_is_connected;

  self->backend.sample = &each_metric;
  self->backend.flush = &kafka_flush;

  self->backend.sample_freq = frequency;
  self->backend.server = server;

  /* Set the delivery report callback.
   * This callback will be called once per message to inform
   * the application if delivery succeeded or failed.
   * See dr_msg_cb() above.
   * The callback is only triggered from rd_kafka_poll() and
   * rd_kafka_flush(). */
  rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);

  /* Set an error handler callback to catch generic instance-level
   * errors.
   *
   * See the `error_cb()` handler above for how to handle the
   * fatal errors.
   */
  rd_kafka_conf_set_error_cb(conf, error_cb);

  rd_kafka_conf_set_opaque(conf, self);

  self->rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));

  if (!self->rk) {
    log_splunk("backend=kafka event=producer_creation_failed error=\"%s\"",
               errstr);
    rd_kafka_conf_destroy(conf);
    exit(1);
  }

  log_splunk("backend=kafka event=ready");
  // TODO: the backend should be given an opportunity to cleanly shut down

  brubeck_backend_run_threaded((struct brubeck_backend *)self);
  log_splunk("backend=kafka event=started");

  return (struct brubeck_backend *)self;
}
