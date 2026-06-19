#include "rope.hpp"
#include <algorithm>
#include <iostream>
#include <random>
int main() {
  ce::Rope r;

  std::string s;

  std::mt19937 g(42);

  for (size_t i = 0; i < 10000; ++i) {
    size_t p = g() % (s.size() + 1);
