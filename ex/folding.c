#include <stdio.h>

int main() {
  int n = 1;
  n &= 0xffffffff;
  return n;
}
