#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  char a[0x100], b[0x100];

  strcpy(a, "hello");
  strcpy(b, a);
  puts(a);
  printf("%d\n", strlen(a));
  printf("%d\n", strlen(b));
  printf("%d\n", strcmp(a, b));
  b[0] = 'H';
  printf("%d\n", strcmp(a, b));

  return 0;
}
