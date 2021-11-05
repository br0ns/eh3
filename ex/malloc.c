#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  /* char buf[0x100]; */
  char *a, *b, *c, *d;
  a = malloc(0x100);
  b = malloc(0x100);
  c = malloc(0x100);
  d = malloc(0x100);

  printf("%p %p %p %p\n", a, b, c, d);
  free(a);
  free(b);
  free(c);
  free(d);
  a = malloc(0x100);
  b = malloc(0x100);
  c = malloc(0x100);
  d = malloc(0x100);
  printf("%p %p %p %p\n", a, b, c, d);

  return 0;
}
