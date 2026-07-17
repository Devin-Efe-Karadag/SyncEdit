#pragma once
#include "document.hpp"
#include <chrono>
#include <netinet/in.h>
namespace ce {
class PeerManager {
  using Clock = std::chrono::steady_clock;

  struct Connection {
    int fd = -1;

    bool outbound = false, connecting = false, ready = false, dead = false, synced = false;
    uint64_t replica = 0;

    std::string input, output, address, peer_name, remote_host, remote_endpoint;
    Clock::time_point seen = Clock::now();
  };

  struct Target {
    std::string address;
    Clock::time_point retry = Clock::now();

    int backoff = 1;
    uint64_t replica = 0;
  };
  Document &doc;

  std::string name;

  int ep = -1, listener = -1;

  std::map<int, Connection> connections;

  std::vector<Target> targets;
  Clock::time_point tick = Clock::now();
  uint16_t listen_port = 0;

  std::function<void(const std::vector<std::string> &)> save_peers;
  void watch(Connection &c);
  void add(int fd, bool outbound, bool connecting, const std::string &address);
  void queue(Connection &c, wire::Type t, const std::string &s);
  void message(Connection &c, const wire::Frame &f);
  void missing(Connection &c, const Vector &v);
  void broadcast(const Operation &o, int except);

  bool remember(const std::string &address);

  bool active(const Target &target) const;
  void announce();
