#include "protocol.hpp"
#include <iostream>
template <class F> void rejects(F f) {
  bool threw = false;
  try {
    f();
  } catch (const std::exception &) {
    threw = true;
  }
  ce::check(threw, "expected rejection");
