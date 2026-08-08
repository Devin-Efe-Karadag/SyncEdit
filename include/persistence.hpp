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

  size_t checkpoint_count = 0;
  void open_log();
  void replay_records(Crdt &c, uint64_t start);

public:
  uint64_t replica = 0, next = 1;

  std::string display_name;

  size_t replayed_operations = 0;

  bool used_checkpoint = false;

  std::string recovery_note;
  Persistence(const std::string &directory, const std::string &document,
              const std::string &requested_name = {});
  ~Persistence();
  Persistence(const Persistence &) = delete;
  Persistence &operator=(const Persistence &) = delete;
  void replay(Crdt &c, bool verify_full_log = false);
