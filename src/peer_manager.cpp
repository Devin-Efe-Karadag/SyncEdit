#include "peer_manager.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <ifaddrs.h>
#include <limits>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
namespace ce {
static sockaddr_in endpoint(const std::string &s) {
  auto p = s.rfind(':');

  if (p == std::string::npos)
    throw std::runtime_error("expected IPv4:PORT");
  size_t used = 0;

  int port = std::stoi(s.substr(p + 1), &used);

  if (used != s.size() - p - 1 || port < 1 || port > 65535)
    throw std::runtime_error("invalid port");
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(static_cast<uint16_t>(port));

  if (inet_pton(AF_INET, s.substr(0, p).c_str(), &a.sin_addr) != 1)
    throw std::runtime_error("use numeric IPv4 address");
  return a;
}
static std::string remote_host(int fd) {
  sockaddr_in address{};
  socklen_t length = sizeof address;
  check(getpeername(fd, reinterpret_cast<sockaddr *>(&address), &length) == 0, "peer address");

  char text[INET_ADDRSTRLEN]{};

  if (!inet_ntop(AF_INET, &address.sin_addr, text, sizeof text))
    throw std::runtime_error("peer address");
  return text;
}
PeerManager::PeerManager(Document &d, const std::string &n, const std::string &l,
                         const std::vector<std::string> &peers,

                         std::function<void(const std::vector<std::string> &)> save)
    : doc(d), name(n), save_peers(std::move(save)) {
  try {
    ep = epoll_create1(EPOLL_CLOEXEC);
    check(ep >= 0, "epoll_create");
    listener = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    check(listener >= 0, "socket");

    int yes = 1;
    check(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == 0, "reuseaddr");

    auto a = endpoint(l.empty() ? "0.0.0.0:9000" : l);

    bool bound = false;

    for (int port = 9000; port <= 9099; ++port) {
      if (l.empty())
        a.sin_port = htons(static_cast<uint16_t>(port));
      if (bind(listener, reinterpret_cast<sockaddr *>(&a), sizeof a) == 0) {
        bound = true;
        break;
      }

      if (!l.empty() || errno != EADDRINUSE)
        break;
    }
    check(bound, "bind (automatic ports 9000-9099 exhausted)");
    listen_port = ntohs(a.sin_port);
    listening = l.empty() ? "0.0.0.0:" + std::to_string(ntohs(a.sin_port)) : l;
    check(listen(listener, 64) == 0, "listen");
    epoll_event e{};
    e.events = EPOLLIN;
    e.data.fd = listener;
    check(epoll_ctl(ep, EPOLL_CTL_ADD, listener, &e) == 0, "epoll listener");

    for (const auto &p : peers)
      remember(p);
    doc.broadcast = [this](const Operation &o) { broadcast(o, -1); };
  } catch (...) {
    if (listener >= 0)
      close(listener);
    if (ep >= 0)
      close(ep);
    throw;
  }
}
PeerManager::~PeerManager() {
  doc.broadcast = {};

  for (auto &[fd, c] : connections) {
    (void)c;
    close(fd);
  }

  if (listener >= 0)
    close(listener);
  if (ep >= 0)
    close(ep);
}
void PeerManager::watch(Connection &c) {
  epoll_event e{};
  e.events = EPOLLIN | EPOLLRDHUP;

  if (c.connecting || !c.output.empty())
    e.events |= EPOLLOUT;
  e.data.fd = c.fd;
  check(epoll_ctl(ep, EPOLL_CTL_MOD, c.fd, &e) == 0, "epoll modify");
}
void PeerManager::queue(Connection &c, wire::Type t, const std::string &s) {
  auto b = wire::frame(t, s);

  if (c.output.size() + b.size() > 1024 * 1024) {
    c.dead = true;
    last_error = "slow peer disconnected";
    return;
  }
  c.output += b;
  watch(c);
}
void PeerManager::add(int fd, bool out, bool connecting, const std::string &address) {
  Connection c;
  c.fd = fd;
  c.outbound = out;
  c.connecting = connecting;
  c.address = address;
  epoll_event e{};
  e.events = EPOLLIN | EPOLLRDHUP;
  e.data.fd = fd;

  if (epoll_ctl(ep, EPOLL_CTL_ADD, fd, &e) < 0) {
    close(fd);
    throw std::runtime_error("epoll add");
  }
  connections.emplace(fd, std::move(c));

  auto &v = connections.at(fd);
  queue(v, wire::Type::HELLO,
        wire::hello(
            {name, doc.store.replica, doc.store.display_name, listen_port, doc.crdt.summary()}));
}
bool PeerManager::remember(const std::string &address) {
  auto parsed = endpoint(address);

  auto host = ntohl(parsed.sin_addr.s_addr);

  if (!host || host >= 0xe0000000U)
    throw std::runtime_error("invalid peer address");
  if (ntohs(parsed.sin_port) == listen_port) {
    ifaddrs *interfaces = nullptr;
    check(getifaddrs(&interfaces) == 0, "local interfaces");

    bool self = false;

    for (auto *it = interfaces; it; it = it->ifa_next)
      if (it->ifa_addr && it->ifa_addr->sa_family == AF_INET &&
          reinterpret_cast<sockaddr_in *>(it->ifa_addr)->sin_addr.s_addr == parsed.sin_addr.s_addr)
        self = true;
    freeifaddrs(interfaces);

    if (self)
      return false;
  }

  for (const auto &target : targets)
    if (target.address == address)
      return false;
  if (targets.size() >= 64)
    return false;
  targets.push_back({address, Clock::now(), 1, 0});

  if (save_peers) {
    std::vector<std::string> addresses;

    for (const auto &target : targets)
      addresses.push_back(target.address);
    save_peers(addresses);
  }

  return true;
}
void PeerManager::announce() {
  std::vector<std::string> addresses;
  // Introduce only peers whose document handshake has succeeded.

  for (const auto &[fd, connection] : connections) {
    (void)fd;

    if (addresses.size() < 64 && connection.ready && !connection.dead &&
        !connection.remote_endpoint.empty() &&

        std::find(addresses.begin(), addresses.end(), connection.remote_endpoint) ==
            addresses.end())
      addresses.push_back(connection.remote_endpoint);
  }

  for (auto &[fd, connection] : connections) {
    (void)fd;

    if (connection.ready && !connection.dead) {
      auto visible = addresses;

      if (!connection.remote_host.starts_with("127."))
        std::erase_if(visible,
                      [](const std::string &address) { return address.starts_with("127."); });
      visible.erase(std::remove(visible.begin(), visible.end(), connection.remote_endpoint),
                    visible.end());
      queue(connection, wire::Type::PEER_LIST, wire::peers(visible));
    }
  }
}
void PeerManager::broadcast(const Operation &o, int except) {
  for (auto &[fd, c] : connections)
    if (fd != except && c.ready && !c.dead) {
      c.synced = false;
      queue(c, wire::Type::OP_BATCH, wire::operations(std::vector<Operation>{o}));
    }
}
void PeerManager::missing(Connection &c, const Vector &v) {
  std::vector<Operation> batch;

  const auto &history = doc.crdt.operations();

  for (const auto &[replica, local] : doc.crdt.summary()) {
    (void)local;

    auto known = v.find(replica);
    uint64_t counter = known == v.end() ? 0 : known->second;

    if (counter == std::numeric_limits<uint64_t>::max())
      continue;
    auto it = history.lower_bound({replica, counter + 1});

    for (; it != history.end() && it->first.replica == replica && batch.size() < wire::max_batch;
         ++it)
      batch.push_back(it->second);
    if (batch.size() == wire::max_batch)
      break;
  }
  c.synced = batch.empty() && v == doc.crdt.summary() && doc.crdt.settled();

  if (!batch.empty())
    queue(c, wire::Type::OP_BATCH, wire::operations(batch));
  queue(c, wire::Type::SYNC_RESPONSE, wire::summary(doc.crdt.summary()));
}
void PeerManager::message(Connection &c, const wire::Frame &f) {
  using wire::Type;

  if (f.type == Type::HELLO || f.type == Type::HELLO_ACK) {
    auto h = wire::hello(f.payload);

    if (h.document != name || h.replica == doc.store.replica)
      throw std::runtime_error("document/replica mismatch");
    if (c.ready && c.replica != h.replica)
      throw std::runtime_error("identity changed");
    c.replica = h.replica;
    c.peer_name = h.peer_name;
    c.remote_host = remote_host(c.fd);
    c.remote_endpoint = c.remote_host + ":" + std::to_string(h.listen_port);
    (void)remember(c.remote_endpoint);

    for (auto &t : targets)
      if (t.address == c.address || t.address == c.remote_endpoint) {
        t.replica = h.replica;
        t.backoff = 1;
      }
    for (auto &[fd, other] : connections) {
      if (fd == c.fd || other.dead || !other.ready || other.replica != h.replica)
        continue;
      bool prefer = doc.store.replica < h.replica;

      if (c.outbound == prefer && other.outbound != prefer)
        other.dead = true;
      else {
        c.dead = true;
        return;
      }
    }
    c.ready = true;

    if (f.type == Type::HELLO)
      queue(c, Type::HELLO_ACK,
            wire::hello({name, doc.store.replica, doc.store.display_name, listen_port,
                         doc.crdt.summary()}));
    missing(c, h.summary);
    announce();
    return;
  }

  if (!c.ready)
    throw std::runtime_error("handshake required");
  switch (f.type) {
  case Type::OP_BATCH:
    for (auto o : wire::operations(f.payload))
      if (doc.receive(o)) {
        c.synced = false;
        broadcast(o, c.fd);
      }
    queue(c, Type::SYNC_REQUEST, wire::summary(doc.crdt.summary()));
    break;
  case Type::SYNC_REQUEST:
    missing(c, wire::summary(f.payload));
    break;
  case Type::SYNC_RESPONSE: {
    auto v = wire::summary(f.payload);
    c.synced = v == doc.crdt.summary() && doc.crdt.settled();
    break;
  }
  case Type::PING:
    if (!f.payload.empty())
      throw std::runtime_error("bad ping");
    queue(c, Type::PONG, "");
    break;
  case Type::PONG:
    if (!f.payload.empty())
      throw std::runtime_error("bad pong");
    break;
  case Type::PEER_LIST: {
    auto addresses = wire::peers(f.payload);

    for (const auto &address : addresses)
      endpoint(address);
    for (const auto &address : addresses) {
      if (address.starts_with("127.") && !c.remote_host.starts_with("127."))
        continue;
      remember(address);
    }
    break;
  }
  case Type::ERROR:
    throw std::runtime_error("remote protocol error");
  default:
    throw std::runtime_error("unexpected message");
  }
}
void PeerManager::poll(int timeout) {
  auto now = Clock::now();

  for (auto &t : targets) {
    if (active(t) || now < t.retry || connections.size() >= 64)
      continue;
    t.retry = now + std::chrono::seconds(t.backoff);
    t.backoff = std::min(30, t.backoff * 2);

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    check(fd >= 0, "client socket");

    auto a = endpoint(t.address);

    int r = connect(fd, reinterpret_cast<sockaddr *>(&a), sizeof a);

    if (r < 0 && errno != EINPROGRESS) {
      check(close(fd) == 0, "close failed connection");
      continue;
    }
    add(fd, true, r < 0, t.address);
  }
  epoll_event events[64];

  int n = epoll_wait(ep, events, 64, timeout);

  if (n < 0 && errno == EINTR)
    return;
  check(n >= 0, "epoll_wait");

  for (int i = 0; i < n; ++i) {
    int fd = events[i].data.fd;

    if (fd == listener) {
      for (int j = 0; j < 64; ++j) {
        int client = accept4(listener, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);

        if (client < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;
          if (errno == EINTR)
            continue;
          check(false, "accept");
        }

        if (connections.size() >= 64) {
          check(close(client) == 0, "connection limit close");
          continue;
        }
        add(client, false, false, "incoming");
      }
      continue;
    }

    auto it = connections.find(fd);

    if (it == connections.end())
      continue;
    auto &c = it->second;

    if (c.dead)
      continue;
    try {
      if (c.connecting && (events[i].events & EPOLLOUT)) {
        int err = 0;
        socklen_t len = sizeof err;
        check(getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0, "connect status");

        if (err)
          throw std::runtime_error("connect failed");
        c.connecting = false;
      }

      if (events[i].events & EPOLLIN) {
        for (int j = 0; j < 16; ++j) {
          char b[8192];

          auto r = recv(fd, b, sizeof b, 0);

          if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
              break;
            if (errno == EINTR)
              continue;
            throw std::runtime_error("receive failed");
          }

          if (!r) {
            c.dead = true;
            break;
          }
          c.seen = now;
          c.input.append(b, static_cast<size_t>(r));

          if (c.input.size() > wire::max_payload + 12 + sizeof b)
            throw std::runtime_error("input limit");
          while (auto f = wire::take(c.input)) {
            message(c, *f);

            if (c.dead)
              break;
          }

          if (c.dead)
            break;
        }
      }

      if (!c.connecting && !c.dead && (events[i].events & EPOLLOUT) && !c.output.empty()) {
        auto r = send(fd, c.output.data(), c.output.size(), MSG_NOSIGNAL);

        if (r > 0)
          c.output.erase(0, static_cast<size_t>(r));
        else if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
          c.dead = true;
      }

      if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
        c.dead = true;
      watch(c);
    } catch (const StorageError &) {
      throw; // A failed durable write must stop the editor, never be
             // acknowledged.
    } catch (const std::exception &e) {
      last_error = e.what(); // Best-effort ERROR, then close: errors are never
                             // retransmitted.
      auto error = wire::frame(wire::Type::ERROR, "invalid peer message");

      auto sent = send(fd, error.data(), error.size(), MSG_NOSIGNAL);
      (void)sent;
      c.dead = true;
    }
  }

  if (now - tick >= std::chrono::seconds(1)) {
    tick = now;

    for (auto &[fd, c] : connections) {
      (void)fd;

      if (now - c.seen > std::chrono::seconds(15)) {
        c.dead = true;
        continue;
      }

      if (c.ready && !c.dead) {
        queue(c, wire::Type::SYNC_REQUEST, wire::summary(doc.crdt.summary()));
        queue(c, wire::Type::PING, "");
      }
    }
  }

  for (auto it = connections.begin(); it != connections.end();) {
    if (it->second.dead) {
      check(epoll_ctl(ep, EPOLL_CTL_DEL, it->first, nullptr) == 0, "epoll delete");
      check(close(it->first) == 0, "close peer");
      it = connections.erase(it);
    } else
      ++it;
  }
}
size_t PeerManager::count() const {
  return std::count_if(connections.begin(), connections.end(),
                       [](const auto &entry) { return entry.second.ready && !entry.second.dead; });
}
bool PeerManager::synced() const {
  return count() && std::none_of(connections.begin(), connections.end(), [](const auto &entry) {
           const auto &c = entry.second;

           return c.ready && !c.dead && !c.synced;
         });
}
bool PeerManager::active(const Target &target) const {
  return std::any_of(connections.begin(), connections.end(), [&](const auto &entry) {
    const auto &c = entry.second;

    return !c.dead &&
           (c.address == target.address || (target.replica && c.replica == target.replica));
  });
}
std::vector<std::string> PeerManager::status() const {
  std::vector<std::string> s;
  s.push_back("Listening: " + listening);
  s.push_back("Storage: " + storage_path);

  if (listener >= 0)
    s.push_back("Invite: use --join <this machine's IPv4>:" +
                listening.substr(listening.rfind(':') + 1));
  for (const auto &[fd, c] : connections) {
    (void)fd;

    if (!c.dead)
      s.push_back((c.peer_name.empty() ? "Peer" : c.peer_name) + "  |  " +
                  (c.ready ? (c.synced ? "Synced" : "Syncing") : "Connecting") + "  |  " +
                  (c.remote_endpoint.empty() ? c.address : c.remote_endpoint));
  }

  for (const auto &t : targets)
    if (!active(t))
      s.push_back(t.address + " offline / reconnecting");
  if (!last_error.empty())
    s.push_back("Last event: " + last_error);
  return s;
}
} // namespace ce
