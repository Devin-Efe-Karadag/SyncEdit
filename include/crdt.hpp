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
