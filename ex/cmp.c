int main() {
  int x, a, b, c, d, e, f;
  x = 0xff;
  a = 0xff == x;
  b = 0xff != x;
  c = 0xff < x;
  d = 0xff <= x;
  e = 0xff > x;
  printf("%d %d %d %d %d %d\n", a, b,c, d, e, f);
  x = -10;
  a = x == -10;
  b = x != -10;
  c = x < -10;
  d = x <= -10;
  e = x > -10;
  printf("%d %d %d %d %d %d\n", a, b,c, d, e, f);
  x = 10;
  if (x > -20) {
    puts("success");
  }
  return 0;
}
