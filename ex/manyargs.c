#include <stdio.h>
int foo(int a, int b, int c, int d, int e, int f) {
  int x = a+b+c+d+e+f;
  printf("a+b+c+d+e+f=%d\n", x);
  return x;
}
int main() {
  foo(1,2,3,4,5,6);
  return 0;
}
