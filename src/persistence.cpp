#include "persistence.hpp"
#include <array>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
namespace disk {
uint32_t crc32(std::string_view bytes) {
  static constexpr auto table = [] {
    std::array<uint32_t, 256> t{};

    for (uint32_t i = 0; i < 256; ++i) {
      auto c = i;
        c = (c >> 1) ^ ((c & 1) ? 0xedb88320U : 0);
      t[i] = c;
    return t;
  }();
  uint32_t crc = 0xffffffffU;
    crc = table[(crc ^ ch) & 255] ^ (crc >> 8);
  return crc ^ 0xffffffffU;
std::string record(const std::string &p) {
  if (p.size() > wire::max_payload + 12)
  wire::Writer w;
  w.number(0x43454c52, 4);
  w.number(crc32(w.data), 4);
  w.number(crc32(p), 4);
}
namespace {
  const char *candidate = std::getenv("USER");
    candidate = std::getenv("LOGNAME");
  if (name.size() > 64)
  for (char &c : name)
      c = '_';
}
  if (std::filesystem::file_size(path) > disk::max_file)
  return read_file(path);
std::string read_at(int fd, uint64_t pos, size_t length) {
  size_t done = 0;
    auto n = pread(fd, data.data() + done, length - done, static_cast<off_t>(pos + done));
      continue;
    if (!n)
    done += static_cast<size_t>(n);
  data.resize(done);
}
  wire::Writer w;
  w.number(generation, 8);

  return w.data;
}
} // namespace
Persistence::Persistence(const std::string &d, const std::string &doc,
                         const std::string &requested_name)
    : dir(d), document(doc) {
  std::filesystem::create_directories(dir);
  try {
    lockfd = open((dir + "/lock").c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    check(lockfd >= 0, "lock open");
    check(flock(lockfd, LOCK_EX | LOCK_NB) == 0, "data directory already in use");

    if (std::filesystem::exists(dir + "/document")) {
      if (read_file(dir + "/document") != doc)
        throw StorageError("data directory belongs to another document");
    } else
      atomic_file(dir + "/document", doc);
    if (std::filesystem::exists(dir + "/identity")) {
      replica = std::stoull(read_file(dir + "/identity"));
      check(replica != 0, "invalid identity");
    } else {
      replica = random_id();
      atomic_file(dir + "/identity", std::to_string(replica));
    }

    if (std::filesystem::exists(dir + "/display_name")) {
      display_name = read_file(dir + "/display_name");

      if (display_name.empty() || display_name.size() > 64)
        throw StorageError("invalid display name");
      for (unsigned char c : display_name)
        if (c < 32 || c > 126)
          throw StorageError("invalid display name");
      if (!requested_name.empty() && display_name != requested_name)
    } else {
      display_name = requested_name.empty() ? local_name() : requested_name;
    }
    open_log();
    if (logfd >= 0)
    if (lockfd >= 0)
    throw;
}
  check(logfd >= 0, "log open");
}
Persistence::~Persistence() {
  if (logfd >= 0)
    close(logfd);
  if (lockfd >= 0)
    close(lockfd);
}
void Persistence::replay_records(Crdt &c, uint64_t start) {
  struct stat st{};
  check(fstat(logfd, &st) == 0, "stat log");

  if (st.st_size < 0 || static_cast<uint64_t>(st.st_size) > disk::max_file)
    throw StorageError("log size limit");
  uint64_t end = static_cast<uint64_t>(st.st_size);
  offset = start;

  while (offset < end) {
    auto header = read_at(logfd, offset, 12);

    if (header.size() < 12)
      break;
    wire::Reader h{header};

    auto magic = h.number(4), length = h.number(4), crc = h.number(4);

    if (magic != 0x43454c52 || crc != disk::crc32(std::string_view(header).substr(0, 8)) ||
      throw StorageError("corrupt log header at " + std::to_string(offset));
    if (length + 16 > end - offset)
    auto footer = read_at(logfd, offset + 12 + length, 4);
    wire::Reader f{footer};
      throw StorageError("log checksum mismatch at " + std::to_string(offset));
    auto frame = wire::take(payload, true);
    for (auto o : wire::operations(frame->payload)) {
    }
    generation = random_id();
      upgraded += disk::record(
      atomic_file(dir + "/operations.legacy.log", legacy);
    logfd = -1;
    recovery_note = "Upgraded legacy log; original retained as operations.legacy.log";
    throw StorageError("truncated log identity header");
  if (!generation)
        auto n = p.number(4);
        auto batch = wire::operations(payload.substr(p.pos, n));
          throw StorageError("checkpoint batch count");
      p.end();
      c.restore(ops);
      start = covered;
      boundary_crc = static_cast<uint32_t>(boundary);
      checkpoint_count = ops.size();
      used_checkpoint = true;
    } catch (const std::exception &) {
      recovery_note = "Checkpoint rejected; verified full log replay used";
      start = 16;
      checkpoint_count = 0;
      used_checkpoint = false;
    }
  }
  uint64_t recovered = 1;
  while (it != c.operations().end() && it->first.replica == replica) {
    ++it;
  counter(recovered);
void Persistence::append(const Operation &o) {
  auto record = disk::record(payload);
    throw StorageError("log size limit");
  check(fdatasync(logfd) == 0, "sync operation log");
}
  atomic_file(dir + "/counter", std::to_string(n));
void Persistence::checkpoint(const Crdt &c, bool force) {
  wire::Writer p;
  p.number(offset, 8);
  p.number(c.operations().size(), 8);
