#pragma once
#include "util.hpp"
#include <map>
#include <memory>
#include <vector>
namespace ce {
// Euler-tour markers give each CRDT subtree a contiguous interval. Weighted
// rank counts visible opening markers, including when the anchor is tombstoned.
class OrderIndex {
public:
  using Key = std::pair<Id, bool>; // false = opening, true = closing

  struct Marker {
    Key key;

    bool visible;
  };

private:
  struct Node {
    Marker marker;

    size_t count = 1, weight = 0;
    uint64_t priority;
    Node *parent = nullptr;

    std::unique_ptr<Node> left, right;
    Node(Marker m, uint64_t p) : marker(m), weight(m.visible), priority(p) {}
  };
  using Ptr = std::unique_ptr<Node>;
  Ptr root;

  std::map<Key, Node *> locations;
  uint64_t seed = 998123;

  static size_t count(const Ptr &p);

  static size_t weight(const Ptr &p);

  static void update(Ptr &p);

  static Ptr merge(Ptr a, Ptr b);

  static std::pair<Ptr, Ptr> split(Ptr p, size_t rank);
  Ptr node(Marker m);

  size_t rank(Key key) const;

public:
  OrderIndex();
  void insert(Id id, Key before);
  void hide(Id id);

  size_t before(Key key) const;

  size_t after(Id id) const;
  void assign(const std::vector<Marker> &markers);
};
} // namespace ce
