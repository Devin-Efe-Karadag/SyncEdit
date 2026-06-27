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
