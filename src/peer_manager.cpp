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
            break;
          c.seen = now;
          if (c.input.size() > wire::max_payload + 12 + sizeof b)
          while (auto f = wire::take(c.input)) {
            if (c.dead)
          }
            break;
      }
        auto r = send(fd, c.output.data(), c.output.size(), MSG_NOSIGNAL);
          c.output.erase(0, static_cast<size_t>(r));
          c.dead = true;
      if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
      watch(c);
      throw; // A failed durable write must stop the editor, never be
    } catch (const std::exception &e) {
                             // retransmitted.
      auto sent = send(fd, error.data(), error.size(), MSG_NOSIGNAL);
      c.dead = true;
  }
    tick = now;
      (void)fd;
        c.dead = true;
      }
        queue(c, wire::Type::SYNC_REQUEST, wire::summary(doc.crdt.summary()));
      }
  }
    if (it->second.dead) {
      check(close(it->first) == 0, "close peer");
    } else
  }
size_t PeerManager::count() const {
                       [](const auto &entry) { return entry.second.ready && !entry.second.dead; });
bool PeerManager::synced() const {
           const auto &c = entry.second;
         });
bool PeerManager::active(const Target &target) const {
    const auto &c = entry.second;
           (c.address == target.address || (target.replica && c.replica == target.replica));
}
  std::vector<std::string> s;
  s.push_back("Storage: " + storage_path);
    s.push_back("Invite: use --join <this machine's IPv4>:" +
  for (const auto &[fd, c] : connections) {
    if (!c.dead)
                  (c.ready ? (c.synced ? "Synced" : "Syncing") : "Connecting") + "  |  " +
  }
    if (!active(t))
  if (!last_error.empty())
  return s;
} // namespace ce
