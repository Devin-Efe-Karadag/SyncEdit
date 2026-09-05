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

    char c = g() % 8 == 0 ? '\n' : static_cast<char>('a' + g() % 26);
    r.insert(p, {{1, i + 1}, c});
    s.insert(p, 1, c);
    ce::check(r.at(p).id == ce::Id{1, i + 1}, "ID mapping");
    ce::check(r.index({1, i + 1}) == p, "reverse mapping");

    if (s.size() > 50 && g() % 2) {
      p = g() % s.size();
      s.erase(p, 1);
      r.erase(p);
    }

    size_t probe = g() % (s.size() + 1);

    size_t line = static_cast<size_t>(
        std::count(s.begin(), s.begin() + static_cast<std::ptrdiff_t>(probe), '\n'));
    ce::check(r.line_of(probe) == line, "newline rank");

    size_t start = probe;

    while (start && s[start - 1] != '\n')
      --start;
    ce::check(r.line_start(line) == start, "line start select");

    if (!s.empty()) {
      size_t at = g() % s.size();
      ce::check(r.index(r.at(at).id) == at, "stable handle after chunk split");
    }
    ce::check(r.size() == s.size() && r.range(0, r.size()) == s, "rope differential");
