#include <stdlib.h>
#include <stdio.h>

#define TRACE

#include "../trace.h"

#define T_FOO (1 << 0)
#define T_BAR (1 << 1)
#define T_BAZ (1 << 2)

int main(void) {
  t_tags_on(T_BAR);
  t_tag_mark(T_FOO, "foo");
  t_tag_mark(T_BAR, "bar");
  t_tag_mark(T_BAZ, "baz");
  info(T_FOO, "Hello");
  info(T_BAR, "Barbaaar");
  info(T_FOO, ", there\n");
  info(T_BAR|T_BAZ, "foo\nbar\nbaz");
  info(T_BAR, "\n");
  return 0;
}
