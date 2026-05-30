#pragma once
#include "persistence.hpp"
#include <functional>
namespace ce {
class Document {
public:
  Persistence store;
  Crdt crdt;

  std::function<void(const Operation &)> broadcast;
  Document(const std::string &dir, const std::string &name, bool verify_log = false,
           const std::string &requested_name = {})
      : store(dir, name, requested_name) {
    store.replay(crdt, verify_log);
