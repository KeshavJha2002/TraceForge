#define base 10

int func(int a) {
  if(a <= 1) return a;
  return a*base+func(a-1);
}

int main() {
  return func(5);
}