#include "persistence.hpp"
#include <array>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
namespace ce {
namespace disk {
uint32_t crc32(std::string_view bytes) {
  static constexpr auto table = [] {
    std::array<uint32_t, 256> t{};

    for (uint32_t i = 0; i < 256; ++i) {
      auto c = i;

      for (int j = 0; j < 8; ++j)
        c = (c >> 1) ^ ((c & 1) ? 0xedb88320U : 0);
      t[i] = c;
    }

    return t;
  }();
  uint32_t crc = 0xffffffffU;

  for (unsigned char ch : bytes)
    crc = table[(crc ^ ch) & 255] ^ (crc >> 8);
  return crc ^ 0xffffffffU;
}
std::string record(const std::string &p) {
  if (p.size() > wire::max_payload + 12)
    throw StorageError("record too large");
  wire::Writer w;
  w.number(0x43454c52, 4);
  w.number(p.size(), 4);
  w.number(crc32(w.data), 4);
  w.data += p;
  w.number(crc32(p), 4);

  return w.data;
}
} // namespace disk
namespace {
std::string local_name() {
  const char *candidate = std::getenv("USER");

  if (!candidate || !*candidate)
    candidate = std::getenv("LOGNAME");
  std::string name = candidate && *candidate ? candidate : "peer";

  if (name.size() > 64)
    name.resize(64);
  for (char &c : name)
    if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126)
      c = '_';
  return name;
}
std::string bounded_file(const std::string &path) {
  if (std::filesystem::file_size(path) > disk::max_file)
    throw StorageError("storage file exceeds limit");
  return read_file(path);
}
std::string read_at(int fd, uint64_t pos, size_t length) {
  std::string data(length, '\0');

  size_t done = 0;

  while (done < length) {
    auto n = pread(fd, data.data() + done, length - done, static_cast<off_t>(pos + done));

    if (n < 0 && errno == EINTR)
      continue;
    check(n >= 0, "read operation log");

    if (!n)
      break;
    done += static_cast<size_t>(n);
  }
  data.resize(done);

  return data;
}
std::string log_header(uint64_t generation) {
  wire::Writer w;
  w.data = "CELOG002";
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
        throw StorageError("local replica name does not match the selected profile");
    } else {
      display_name = requested_name.empty() ? local_name() : requested_name;
      atomic_file(dir + "/display_name", display_name);
    }
    open_log();
  } catch (...) {
    if (logfd >= 0)
      close(logfd);
    if (lockfd >= 0)
      close(lockfd);
    throw;
  }
}
void Persistence::open_log() {
  logfd = open((dir + "/operations.log").c_str(), O_RDWR | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
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
        length > wire::max_payload + 12)
      throw StorageError("corrupt log header at " + std::to_string(offset));
    if (length + 16 > end - offset)
      break;
    auto payload = read_at(logfd, offset + 12, length);

    auto footer = read_at(logfd, offset + 12 + length, 4);
    wire::Reader f{footer};
    boundary_crc = static_cast<uint32_t>(f.number(4));

    if (payload.size() != length || boundary_crc != disk::crc32(payload))
      throw StorageError("log checksum mismatch at " + std::to_string(offset));
    auto frame = wire::take(payload, true);

    if (!frame || !payload.empty() || frame->type != wire::Type::OP_BATCH)
      throw StorageError("invalid log frame");
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
    validated.restore(ops);
    generation = random_id();

    std::string upgraded = log_header(generation);

    for (auto o : ops)
      upgraded += disk::record(
          wire::frame(wire::Type::OP_BATCH, wire::operations(std::vector<Operation>{o})));
    if (!std::filesystem::exists(dir + "/operations.legacy.log"))
      atomic_file(dir + "/operations.legacy.log", legacy);
    atomic_file(dir + "/operations.log", upgraded);
    check(close(logfd) == 0, "close legacy log");
    logfd = -1;
    open_log();
    header = log_header(generation);
    recovery_note = "Upgraded legacy log; original retained as operations.legacy.log";
  }

  if (header.size() != 16)
    throw StorageError("truncated log identity header");
  wire::Reader h{header};
  h.pos = 8;
  generation = h.number(8);

  if (!generation)
    throw StorageError("invalid log generation");
  uint64_t start = 16;

  if (!verify_full_log && std::filesystem::exists(dir + "/checkpoint.bin")) {
    try {
      auto bytes = bounded_file(dir + "/checkpoint.bin");
      wire::Reader r{bytes};

      if (bytes.substr(0, 8) != "CECKP002")
        throw StorageError("checkpoint version");
      r.pos = 8;

      auto length = r.number(8), hc = r.number(4);

      if (hc != disk::crc32(std::string_view(bytes).substr(0, 16)) || length > disk::max_file ||
          length + 24 != bytes.size())
        throw StorageError("checkpoint header");
      auto payload = bytes.substr(20, length);
      r.pos = 20 + length;

      if (r.number(4) != disk::crc32(payload))
        throw StorageError("checkpoint checksum");
      wire::Reader p{payload};

      if (p.number(8) != replica || p.number(8) != generation)
        throw StorageError("checkpoint identity");
      auto covered = p.number(8);

      auto boundary = p.number(4);

      if (p.str() != document)
        throw StorageError("checkpoint document");
      struct stat st{};
      check(fstat(logfd, &st) == 0, "checkpoint log stat");

      if (covered < 16 || covered > static_cast<uint64_t>(st.st_size))
        throw StorageError("checkpoint offset");
      if (covered > 16) {
        auto tail = read_at(logfd, covered - 4, 4);
        wire::Reader t{tail};

        if (t.number(4) != boundary)
          throw StorageError("checkpoint log boundary");
      }

      auto count = p.number(8);

      if (count > 1000000)
        throw StorageError("checkpoint operation limit");
      std::vector<Operation> ops;
      ops.reserve(count);

      while (ops.size() < count) {
        auto n = p.number(4);

        if (n > wire::max_payload || n > payload.size() - p.pos)
          throw StorageError("checkpoint batch length");
        auto batch = wire::operations(payload.substr(p.pos, n));
        p.pos += n;

        if (batch.empty() || batch.size() > count - ops.size())
          throw StorageError("checkpoint batch count");
        ops.insert(ops.end(), batch.begin(), batch.end());
      }
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
  replay_records(c, start);
  uint64_t recovered = 1;

  auto it = c.operations().lower_bound({replica, 0});

  while (it != c.operations().end() && it->first.replica == replica) {
    recovered = std::max(recovered, it->first.counter + 1);
    ++it;
  }
  counter(recovered);
}
void Persistence::append(const Operation &o) {
  auto payload = wire::frame(wire::Type::OP_BATCH, wire::operations(std::vector<Operation>{o}));

  auto record = disk::record(payload);

  if (offset + record.size() > disk::max_file)
    throw StorageError("log size limit");
  write_all(logfd, record);
  check(fdatasync(logfd) == 0, "sync operation log");
  offset += record.size();
  boundary_crc = disk::crc32(payload);
}
void Persistence::counter(uint64_t n) {
  next = n;
  atomic_file(dir + "/counter", std::to_string(n));
}
void Persistence::snapshot(const std::string &s) { atomic_file(dir + "/snapshot.txt", s); }
void Persistence::checkpoint(const Crdt &c, bool force) {
  if (!force && c.operations().size() - checkpoint_count < 2048)
    return;
  wire::Writer p;
  p.number(replica, 8);
  p.number(generation, 8);
  p.number(offset, 8);
  p.number(boundary_crc, 4);
  p.str(document);
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
