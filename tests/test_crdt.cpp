#include "crdt.hpp"
#include <algorithm>
#include <iostream>
#include <random>
int main() {
  using namespace ce;
  Operation a{{1, 1}, {}, 1, 'a', true}, b{{2, 1}, {}, 1, 'b', true}, c{{1, 2}, a.id, 2, 'c', true},
      d{{3, 1}, a.id, 3, 0, false};
  Crdt x;
  x.receive(d);
  x.receive(c);
  check(x.buffered() == 2, "dependencies buffered");
  x.receive(b);
  x.receive(a);
  check(x.text() == "bc" && x.buffered() == 0, "tombstone descendants");
  check(!x.receive(a), "dedup");
  Crdt order;
  order.receive(a);
  order.receive(b);
  check(order.text() == "ba", "concurrent root tie ordered by immutable ID");
  order.receive(c);
  order.receive({{2, 2}, b.id, 2, 'd', true});
  check(order.text() == "bdac", "inserts after distinct predecessors");
  order.receive({{3, 2}, {}, 4, 'z', true});
  check(order.text() == "zbdac", "causally later root insertion before existing text");

  std::mt19937 rng(22);

  std::vector<Operation> ops{a, b, c, d};

  for (uint64_t replica = 4; replica < 9; ++replica) {
    Id parent = a.id;

    for (uint64_t n = 1; n < 80; ++n) {
      Operation o{{replica, n}, parent, n + 4, static_cast<char>('a' + rng() % 26), true};
      ops.push_back(o);
      parent = o.id;

      if (n % 7 == 0) {
        ++n;
        ops.push_back({{replica, n}, parent, n + 4, 0, false});
      }
    }
  }

  std::string expected;

  for (int trial = 0; trial < 100; ++trial) {
    std::shuffle(ops.begin(), ops.end(), rng);
    Crdt r;

    for (auto o : ops) {
      r.receive(o);
      r.receive(o);
    }
    check(r.buffered() == 0, "all dependencies resolved");
    Crdt loaded;
    loaded.restore(ops);
    check(loaded.text() == r.text() && loaded.summary() == r.summary(),
          "bulk checkpoint equivalence");
    for (size_t i = 0; i < r.rope.size(); ++i) {
      check(r.rope.index(r.rope.at(i).id) == i, "ID rank after reordered edits");
      check(r.resolve(r.anchor(i)) == i, "cursor rank");
    }

    if (!trial)
      expected = r.text();
    check(r.text() == expected, "shuffled convergence");
  }
  Crdt cursor;
  cursor.receive(a);
  cursor.receive(c); // ac

  auto anchor = cursor.anchor(1);
  cursor.receive({{7, 1}, {}, 10, 'z', true}); // zac
  check(cursor.resolve(anchor) == 2, "cursor follows anchor after earlier remote insert");
  cursor.receive({{7, 2}, a.id, 11, 'x', true}); // zaxc
  check(cursor.resolve(anchor) == 2, "left affinity for same-gap insert");
  cursor.receive(d); // zxc; deleted a remains the anchor
  check(cursor.resolve(anchor) == 1, "tombstone anchor fallback");
  cursor.receive({{7, 3}, {7, 1}, 12, 0, false});
  check(cursor.resolve(anchor) == 0, "earlier anchor-neighbor deletion");
  Crdt restored;

  std::vector<Operation> state;
