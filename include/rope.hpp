#pragma once
#include "util.hpp"
#include <map>
#include <memory>
#include <vector>
namespace ce {
// Chunked implicit treap. Stable node handles make ID -> rank logarithmic.
class Rope {
  };
  using Ptr = std::unique_ptr<Node>;
  std::map<Id, std::pair<Node *, size_t>> locations;
  uint64_t seed = 1234567;
  uint64_t next();

  static size_t len(const Ptr &p);

  static size_t lines(const Ptr &p);

  static void update(Ptr &p);
  Ptr node(std::vector<Entry> entries);

  bool append_chunk(Ptr &p, Entry e);

  static Ptr merge(Ptr a, Ptr b);

  std::pair<Ptr, Ptr> split(Ptr p, size_t i);
