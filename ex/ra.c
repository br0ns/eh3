#include <stdio.h>

int foo() {
  int a, b, c, d, e, f, g, h, i, j;
  a = 0;
  b = a + 1;
  c = a + b + 2;
  /* d = a + b + c + 3; */
  /* e = a + b + c + d + 4; */
  /* f = a + b + c + d + e + 5; */
  /* g = a + b + c + d + e + f + 6; */
  /* h = a + b + c + d + e + f + g + 7; */
  /* i = a + b + c + d + e + f + g + h + 8; */
  /* j = a + b + c + d + e + f + g + h + i + 9; */
  return a + b + c;
}

int main(int argc, char *argv[]) {
  foo();
  return 0;
}
