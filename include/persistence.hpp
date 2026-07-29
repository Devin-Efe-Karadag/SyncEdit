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
