#pragma once
#include "crdt.hpp"
#include <optional>
namespace ce::wire {
constexpr uint32_t magic = 0x43454454;
constexpr uint16_t version = 3;
constexpr size_t max_payload = 65536, max_batch = 256, max_replicas = 1024;
enum class Type : uint16_t {
  HELLO = 1,
  HELLO_ACK,
  OP_BATCH,
  SYNC_REQUEST,
  SYNC_RESPONSE,
  PING,
  PONG,
  ERROR,
  PEER_LIST
};
struct Frame {
  Type type;

  std::string payload;
};
struct Writer {
  std::string data;
  void number(uint64_t n, size_t bytes);
  void id(Id i);
  void str(const std::string &s);
};
struct Reader {
  const std::string &data;

  size_t pos = 0;
  uint64_t number(size_t bytes);
  Id id();

  std::string str();
  void end();
};
std::string frame(Type t, const std::string &payload);
std::optional<Frame> take(std::string &buffer, bool operation_log = false);
std::string operations(const std::vector<Operation> &ops);
std::vector<Operation> operations(const std::string &data);
void vector(Writer &w, const Vector &v);
Vector vector(Reader &r);
struct Hello {
  std::string document;
  uint64_t replica;

  std::string peer_name;
  uint16_t listen_port = 0;
  Vector summary;
};
std::string hello(const Hello &h);
Hello hello(const std::string &s);
std::string summary(const Vector &v);
Vector summary(const std::string &s);
std::string peers(const std::vector<std::string> &addresses);
std::vector<std::string> peers(const std::string &data);
} // namespace ce::wire
