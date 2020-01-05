#ifndef __BRUBECK_TAGS_H__
#define __BRUBECK_TAGS_H__

#include <stdint.h>

struct brubeck_tag {
  const char* key;
  const char* value;
};

struct brubeck_tags {
  uint16_t tag_len;
  uint16_t num_tags;
  struct brubeck_tag tags[];
};

const struct brubeck_tags *brubeck_parse_tags(const char*);

#endif
