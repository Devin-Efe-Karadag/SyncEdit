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
  void preflight(const Operation &o) const;
  bool contains(Id id) const { return history.contains(id); }
  Vector summary() const { return versions; }
  Id anchor(size_t cursor) const { return cursor ? rope.at(cursor - 1).id : Id{}; }
  void restore(const std::vector<Operation> &operations);
  size_t buffered() const { return pending.size(); }
};
} // namespace ce
