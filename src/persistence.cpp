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
      c.receive(o);
      ++replayed_operations;
    }
    offset += length + 16;
  }

  if (offset != end) {
    check(ftruncate(logfd, static_cast<off_t>(offset)) == 0, "repair interrupted log tail");
    check(fsync(logfd) == 0, "sync tail repair");
    recovery_note = "Recovered incomplete final log record";
  }
}
void Persistence::replay(Crdt &c, bool verify_full_log) {
  auto header = read_at(logfd, 0, 16);

  if (header.empty()) {
    generation = random_id();
    atomic_file(dir + "/operations.log", log_header(generation));
    check(close(logfd) == 0, "close empty log");
    logfd = -1;
    open_log();
    header = log_header(generation);
  }

  if (header.substr(0, 8) != "CELOG002") {
    if (header.substr(0, 4) != "CEDT" &&
        !(header.size() < 4 && std::string("CEDT").starts_with(header)))
      throw StorageError("invalid log identity header");
    // One-time, atomic upgrade. Keep the original bytes as an explicit backup.

    auto legacy = bounded_file(dir + "/operations.log"), rest = legacy;

    std::vector<Operation> ops;

    while (!rest.empty()) {
      auto f = wire::take(rest, true);

      if (!f)
        break;
      if (f->type != wire::Type::OP_BATCH)
        throw StorageError("invalid legacy record");
      for (auto o : wire::operations(f->payload))
        ops.push_back(o);
    }
    Crdt validated;
    generation = random_id();

    std::string upgraded = log_header(generation);
      upgraded += disk::record(
          wire::frame(wire::Type::OP_BATCH, wire::operations(std::vector<Operation>{o})));
      atomic_file(dir + "/operations.legacy.log", legacy);
    atomic_file(dir + "/operations.log", upgraded);
    logfd = -1;
    open_log();
    recovery_note = "Upgraded legacy log; original retained as operations.legacy.log";
  }
    throw StorageError("truncated log identity header");
  wire::Reader h{header};
  h.pos = 8;
  if (!generation)
    throw StorageError("invalid log generation");
  uint64_t start = 16;

  if (!verify_full_log && std::filesystem::exists(dir + "/checkpoint.bin")) {
      auto bytes = bounded_file(dir + "/checkpoint.bin");
      if (bytes.substr(0, 8) != "CECKP002")
      r.pos = 8;
      if (hc != disk::crc32(std::string_view(bytes).substr(0, 16)) || length > disk::max_file ||
        throw StorageError("checkpoint header");
      r.pos = 20 + length;
        throw StorageError("checkpoint checksum");
        throw StorageError("checkpoint identity");
      auto covered = p.number(8);
      if (p.str() != document)
      struct stat st{};
      if (covered < 16 || covered > static_cast<uint64_t>(st.st_size))
      if (covered > 16) {
        wire::Reader t{tail};
          throw StorageError("checkpoint log boundary");
      auto count = p.number(8);
        throw StorageError("checkpoint operation limit");
      ops.reserve(count);
        auto n = p.number(4);

        if (n > wire::max_payload || n > payload.size() - p.pos)
        auto batch = wire::operations(payload.substr(p.pos, n));
        p.pos += n;
          throw StorageError("checkpoint batch count");
        ops.insert(ops.end(), batch.begin(), batch.end());
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
  offset += record.size();
}
void Persistence::counter(uint64_t n) {
  atomic_file(dir + "/counter", std::to_string(n));
}
void Persistence::checkpoint(const Crdt &c, bool force) {
  if (!force && c.operations().size() - checkpoint_count < 2048)
  wire::Writer p;
  p.number(replica, 8);
  p.number(offset, 8);
  p.number(boundary_crc, 4);
  p.number(c.operations().size(), 8);

  std::vector<Operation> batch;

  auto flush = [&] {
    if (batch.empty())
      return;
    auto b = wire::operations(batch);
    p.number(b.size(), 4);
    p.data += b;
    batch.clear();
  };

  for (const auto &[id, o] : c.operations()) {
    (void)id;
    batch.push_back(o);

    if (batch.size() == wire::max_batch)
      flush();
  }
  flush();
  wire::Writer w;
  w.data = "CECKP002";
  w.number(p.data.size(), 8);
  w.number(disk::crc32(w.data), 4);
  w.data += p.data;
  w.number(disk::crc32(p.data), 4);
  atomic_file(dir + "/checkpoint.bin", w.data);
  checkpoint_count = c.operations().size();
}
} // namespace ce
