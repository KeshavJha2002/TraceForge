#include <iostream>

int main() {
  int a = 10;
  std::cout << "[state] a=" << a << "\n";
  int b = 20;
  std::cout << "[state] b=" << b << "\n";
  for(int i=0;i<5;++i) {
    if(i%2) {
      a = a+b;
      std::cout << "[state] a=" << a << "\n";
    } else {
      b = a-b;
      std::cout << "[state] b=" << b << "\n";
    }
  }
  return a+b;
}