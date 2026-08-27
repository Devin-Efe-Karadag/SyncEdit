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
    Document restored(dir, "notes", true);
    check(restored.crdt.text() == "ab", "old protocol operation log recovery");
  }
  check(original.size() == 16 + 2 * 72, "record layout");
  // Every possible interrupted write boundary of the last record.

  for (size_t cut = first; cut < original.size(); ++cut) {
    atomic_file(dir + "/operations.log", original.substr(0, cut));
    Document d(dir, "notes");
    check(d.crdt.text() == "a" && d.store.next == 2, "torn write retains committed prefix");
    check(fs::file_size(dir + "/operations.log") == first, "tail repaired");
  }
  // Every byte in a complete record, including header, payload, CRC.

  for (size_t pos = 16; pos < first; ++pos) {
    auto corrupt = original;
    corrupt[pos] ^= 1;
    atomic_file(dir + "/operations.log", corrupt);
    rejects([&] { Document d(dir, "notes"); });
    check(read_file(dir + "/operations.log") == corrupt, "corrupt complete log not truncated");
  }
  atomic_file(dir + "/operations.log", original);

  std::string checkpoint;
  {
    Document d(dir, "notes");
    d.receive({{9, 2}, {9, 1}, 10, 'q', true});
    d.receive({{9, 3}, {9, 2}, 11, 0, false});
    d.save(true);
    checkpoint = read_file(dir + "/checkpoint.bin");
    d.insert(1, 'x');
  }
  {
    Document d(dir, "notes");
    check(d.store.used_checkpoint && d.store.replayed_operations == 1,
          "only checkpoint tail replayed");
    check(d.crdt.text() == "axb" && d.crdt.buffered() == 2, "checkpoint pending state");
    d.receive({{9, 1}, {}, 9, 'p', true});
    check(d.crdt.text() == "paxb" && d.crdt.buffered() == 0, "checkpoint dependencies resolve");
    d.save(true);
