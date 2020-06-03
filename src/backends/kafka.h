#ifndef __BRUBECK_KAFKA_H__
#define __BRUBECK_KAFKA_H__

#include <jansson.h>
#include <librdkafka/rdkafka.h>

struct brubeck_kafka_document {
  json_t *json;
  bool is_dirty;
};

struct brubeck_kafka {
  struct brubeck_backend backend;

  rd_kafka_t *rk; /* Producer instance handle */
  bool connected;
  const char *topic;
  const char *tag_subdocument;
  size_t bytes_sent;
  struct brubeck_kafka_document **documents;
};

struct brubeck_backend *brubeck_kafka_new(struct brubeck_server *server,
                                          json_t *settings, int shard_n);

#endif
