#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <tuple>
namespace ce {
struct Id {
  uint64_t replica = 0, counter = 0;

  auto operator<=>(const Id &) const = default;
};
struct Entry {
  Id id;

  char value;
};
void check(bool ok, const std::string &message);
void write_all(int fd, const std::string &bytes);
std::string read_file(const std::string &path);
void atomic_file(const std::string &path, const std::string &data);
uint64_t random_id();
} // namespace ce
