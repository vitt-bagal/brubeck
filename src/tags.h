#ifndef __BRUBECK_TAGS_H__
#define __BRUBECK_TAGS_H__

#include <stdint.h>

struct brubeck_tag {
  const char* key;
  const char* value;
};

struct brubeck_tag_set {
  uint32_t index;
  uint16_t tag_len;
  uint16_t num_tags;
  struct brubeck_tag tags[];
};

typedef struct brubeck_tags_t brubeck_tags_t;
brubeck_tags_t * brubeck_tags_create(const uint64_t size);

// The input string must not be freed until the returned tag set is also freed
struct brubeck_tag_set *brubeck_parse_tags(char*, uint16_t);

const struct brubeck_tag_set *brubeck_get_tag_set(struct brubeck_tags_t*, const char* tag_str, uint16_t tag_str_len);

#endif
