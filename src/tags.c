#include "tags.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// CR lpalmer: do slab thing
static const char char_assoc = '=';
static const char *str_assoc = "=";
static const char char_delim = ',';
static const char *str_delim = ",";
#define TAG_BUF_SIZE 1024
static char buf[TAG_BUF_SIZE];

const uint16_t count_char_in_str(const char *str, char c) {
  uint16_t count;
  for (count = 0; str[count]; str[count] == c ? count++ : c++)
    ;
  return count;
}

bool parse_tag(char *kv_str, struct brubeck_tag *tag) {
  char *state, *key, *value;

  key = strtok_r(kv_str, str_assoc, &state);
  if (key == NULL)
    return false;
  value = strtok_r(NULL, str_assoc, &state);
  if (value == NULL)
    return false;

  tag->key = key;
  tag->value = value;
  return true;
}

const struct brubeck_tags *brubeck_parse_tags(const char *name) {
  char *state, *tok;

  // CR lpalmer: I don't need to take the whole name here because we will already need to have it separated out for hash lookup
  tok = strchr(name, char_delim);

  uint16_t num_possible_tags = tok ? count_char_in_str(tok, char_assoc) : 0;

  size_t alloc_size = sizeof(struct brubeck_tags) +
                      num_possible_tags * sizeof(struct brubeck_tag *);
  struct brubeck_tags *tags = malloc(alloc_size);
  memset(tags, 0x0, alloc_size);

  if (tok) {
    tags->tag_len = strlen(tok);
    strncpy(buf, tok, TAG_BUF_SIZE);
    for (tok = strtok_r(buf, str_delim, &state); tok;
         tok = strtok_r(NULL, str_delim, &state)) {
      if (parse_tag(tok, &(tags->tags[tags->num_tags]))) {
        ++(tags->num_tags);
      }
    }
  }
  return tags;
}
