#pragma once
#include "order_index.hpp"
#include "rope.hpp"
#include <map>
#include <set>
namespace ce {
struct Operation {
  Id id, ref;
  uint64_t time = 0;

  char value = 0;

  bool insert = true;

  auto operator<=>(const Operation &) const = default;
};
using Vector = std::map<uint64_t, uint64_t>;
class Crdt {
  struct Element {
    Operation op;

    bool deleted = false;

    std::set<std::pair<uint64_t, Id>, std::greater<>> children;
  };

  std::map<Id, Element> nodes;

  std::map<Id, Operation> pending;

  std::map<Id, Operation> history;

  std::multimap<Id, Id> waiting;
  Vector versions;

  size_t contiguous = 0;
  OrderIndex order;
  void drain(Operation first);
  void remember(const Operation &o);

public:
  Rope rope;
  uint64_t clock = 0;
  Crdt();
  void preflight(const Operation &o) const;

  bool receive(const Operation &o);

  bool contains(Id id) const { return history.contains(id); }

  const auto &operations() const { return history; }
  Vector summary() const { return versions; }
  // Cursor has left affinity: stay immediately after the same predecessor,
  // including its logical location if that predecessor is deleted.
  Id anchor(size_t cursor) const { return cursor ? rope.at(cursor - 1).id : Id{}; }

  size_t resolve(Id anchor) const { return order.after(anchor); }
  void restore(const std::vector<Operation> &operations);

  bool settled() const;

  size_t buffered() const { return pending.size(); }

  std::string text() const { return rope.range(0, rope.size()); }
};
void validate(const Operation &o);
} // namespace ce
