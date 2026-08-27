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
}
int main() {
  using namespace ce::wire;

  auto bytes = frame(Type::PING, "");

  for (size_t n = 0; n < bytes.size(); ++n) {
    auto s = bytes.substr(0, n);
    ce::check(!take(s), "partial frame");
  }

  auto s = bytes;
  ce::check(take(s)->type == Type::PING && s.empty(), "complete frame");
  s = bytes;
  s[0] = 0;
  rejects([&] { take(s); });
  s = bytes;
  s[4] = 1;
  rejects([&] { take(s); });
  s = bytes;
  s[8] = 127;
  rejects([&] { take(s); });
  s = bytes;
  s[7] = 99;
  rejects([&] { take(s); });
  rejects([&] { frame(Type::PING, std::string(max_payload + 1, 'x')); });
  ce::Operation op{{1, 1}, {}, 1, 'a', true};
