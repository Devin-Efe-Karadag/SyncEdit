#pragma once
#include "util.hpp"
#include <map>
#include <memory>
#include <vector>
namespace ce {
// Chunked implicit treap. Stable node handles make ID -> rank logarithmic.
class Rope {
  struct Node {
    std::vector<Entry> chunk;

    std::unique_ptr<Node> left, right;
    Node *parent = nullptr;

    size_t length = 0, newlines = 0;
    uint64_t priority;
    Node(std::vector<Entry> c, uint64_t p) : chunk(std::move(c)), priority(p) {}
  };
  using Ptr = std::unique_ptr<Node>;
  Ptr root;

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

  static void collect(const Node *p, size_t i, size_t n, std::string &s);

public:
  size_t size() const { return len(root); }
  Entry at(size_t i) const;
  void insert(size_t i, Entry e);
  void erase(size_t i);

  std::string range(size_t i, size_t n) const;

  size_t index(Id id) const;

  size_t line_of(size_t index) const;

  size_t line_start(size_t line) const;
  void assign(const std::vector<Entry> &entries);
};
} // namespace ce
