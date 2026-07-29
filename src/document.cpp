#include "document.hpp"
namespace ce {
bool Document::receive(const Operation &o) {
  crdt.preflight(o);

  if (crdt.contains(o.id)) {
    crdt.receive(o);

    return false;
  }
  try {
    store.append(o);
  } catch (const std::exception &e) {
    throw StorageError(e.what());
  }
  crdt.receive(o);

  return true;
}
void Document::local(Operation o) {
  receive(o);
  store.counter(o.id.counter + 1);

  if (broadcast)
    broadcast(o);
}
void Document::insert(size_t p, char c) {
  if (p > crdt.rope.size())
    throw std::out_of_range("cursor");
  local({{store.replica, store.next}, p ? crdt.rope.at(p - 1).id : Id{}, crdt.clock + 1, c, true});
}
void Document::erase(size_t p) {
  local({{store.replica, store.next}, crdt.rope.at(p).id, crdt.clock + 1, 0, false});
}
} // namespace ce
