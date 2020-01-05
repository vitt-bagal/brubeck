#include "tags.h"
#include "sput.h"

static void check_parse(const char *source, struct brubeck_tags *expected,
                        const struct brubeck_tag tag_arr[]) {
  char str[256];
  strcpy(str, source);

  const struct brubeck_tags *tags = brubeck_parse_tags(str);
  sput_fail_if((expected && !tags) || (tags && !expected), source);
  sput_fail_unless(tags->num_tags == expected->num_tags, source);
  sput_fail_unless(tags->tag_len == expected->tag_len, source);
  for (int i = 0; i < tags->num_tags; ++i) {
    sput_fail_unless(strcmp(tags->tags[i].key, tag_arr[i].key) == 0, source);
    sput_fail_unless(strcmp(tags->tags[i].value, tag_arr[i].value) == 0,
                     source);
  }
}

void test_tags(void) {
  /* expected */
  check_parse("", &(struct brubeck_tags){.tag_len = 0, .num_tags = 0},
              (struct brubeck_tag[]){});
  check_parse("foo", &(struct brubeck_tags){.tag_len = 0, .num_tags = 0},
              (struct brubeck_tag[]){});
  check_parse("foo=bar", &(struct brubeck_tags){.tag_len = 0, .num_tags = 0},
              (struct brubeck_tag[]){});
  check_parse("s,foo=bar", &(struct brubeck_tags){.tag_len = 8, .num_tags = 1},
              (struct brubeck_tag[]){{.key = "foo", .value = "bar"}});
  check_parse("s,foo=bar,baz=42",
              &(struct brubeck_tags){.tag_len = 15, .num_tags = 2},
              (struct brubeck_tag[]){{"foo", "bar"}, {"baz", "42"}});

  /* malformed */
  check_parse(",foo=bar,,baz=42,",
              &(struct brubeck_tags){.tag_len = 17, .num_tags = 2},
              (struct brubeck_tag[]){{"foo", "bar"}, {"baz", "42"}});
  check_parse("s,junk,=,junk=,=junk",
              &(struct brubeck_tags){.tag_len = 19, .num_tags = 0},
              (struct brubeck_tag[]){});
  check_parse("s,foo===bar",
              &(struct brubeck_tags){.tag_len = 10, .num_tags = 1},
              (struct brubeck_tag[]){{"foo", "bar"}});
  check_parse("foo=bar,", &(struct brubeck_tags){.tag_len = 1, .num_tags = 0},
              (struct brubeck_tag[]){});
}
