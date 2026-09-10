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

      auto [begin, end] = waiting.equal_range(o.id);

      for (auto w = begin; w != end; ++w)
        ready.push_back(history.at(w->second));
      waiting.erase(begin, end);
    } else {
      auto &n = nodes.at(o.ref);

      if (!n.deleted) {
        rope.erase(order.before({o.ref, false}));
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

  if (nodes.contains(o.ref))
    drain(o);
  else
    waiting.emplace(o.ref, o.id);
  return true;
}
bool Crdt::settled() const { return pending.empty() && contiguous == history.size(); }
void Crdt::restore(const std::vector<Operation> &ops) {
  // Bulk construction avoids replaying all past rope mutations. Historical
  // operations remain available for deduplication and anti-entropy.
  Crdt fresh;

  if (ops.size() > 1000000)
    throw std::runtime_error("checkpoint operation limit");
  for (auto o : ops) {
    validate(o);

    if (!fresh.history.emplace(o.id, o).second)
      throw std::runtime_error("duplicate checkpoint operation");
    fresh.clock = std::max(fresh.clock, o.time);
  }

  for (const auto &[id, o] : fresh.history) {
    auto &c = fresh.versions[id.replica];

    if (id.counter == c + 1) {
      ++c;
      ++fresh.contiguous;
    }

    auto p = fresh.history.find(o.ref);

    if (p != fresh.history.end() && (!p->second.insert || (o.insert && p->second.time >= o.time)))
      throw std::runtime_error("invalid checkpoint dependency");
  }

  if (fresh.versions.size() > 1024)
    throw std::runtime_error("checkpoint replica limit");
  std::vector<Operation> sorted = ops;

  std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
    return std::tie(a.time, a.id) < std::tie(b.time, b.id);
  });

  for (auto o : sorted)
    if (o.insert) {
      if (fresh.nodes.contains(o.ref)) {
        fresh.nodes.emplace(o.id, Element{o, false, {}});
        fresh.nodes.at(o.ref).children.emplace(o.time, o.id);
      } else {
        fresh.pending.emplace(o.id, o);
        fresh.waiting.emplace(o.ref, o.id);
      }
    }
  for (auto o : sorted)
    if (!o.insert) {
      if (fresh.nodes.contains(o.ref))
        fresh.nodes.at(o.ref).deleted = true;
      else {
        fresh.pending.emplace(o.id, o);
        fresh.waiting.emplace(o.ref, o.id);
      }
    }
  std::vector<OrderIndex::Marker> markers;

  std::vector<Entry> visible;

  std::vector<OrderIndex::Key> stack{{Id{}, false}};

  while (!stack.empty()) {
    auto key = stack.back();
    stack.pop_back();

    const auto &n = fresh.nodes.at(key.first);

    bool show = !key.second && key.first != Id{} && !n.deleted;
    markers.push_back({key, show});

    if (show)
      visible.push_back({key.first, n.op.value});
    if (!key.second) {
      stack.push_back({key.first, true});

      for (auto it = n.children.rbegin(); it != n.children.rend(); ++it)
        stack.push_back({it->second, false});
    }
  }
  fresh.order.assign(markers);
  fresh.rope.assign(visible);
  *this = std::move(fresh);
}
} // namespace ce
