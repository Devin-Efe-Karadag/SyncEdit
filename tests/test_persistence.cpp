#include "document.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
int main() {
  std::string dir = "/tmp/syncedit-unit-" + std::to_string(getpid());
  uint64_t id;
  {
    ce::Document d(dir, "notes");
    id = d.store.replica;
    d.insert(0, 'a');
    d.insert(1, 'b');
    d.insert(1, 'x');
    ce::check(d.crdt.text() == "axb", "middle insert");
    d.erase(0);
    d.save();

    bool locked = false;
    try {
      ce::Document other(dir, "notes");
    } catch (...) {
      locked = true;
    }
    ce::check(locked, "exclusive data directory");
  }
  {
    std::ofstream f(dir + "/operations.log", std::ios::app | std::ios::binary);
    f << "CED";
  }
  {
    ce::Document d(dir, "notes");
    ce::check(d.store.replica == id && d.crdt.text() == "xb" && d.store.next == 5,
              "replay and identity");
    d.insert(0, 'z');
    d.save();
  }
  {
    ce::Document d(dir, "notes");
    ce::check(d.crdt.text() == "zxb" && d.store.next == 6, "tail repair and new edit");
  }
  ce::check(ce::read_file(dir + "/snapshot.txt") == "zxb", "snapshot");
  {
    ce::Document d(dir, "notes");
    d.receive({{88, 2}, {88, 1}, 20, 'q', true});
    d.receive({{88, 3}, {88, 2}, 21, 0, false});
    ce::check(d.crdt.buffered() == 2, "pending operations durable");
  }
  {
    ce::Document d(dir, "notes");
    ce::check(d.crdt.buffered() == 2, "pending replay");
    d.receive({{88, 1}, {}, 19, 'p', true});
    ce::check(d.crdt.text() == "pzxb" && d.crdt.buffered() == 0, "replayed dependency drain");

    auto bytes = std::filesystem::file_size(dir + "/operations.log");

    bool invalid = false;
    try {
      d.receive({{99, 1}, {88, 1}, 1, '!', true});
    } catch (...) {
      invalid = true;
    }
    ce::check(invalid && std::filesystem::file_size(dir + "/operations.log") == bytes,
              "invalid operation never logged");
  }

  bool mismatch = false;
  try {
    ce::Document d(dir, "other");
  } catch (...) {
    mismatch = true;
  }
  ce::check(mismatch, "persistent document mismatch");

  std::filesystem::remove_all(dir);

  std::cout << "replay, interrupted tail, lock, counter, snapshot passed\n";
}
