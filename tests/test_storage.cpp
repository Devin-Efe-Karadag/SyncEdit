#include "document.hpp"
#include <filesystem>
#include <iostream>
#include <unistd.h>
using namespace ce;
namespace fs = std::filesystem;
template <class F> void rejects(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::exception &) {
    rejected = true;
  }
  check(rejected, "corrupt record must fail closed");
}
int main() {
  check(disk::crc32("123456789") == 0xcbf43926U, "CRC32 known vector");

  auto dir = "/tmp/syncedit-storage-" + std::to_string(getpid());
  fs::remove_all(dir);

  std::string original;
  {
    Document d(dir, "notes");
    d.insert(0, 'a');
    d.insert(1, 'b');
    original = read_file(dir + "/operations.log");
  }

  const size_t first = 16 + 72;
  // Handshake revisions must never make unchanged durable operations unreadable.

  for (unsigned char revision : {1, 2, 3}) {
    auto old = original.substr(0, 16);

    for (size_t pos = 16; pos < original.size(); pos += 72) {
      auto payload = original.substr(pos + 12, 56);
      payload[5] = static_cast<char>(revision);
      old += disk::record(payload);
    }
    atomic_file(dir + "/operations.log", old);
