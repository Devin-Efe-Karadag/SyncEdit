#pragma once
#include "protocol.hpp"
#include <string_view>
namespace ce {
class StorageError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
namespace disk {
constexpr size_t max_file = 128 * 1024 * 1024;
uint32_t crc32(std::string_view bytes);
std::string record(const std::string &payload);
} // namespace disk
class Persistence {
  std::string dir, document;

  int lockfd = -1, logfd = -1;
  uint64_t generation = 0, offset = 16;
  uint32_t boundary_crc = 0;
