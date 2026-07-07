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
