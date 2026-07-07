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
  validate(o);

  auto it = history.find(o.id);

  if (it != history.end()) {
    if (it->second != o)
      throw std::runtime_error("conflicting operation ID");
    return;
  }

  if (history.size() >= 1000000)
    throw std::runtime_error("operation limit");
  auto parent = history.find(o.ref);

  if (parent != history.end() &&
      (!parent->second.insert || (o.insert && parent->second.time >= o.time)))
    throw std::runtime_error("invalid dependency");
  auto [begin, end] = waiting.equal_range(o.id);

  for (auto w = begin; w != end; ++w) {
    const auto &op = history.at(w->second);

    if (!o.insert || (op.insert && op.time <= o.time))
      throw std::runtime_error("inconsistent buffered dependency");
  }

  if (!versions.contains(o.id.replica) && versions.size() >= 1024)
    throw std::runtime_error("replica limit");
}
void Crdt::remember(const Operation &o) {
  history.emplace(o.id, o);
  clock = std::max(clock, o.time);

  auto &c = versions[o.id.replica];

  while (history.contains({o.id.replica, c + 1})) {
    ++c;
    ++contiguous;
  }
}
void Crdt::drain(Operation first) {
  std::vector<Operation> ready{first};

  while (!ready.empty()) {
    auto o = ready.back();
    ready.pop_back();

    if (o.insert) {
      nodes.emplace(o.id, Element{o, false, {}});

      auto &siblings = nodes.at(o.ref).children;

      auto it = siblings.emplace(o.time, o.id).first;

      auto following = std::next(it);
      OrderIndex::Key before = following == siblings.end()
                                   ? OrderIndex::Key{o.ref, true}
                                   : OrderIndex::Key{following->second, false};
      size_t pos = order.before(before);
      order.insert(o.id, before);
      rope.insert(pos, {o.id, o.value});
        ready.push_back(history.at(w->second));
      if (!n.deleted) {
        order.hide(o.ref);
        n.deleted = true;
      }
    }
    pending.erase(o.id);
  }
}
bool Crdt::receive(const Operation &o) {
  preflight(o);

  if (history.contains(o.id))
    return false;
  remember(o);
  pending.emplace(o.id, o);
