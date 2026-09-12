#include "crdt.hpp"
#include <chrono>
#include <iostream>
#include <random>
int main() {
  using namespace ce;
  using Clock = std::chrono::steady_clock;

  std::cout << "characters,seed_ms,random_edits,edit_ms,lookup_ms\n";

  for (size_t size : {1000U, 5000U, 10000U}) {
    Crdt c;

    auto start = Clock::now();
    Id parent{};

    for (size_t i = 1; i <= size; ++i) {
      Operation o{{1, i}, parent, i, 'a', true};
      c.receive(o);
      parent = o.id;
    }

    auto seeded = Clock::now();

    std::mt19937 rng(42);
    uint64_t counter = size;

    for (size_t i = 0; i < 1000; ++i) {
      size_t p = rng() % (c.rope.size() + 1);
      c.receive({{1, ++counter}, p ? c.rope.at(p - 1).id : Id{}, c.clock + 1, 'x', true});

      if (i % 2 == 0)
        c.receive({{1, ++counter}, c.rope.at(rng() % c.rope.size()).id, c.clock + 1, 0, false});
    }

    auto edited = Clock::now();

    size_t checkSum = 0;

    for (size_t i = 0; i < 1000; ++i) {
      size_t p = rng() % c.rope.size();

      auto actual = c.rope.index(c.rope.at(p).id);
      check(actual == p, "benchmark lookup");
      checkSum += actual;
    }

    auto looked = Clock::now();
    (void)checkSum;

    auto ms = [](auto a, auto b) {
      return std::chrono::duration<double, std::milli>(b - a).count();
    };

    std::cout << size << ',' << ms(start, seeded) << ",1500," << ms(seeded, edited) << ','
              << ms(edited, looked) << std::endl;
  }
}
