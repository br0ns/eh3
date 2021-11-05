#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

void foo(char *buf, char *fmt, ...) {
  FILE f = {-1, EOF};
  int ret;
  va_list ap;

  va_start(ap, fmt);
  f.obuf = buf;
  f.osize = sizeof(buf) - 1;
  ret = vfprintf(&f, fmt, ap);
  buf[f.olen] = '\0';

  /* vfprintf(stdout, fmt, ap); */
  va_end(ap);
}

int main(int argc, char *argv[]) {
  /* char buf[0x100]; */

  /* /\* foo(buf, "%s%s%s\n", "a", "b", "c"); *\/ */

  /* /\* char buf[0x100]; *\/ */
  /* sprintf(buf, "%s", "hello"); */
  /* puts(buf); */

  if (argc != 2) {
    printf("usage: %s <name>\n", argv[0]);
    return 1;
  }
  printf("Hello, %s\n", argv[1]);

  return 0;
}
