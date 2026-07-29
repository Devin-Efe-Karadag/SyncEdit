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
    d.insert(1, 'b');
    ce::check(d.crdt.text() == "axb", "middle insert");
    d.save();
    try {
    } catch (...) {
    }
  }
    std::ofstream f(dir + "/operations.log", std::ios::app | std::ios::binary);
  }
    ce::check(d.store.replica == id && d.crdt.text() == "xb" && d.store.next == 5,
              "replay and identity");
    d.save();
  {
    ce::check(d.crdt.text() == "zxb" && d.store.next == 6, "tail repair and new edit");
  ce::check(ce::read_file(dir + "/snapshot.txt") == "zxb", "snapshot");
    ce::Document d(dir, "notes");
    d.receive({{88, 3}, {88, 2}, 21, 0, false});
  }
    ce::Document d(dir, "notes");
    d.receive({{88, 1}, {}, 19, 'p', true});
    auto bytes = std::filesystem::file_size(dir + "/operations.log");
    try {
    } catch (...) {
    }
              "invalid operation never logged");
  bool mismatch = false;
    ce::Document d(dir, "other");
    mismatch = true;
  ce::check(mismatch, "persistent document mismatch");
  std::cout << "replay, interrupted tail, lock, counter, snapshot passed\n";
