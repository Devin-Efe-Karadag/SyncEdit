#include "rope.hpp"
#include <algorithm>
namespace ce {
uint64_t Rope::next() {
  seed ^= seed << 13;
  seed ^= seed >> 7;
  seed ^= seed << 17;

  return seed;
}
size_t Rope::len(const Ptr &p) { return p ? p->length : 0; }
size_t Rope::lines(const Ptr &p) { return p ? p->newlines : 0; }
void Rope::update(Ptr &p) {
  if (!p)
    return;
  p->length = len(p->left) + p->chunk.size() + len(p->right);
  p->newlines = lines(p->left) + lines(p->right);

  for (auto e : p->chunk)
    p->newlines += e.value == '\n';
  if (p->left)
    p->left->parent = p.get();
  if (p->right)
    p->right->parent = p.get();
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

  auto b = merge(node({k, p->chunk.end()}), std::move(p->right));

  return {std::move(a), std::move(b)};
}
Entry Rope::at(size_t i) const {
  if (i >= size())
    throw std::out_of_range("rope index");
  auto p = root.get();

  while (p) {
    size_t l = len(p->left);

    if (i < l)
      p = p->left.get();
    else if (i < l + p->chunk.size())
      return p->chunk[i - l];
    else {
      i -= l + p->chunk.size();
      p = p->right.get();
    }
  }
  throw std::logic_error("rope metadata");
}
bool Rope::append_chunk(Ptr &p, Entry e) {
  if (!p)
    return false;
  if (p->right) {
    bool added = append_chunk(p->right, e);

    if (added)
      update(p);
    return added;
  }

  if (p->chunk.size() >= 128)
    return false;
  locations[e.id] = {p.get(), p->chunk.size()};
  p->chunk.push_back(e);
  update(p);

  return true;
}
void Rope::insert(size_t i, Entry e) {
  if (i > size() || locations.contains(e.id))
    n -= take;
    i = l;
  }

  if (n && i < l + c) {
    size_t start = i - l, take = std::min(n, c - start);

    for (size_t j = 0; j < take; ++j)
      s += p->chunk[start + j].value;
    n -= take;
    i = l + c;
  }

  if (n)
    collect(p->right.get(), i - l - c, n, s);
}
std::string Rope::range(size_t i, size_t n) const {
  if (i > size() || n > size() - i)
    throw std::out_of_range("rope range");
  std::string s;
  s.reserve(n);
  collect(root.get(), i, n, s);

  return s;
}
size_t Rope::index(Id id) const {
  auto it = locations.find(id);

  if (it == locations.end())
    return size();
  auto [p, offset] = it->second;

  size_t rank = len(p->left) + offset;

  while (p->parent) {
    auto parent = p->parent;

    if (parent->right.get() == p)
      rank += len(parent->left) + parent->chunk.size();
    p = parent;
  }

  return rank;
}
size_t Rope::line_of(size_t i) const {
  if (i > size())
    throw std::out_of_range("line index");
  size_t result = 0;

  auto p = root.get();

  while (p) {
    size_t l = len(p->left);

    if (i < l) {
      p = p->left.get();
      continue;
    }
    result += lines(p->left);
    i -= l;

    size_t take = std::min(i, p->chunk.size());

    for (size_t j = 0; j < take; ++j)
      result += p->chunk[j].value == '\n';
    if (i <= p->chunk.size())
      break;
    i -= p->chunk.size();
    p = p->right.get();
  }

  return result;
}
size_t Rope::line_start(size_t line) const {
  if (!line)
    return 0;
  if (line > lines(root))
    return size();
  auto p = root.get();

  size_t offset = 0;

  while (p) {
    auto l = lines(p->left);

    if (line <= l) {
      p = p->left.get();
      continue;
    }
    line -= l;
    offset += len(p->left);

    for (auto e : p->chunk) {
      ++offset;

      if (e.value == '\n' && !--line)
        return offset;
    }
    p = p->right.get();
  }

  return size();
}
void Rope::assign(const std::vector<Entry> &entries) {
  root.reset();
  locations.clear();

  for (size_t i = 0; i < entries.size(); i += 128) {
    auto end = std::min(entries.size(), i + 128);
    root = merge(std::move(root), node({entries.begin() + static_cast<std::ptrdiff_t>(i),
                                        entries.begin() + static_cast<std::ptrdiff_t>(end)}));
  }
}
} // namespace ce
