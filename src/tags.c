#include "brubeck.h"
#include "ck_ht.h"
#include "ck_malloc.h"

// CR lpalmer: do slab thing
static const char char_assoc = '=';
static const char *str_assoc = "=";
static const char *str_delim = ",";

struct brubeck_tags_t {
  uint32_t num_tag_sets;
  ck_ht_t table;
  pthread_mutex_t write_mutex;
};

static void *tags_malloc(size_t r) { return xmalloc(r); }

static void tags_free(void *p, size_t b, bool r) { free(p); }

static struct ck_malloc ALLOCATOR = {.malloc = tags_malloc, .free = tags_free};

brubeck_tags_t *brubeck_tags_create(const uint64_t size) {
  brubeck_tags_t *tags = xmalloc(sizeof(brubeck_tags_t));
  memset(tags, 0x0, sizeof(brubeck_tags_t));
  pthread_mutex_init(&tags->write_mutex, NULL);

  if (!ck_ht_init(&tags->table, CK_HT_MODE_BYTESTRING, NULL, &ALLOCATOR, size,
                  0x0BADF00D)) {
    free(tags);
    return NULL;
  }

  return tags;
}

bool brubeck_tags_insert(brubeck_tags_t *tags, const char *key,
                         uint16_t key_len, struct brubeck_tag_set *val) {
  ck_ht_hash_t h;
  ck_ht_entry_t entry;
  bool result;

  ck_ht_hash(&h, &tags->table, key, key_len);
  ck_ht_entry_set(&entry, h, key, key_len, val);

  pthread_mutex_lock(&tags->write_mutex);
  result = ck_ht_put_spmc(&tags->table, h, &entry);
  if (result) {
    val->index = (tags->num_tag_sets)++;
  }
  pthread_mutex_unlock(&tags->write_mutex);
  return result;
}

struct brubeck_tag_set *brubeck_tags_find(brubeck_tags_t *tags, const char *key,
                                          uint16_t key_len) {
  ck_ht_hash_t h;
  ck_ht_entry_t entry;

  ck_ht_hash(&h, &tags->table, key, key_len);
  ck_ht_entry_key_set(&entry, key, key_len);

  if (ck_ht_get_spmc(&tags->table, h, &entry))

    return ck_ht_entry_value(&entry);

  return NULL;
}

const uint16_t brubeck_tag_offset(const char *str) {
  uint16_t offset = 0;
  // Support both InfluxDB and Librato tag formats
  for (offset = 0; *str && *str != ',' && *str != '#'; ++offset) {
    ++str;
  }
  return offset;
}

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

struct brubeck_tag_set *brubeck_parse_tags(char *tag_str,
                                           uint16_t tag_str_len) {
  char *state;

  uint16_t num_possible_tags =
      tag_str ? count_char_in_str(tag_str, char_assoc) : 0;

  size_t alloc_size = sizeof(struct brubeck_tag_set) +
                      num_possible_tags * sizeof(struct brubeck_tag *);
  struct brubeck_tag_set *tag_set = malloc(alloc_size);
  memset(tag_set, 0x0, alloc_size);

  if (tag_str) {
    tag_set->tag_len = tag_str_len;
    for (tag_str = strtok_r(tag_str, str_delim, &state); tag_str;
         tag_str = strtok_r(NULL, str_delim, &state)) {
      if (parse_tag(tag_str, &(tag_set->tags[tag_set->num_tags]))) {
        ++(tag_set->num_tags);
      }
    }
  }
  return tag_set;
}

const struct brubeck_tag_set *
brubeck_get_tag_set_of_tag_str(struct brubeck_tags_t *tags, const char *tag_str,
                               uint16_t tag_str_len) {
  struct brubeck_tag_set *tag_set;
  tag_set = brubeck_tags_find(tags, tag_str, tag_str_len);

  if (tag_set == NULL) {
    char *tag_str_copy = strdup(tag_str);
    tag_set = brubeck_parse_tags(tag_str_copy, tag_str_len);
    if (!brubeck_tags_insert(tags, tag_str, tag_str_len, tag_set)) {
      free(tag_str_copy);
      free(tag_set);
      tag_set = brubeck_tags_find(tags, tag_str, tag_str_len);
    }
  }

  return tag_set;
}

const struct brubeck_tag_set *brubeck_get_tag_set(struct brubeck_tags_t *tags,
                                                  const char *key_str,
                                                  uint16_t key_len) {
  uint16_t tag_offset;
  tag_offset = brubeck_tag_offset(key_str);

  return brubeck_get_tag_set_of_tag_str(tags, key_str + tag_offset,
                                        key_len - tag_offset);
}
