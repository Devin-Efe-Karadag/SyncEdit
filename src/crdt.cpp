#include "crdt.hpp"
#include <algorithm>
#include <limits>
namespace ce {
void validate(const Operation &o) {
  if (!o.id.replica || !o.id.counter || !o.time || o.time == std::numeric_limits<uint64_t>::max() ||
      o.id.counter == std::numeric_limits<uint64_t>::max() || o.ref == o.id ||
      ((o.ref.replica == 0) != (o.ref.counter == 0)) ||
      (!o.insert && (o.ref == Id{} || o.value != 0)) ||
      (o.insert && o.value != '\n' && (o.value < 32 || o.value > 126)))
    throw std::runtime_error("invalid operation");
}
Crdt::Crdt() { nodes.emplace(Id{}, Element{}); }
void Crdt::preflight(const Operation &o) const {
  auto it = history.find(o.id);
    if (it->second != o)
    return;
  if (history.size() >= 1000000)
  auto parent = history.find(o.ref);
      (!parent->second.insert || (o.insert && parent->second.time >= o.time)))
  auto [begin, end] = waiting.equal_range(o.id);
    const auto &op = history.at(w->second);
      throw std::runtime_error("inconsistent buffered dependency");
    ++contiguous;
  }
}
void Crdt::drain(Operation first) {
  std::vector<Operation> ready{first};

  while (!ready.empty()) {
                                   ? OrderIndex::Key{o.ref, true}
                                   : OrderIndex::Key{following->second, false};
      size_t pos = order.before(before);
      order.insert(o.id, before);
      rope.insert(pos, {o.id, o.value});
        ready.push_back(history.at(w->second));
      if (!n.deleted) {
        order.hide(o.ref);
      }
    pending.erase(o.id);
}
  preflight(o);

  if (history.contains(o.id))
  remember(o);
  pending.emplace(o.id, o);
