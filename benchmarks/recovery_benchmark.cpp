#include "document.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <unistd.h>
int main() {
  using namespace ce;
  using Clock = std::chrono::steady_clock;
  namespace fs = std::filesystem;

  auto dir = "/tmp/syncedit-recovery-bench-" + std::to_string(getpid());
  fs::remove_all(dir);

  std::cout << "operations,full_replay_ms,checkpoint_ms,tail_operations\n";

  for (size_t count : {1000U, 10000U, 50000U}) {
    fs::remove_all(dir);
    {
      Document d(dir, "bench");
      Id parent{};

      std::string records = read_file(dir + "/operations.log");

      for (size_t i = 1; i <= count; ++i) {
        Operation o{{100, i}, parent, i, 'a', true};
        records += disk::record(
            wire::frame(wire::Type::OP_BATCH, wire::operations(std::vector<Operation>{o})));
        parent = o.id;
      }
      atomic_file(dir + "/operations.log", records);
    }

    auto start = Clock::now();
    {
      Document d(dir, "bench", true);
      check(d.crdt.rope.size() == count, "recovery benchmark count");
    }

    auto end = Clock::now();
    double full = std::chrono::duration<double, std::milli>(end - start).count();
    {
      Document d(dir, "bench");
      d.save(true);
      d.insert(0, '!');
    }
    start = Clock::now();

    size_t tail = 0;
    {
      Document d(dir, "bench");
      check(d.crdt.rope.size() == count + 1 && d.crdt.rope.at(0).value == '!',
            "checkpoint benchmark text");
      check(d.store.used_checkpoint, "benchmark uses checkpoint");
      tail = d.store.replayed_operations;
    }
    end = Clock::now();

    std::cout << count << ',' << full << ','
              << std::chrono::duration<double, std::milli>(end - start).count() << ',' << tail
              << std::endl;
  }
  fs::remove_all(dir);
}
