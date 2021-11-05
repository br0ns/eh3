int foo() {return 1;}
int bar() {return 2;}
int baz() {return 3;}
int main() {
  int x;
  x = foo();
  if (x) {
    bar();
  }
  return x;
}
