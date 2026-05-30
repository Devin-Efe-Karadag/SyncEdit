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
    ++contiguous;
  }
}
void Crdt::drain(Operation first) {
  std::vector<Operation> ready{first};

  while (!ready.empty()) {
                                   ? OrderIndex::Key{o.ref, true}
        ready.push_back(history.at(w->second));
      if (!n.deleted) {
