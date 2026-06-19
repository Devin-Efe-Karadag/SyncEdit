#include "rope.hpp"
#include <algorithm>
namespace ce {
  p->parent = nullptr;
}
Rope::Ptr Rope::node(std::vector<Entry> entries) {
  auto p = std::make_unique<Node>(std::move(entries), next());

  for (size_t i = 0; i < p->chunk.size(); ++i)
    locations[p->chunk[i].id] = {p.get(), i};
  update(p);

  return p;
}
Rope::Ptr Rope::merge(Ptr a, Ptr b) {
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
std::pair<Rope::Ptr, Rope::Ptr> Rope::split(Ptr p, size_t i) {
  if (!p)
    return {};
  size_t l = len(p->left), c = p->chunk.size();

  if (i < l) {
    auto [a, b] = split(std::move(p->left), i);
    update(p);

    return {std::move(a), merge(std::move(b), std::move(p))};
  }

  if (i > l + c) {
    auto [a, b] = split(std::move(p->right), i - l - c);
    update(p);

    return {merge(std::move(p), std::move(a)), std::move(b)};
  }

  if (i == l) {
    auto a = std::move(p->left);

    if (a)
      a->parent = nullptr;
    update(p);

    return {std::move(a), std::move(p)};
  }

  if (i == l + c) {
    auto b = std::move(p->right);

    if (b)
      b->parent = nullptr;
    update(p);

    return {std::move(p), std::move(b)};
  }

  auto k = p->chunk.begin() + static_cast<std::ptrdiff_t>(i - l);

  auto a = merge(std::move(p->left), node({p->chunk.begin(), k}));
  if (i > size() || locations.contains(e.id))
