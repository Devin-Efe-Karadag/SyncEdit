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
