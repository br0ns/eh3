#include <stdio.h>

#define SIZE_B 8
#define SIZE_W 32

int imm_size(long i) {
  int sz, oldi=i;
  for (sz = SIZE_B; sz <= SIZE_W; sz += SIZE_B, i >>= SIZE_B) {
    /* Get around buggy integer promotion in `ncc`. */
    if (i >= (int)-128 && i <= 127) {
      return sz;
    }
  }
  return -1;
  /* exit(1); */
}

int main() {
  int i;
  for (i = 0; i < 1000; i++) {
    printf("%d: %d\n", i, imm_size(i));
  }
}
