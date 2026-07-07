#include "protocol.hpp"
#include <algorithm>
namespace ce::wire {
void Writer::number(uint64_t n, size_t b) {
  for (size_t i = b; i > 0; --i)
    data += static_cast<char>((n >> ((i - 1) * 8)) & 255);
}
void Writer::id(Id i) {
  number(i.replica, 8);
  number(i.counter, 8);
}
void Writer::str(const std::string &s) {
  if (s.size() > 255)
    throw std::runtime_error("string too long");
  number(s.size(), 2);
  data += s;
}
uint64_t Reader::number(size_t b) {
  if (b > 8 || b > data.size() - pos)
    throw std::runtime_error("truncated payload");
  uint64_t n = 0;

  while (b--)
    n = (n << 8) | static_cast<unsigned char>(data[pos++]);
  return n;
}
Id Reader::id() {
  auto a = number(8);

  return {a, number(8)};
}
std::string Reader::str() {
  auto n = number(2);

  if (n > 255 || n > data.size() - pos)
    throw std::runtime_error("bad string length");
  auto s = data.substr(pos, n);
  pos += n;

  return s;
}
void Reader::end() {
  if (pos != data.size())
    throw std::runtime_error("trailing payload");
}
std::string frame(Type t, const std::string &p) {
  if (p.size() > max_payload)
    throw std::runtime_error("oversized frame");
  Writer w;
  w.number(magic, 4);
  w.number(version, 2);
  w.number(static_cast<uint16_t>(t), 2);
  w.number(p.size(), 4);

  return w.data + p;
}
std::optional<Frame> take(std::string &b, bool operation_log) {
  if (b.size() < 12)
    return {};
  Reader r{b};

  auto tag = r.number(4), revision = r.number(2);

  if (tag != magic || (revision != version && !(operation_log && revision >= 1 && revision <= 3)))
    throw std::runtime_error("incompatible frame");
  auto t = r.number(2), n = r.number(4);

  if (operation_log && t != static_cast<uint16_t>(Type::OP_BATCH))
    throw std::runtime_error("invalid log message type");
  if (t < 1 || t > 9 || n > max_payload)
    throw std::runtime_error("invalid frame header");
  if (b.size() < 12 + n)
    return {};
  Frame f{static_cast<Type>(t), b.substr(12, n)};
  b.erase(0, 12 + n);

  return f;
}
std::string operations(const std::vector<Operation> &ops) {
  if (ops.size() > max_batch)
    throw std::runtime_error("batch too large");
  Writer w;
  w.number(ops.size(), 2);

  for (auto o : ops) {
    validate(o);
    w.number(o.insert ? 1 : 2, 1);
    w.id(o.id);
    w.id(o.ref);
    w.number(o.time, 8);
    w.number(static_cast<unsigned char>(o.value), 1);
  }

  return w.data;
}
std::vector<Operation> operations(const std::string &s) {
  Reader r{s};

  auto n = r.number(2);

  if (n > max_batch)
    throw std::runtime_error("batch too large");
  std::vector<Operation> v;

  for (size_t i = 0; i < n; ++i) {
    auto k = r.number(1);

    if (k != 1 && k != 2)
      throw std::runtime_error("bad operation kind");
    Operation o;
    o.insert = k == 1;
    o.id = r.id();
    o.ref = r.id();
    o.time = r.number(8);
    o.value = static_cast<char>(r.number(1));
    validate(o);
    v.push_back(o);
  }
  r.end();

  return v;
}
void vector(Writer &w, const Vector &v) {
  if (v.size() > max_replicas)
    throw std::runtime_error("too many replicas");
  w.number(v.size(), 2);

  for (auto [id, c] : v) {
    w.number(id, 8);
    w.number(c, 8);
  }
}
Vector vector(Reader &r) {
  auto n = r.number(2);

  if (n > max_replicas)
    throw std::runtime_error("too many replicas");
  Vector v;

  for (size_t i = 0; i < n; ++i) {
    auto id = r.number(8), c = r.number(8);

    if (!id || !v.emplace(id, c).second)
      throw std::runtime_error("bad vector");
  }

  return v;
}
std::string hello(const Hello &h) {
  Writer w;
  w.str(h.document);
  w.number(h.replica, 8);
  w.str(h.peer_name);
  w.number(h.listen_port, 2);
  vector(w, h.summary);

  return w.data;
}
Hello hello(const std::string &s) {
  Reader r{s};
  Hello h;
  h.document = r.str();
  h.replica = r.number(8);
  h.peer_name = r.str();
  h.listen_port = static_cast<uint16_t>(r.number(2));
  h.summary = vector(r);
  r.end();

  if (!h.replica || h.document.empty() || h.peer_name.empty() || h.peer_name.size() > 64 ||
      !h.listen_port)
    throw std::runtime_error("bad hello");
  return h;
}
std::string summary(const Vector &v) {
  Writer w;
  vector(w, v);

  return w.data;
}
Vector summary(const std::string &s) {
  Reader r{s};

  auto v = vector(r);
  r.end();

  return v;
}
std::string peers(const std::vector<std::string> &addresses) {
  if (addresses.size() > 64)
    throw std::runtime_error("too many peers");
  Writer w;
  w.number(addresses.size(), 1);

  for (const auto &address : addresses)
    w.str(address);
  return w.data;
}
std::vector<std::string> peers(const std::string &s) {
  Reader r{s};

  auto count = r.number(1);

  if (count > 64)
    throw std::runtime_error("too many peers");
  std::vector<std::string> addresses;
  addresses.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    auto address = r.str();

    if (address.empty() ||
        std::find(addresses.begin(), addresses.end(), address) != addresses.end())
      throw std::runtime_error("bad peer list");
    addresses.push_back(std::move(address));
  }
  r.end();

  return addresses;
}
} // namespace ce::wire
