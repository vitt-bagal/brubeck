#ifndef __BRUBECK_KAFKA_H__
#define __BRUBECK_KAFKA_H__

#include <jansson.h>
#include <librdkafka/rdkafka.h>

struct brubeck_kafka {
  struct brubeck_backend backend;

  rd_kafka_t *rk; /* Producer instance handle */
  bool connected;
  json_t *json;
  bool doc_is_dirty;
  const char *topic;
  size_t bytes_sent;
};

struct brubeck_backend *brubeck_kafka_new(struct brubeck_server *server,
                                          json_t *settings, int shard_n);

#endif
