#include "tags.h"
#include "sput.h"

static void check_tags_equal(const struct brubeck_tag_set *x,
                             const struct brubeck_tag_set *y,
                             const char *description) {
  sput_fail_unless(y->num_tags == x->num_tags, description);
  for (int i = 0; i < y->num_tags; ++i) {
    sput_fail_unless(strcmp(x->tags[i].key, y->tags[i].key) == 0, description);
    sput_fail_unless(strcmp(x->tags[i].value, y->tags[i].value) == 0,
                     description);
  }
}

static void check_equal(const struct brubeck_tag_set *x,
                        const struct brubeck_tag_set *y,
                        const char *description) {
  sput_fail_if((x && !y) || (y && !x), description);
  sput_fail_unless(y->tag_len == x->tag_len, description);
  check_tags_equal(x, y, description);
}

const struct brubeck_tag_set *parse(const char *source) {
  char *str;
  uint16_t len = strlen(source);

  str = strdup(source);
  return brubeck_parse_tags(str, len);
}

static void check_parse(const char *source,
                        struct brubeck_tag_set *expected_without_tags,
                        const struct brubeck_tag tag_arr[]) {
  struct brubeck_tag_set *expected =
      malloc(sizeof(struct brubeck_tag_set) +
             sizeof(struct brubeck_tag) * expected_without_tags->num_tags);
  memcpy(expected, expected_without_tags, sizeof(struct brubeck_tag_set));
  memcpy(expected->tags, tag_arr,
         sizeof(struct brubeck_tag) * expected_without_tags->num_tags);

  const struct brubeck_tag_set *tags = parse(source);
  check_equal(expected, tags, source);
}

void test_tag_parsing(void) {
  /* expected */
  check_parse("", &(struct brubeck_tag_set){.tag_len = 0, .num_tags = 0},
              (struct brubeck_tag[]){});
  check_parse("foo", &(struct brubeck_tag_set){.tag_len = 3, .num_tags = 0},
              (struct brubeck_tag[]){});
  check_parse("foo=bar", &(struct brubeck_tag_set){.tag_len = 7, .num_tags = 1},
              (struct brubeck_tag[]){{.key = "foo", .value = "bar"}});
  check_parse(",foo=bar",
              &(struct brubeck_tag_set){.tag_len = 8, .num_tags = 1},
              (struct brubeck_tag[]){{.key = "foo", .value = "bar"}});
  check_parse("s,foo=bar,baz=42",
              &(struct brubeck_tag_set){.tag_len = 16, .num_tags = 2},
              (struct brubeck_tag[]){{"foo", "bar"}, {"baz", "42"}});

  /* malformed */
  check_parse(",foo=bar,,baz=42,",
              &(struct brubeck_tag_set){.tag_len = 17, .num_tags = 2},
              (struct brubeck_tag[]){{"foo", "bar"}, {"baz", "42"}});
  check_parse("s,junk,=,junk=,=junk",
              &(struct brubeck_tag_set){.tag_len = 20, .num_tags = 0},
              (struct brubeck_tag[]){});
  check_parse("s,foo===bar",
              &(struct brubeck_tag_set){.tag_len = 11, .num_tags = 1},
              (struct brubeck_tag[]){{"foo", "bar"}});
  check_parse("foo=bar,",
              &(struct brubeck_tag_set){.tag_len = 8, .num_tags = 1},
              (struct brubeck_tag[]){{.key = "foo", .value = "bar"}});
}

const struct brubeck_tag_set *get_tag_set(struct brubeck_tags_t *tags,
                                          const char *tag_str) {
  uint16_t tag_str_len = strlen(tag_str);
  return brubeck_get_tag_set(tags, tag_str, tag_str_len);
}

void test_tag_storage(void) {
  struct brubeck_tags_t *tags = brubeck_tags_create(1024);
  const struct brubeck_tag_set *t1, *t2;
  const char *str;

  str = "foo=bar";
  t1 = get_tag_set(tags, str);
  sput_fail_if(t1 == NULL, "not null");
  check_equal(t1, parse(str), str);
  sput_fail_unless(t1->index == 0, "index");

  t2 = get_tag_set(tags, str);
  sput_fail_if(t2 == NULL, "not null");
  check_equal(t2, parse(str), str);
  sput_fail_unless(t1 == t2, "caching");

  // An expected behavior nuance: A different string that is logically
  // equivalent results in a physically different tag set having all
  // members the same except for the index
  t2 = get_tag_set(tags, "foo=bar,");
  sput_fail_unless(t2->index == 1, "index");
  check_tags_equal(t1, t2, "equivalent except index");
}

void check_tag_offset(const char *str, const uint16_t offset) {
  sput_fail_unless(brubeck_tag_offset(str) == offset, str);
}

void test_tag_offset(void) {
  check_tag_offset("", BRUBECK_NO_TAG_OFFSET);
  check_tag_offset("no.tags", BRUBECK_NO_TAG_OFFSET);
  check_tag_offset("has.tags,foo=bar,baz=42", 8);
  check_tag_offset("has.tags#foo=bar,baz=42", 8);
}
