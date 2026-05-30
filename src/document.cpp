}
void Document::erase(size_t p) {
  local({{store.replica, store.next}, crdt.rope.at(p).id, crdt.clock + 1, 0, false});
}
} // namespace ce
