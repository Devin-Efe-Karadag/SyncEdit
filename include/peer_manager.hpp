#pragma once
#include "document.hpp"
#include <chrono>
#include <netinet/in.h>
namespace ce {
class PeerManager {
  using Clock = std::chrono::steady_clock;

  struct Connection {
    int fd = -1;
