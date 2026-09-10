#include "order_index.hpp"
namespace ce {
size_t OrderIndex::count(const Ptr &p) { return p ? p->count : 0; }
size_t OrderIndex::weight(const Ptr &p) { return p ? p->weight : 0; }
void OrderIndex::update(Ptr &p) {
  p->count = 1 + count(p->left) + count(p->right);
  p->weight = p->marker.visible + weight(p->left) + weight(p->right);

  if (p->left)
    p->left->parent = p.get();
  if (p->right)
    p->right->parent = p.get();
  p->parent = nullptr;
}
OrderIndex::Ptr OrderIndex::merge(Ptr a, Ptr b) {
  if (!a) {
    if (b)
      b->parent = nullptr;
    return b;
  }

  if (!b) {
    a->parent = nullptr;

    return a;
  }

  if (a->priority > b->priority) {
    a->right = merge(std::move(a->right), std::move(b));
    update(a);

    return a;
  }
  b->left = merge(std::move(a), std::move(b->left));
  update(b);

  return b;
}
std::pair<OrderIndex::Ptr, OrderIndex::Ptr> OrderIndex::split(Ptr p, size_t i) {
  if (!p)
    return {};
  if (i <= count(p->left)) {
    auto [a, b] = split(std::move(p->left), i);
    p->left = std::move(b);
    update(p);

    return {std::move(a), std::move(p)};
  }

  auto [a, b] = split(std::move(p->right), i - count(p->left) - 1);
  p->right = std::move(a);
  update(p);

  return {std::move(p), std::move(b)};
}
OrderIndex::Ptr OrderIndex::node(Marker m) {
  seed ^= seed << 13;
  seed ^= seed >> 7;
  seed ^= seed << 17;

  auto p = std::make_unique<Node>(m, seed);

  if (!locations.emplace(m.key, p.get()).second)
    throw std::runtime_error("duplicate order marker");
  return p;
}
OrderIndex::OrderIndex() { assign({{{Id{}, false}, false}, {{Id{}, true}, false}}); }
size_t OrderIndex::rank(Key key) const {
  auto p = locations.at(key);

  size_t n = count(p->left);

  while (p->parent) {
    auto up = p->parent;

    if (up->right.get() == p)
      n += count(up->left) + 1;
    p = up;
  }

  return n;
}
size_t OrderIndex::before(Key key) const {
  auto p = locations.at(key);

  size_t n = weight(p->left);

  while (p->parent) {
    auto up = p->parent;

    if (up->right.get() == p)
      n += weight(up->left) + up->marker.visible;
    p = up;
  }

  return n;
}
size_t OrderIndex::after(Id id) const {
  return before({id, false}) + locations.at({id, false})->marker.visible;
}
void OrderIndex::insert(Id id, Key beforeKey) {
  auto [a, b] = split(std::move(root), rank(beforeKey));
  a = merge(std::move(a), node({{id, false}, true}));
  a = merge(std::move(a), node({{id, true}, false}));
  root = merge(std::move(a), std::move(b));
}
void OrderIndex::hide(Id id) {
  auto p = locations.at({id, false});

  if (!p->marker.visible)
    return;
  p->marker.visible = false;

  for (; p; p = p->parent)
    --p->weight;
}
void OrderIndex::assign(const std::vector<Marker> &markers) {
  root.reset();
  locations.clear();

  for (auto m : markers)
    root = merge(std::move(root), node(m));
}
} // namespace ce
