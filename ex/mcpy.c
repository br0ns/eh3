struct foo {
  int a, b, c;
};

int main() {
  struct foo x, y = {0};
  x.a = 1;
  x.b = 2;
  x.c = 3;
  y = x;
  return 0;
}
