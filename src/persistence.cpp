uint32_t crc32(std::string_view bytes) {
  static constexpr auto table = [] {
    std::array<uint32_t, 256> t{};

    for (uint32_t i = 0; i < 256; ++i) {
      auto c = i;
        c = (c >> 1) ^ ((c & 1) ? 0xedb88320U : 0);
    return t;
  }();
    crc = table[(crc ^ ch) & 255] ^ (crc >> 8);
std::string record(const std::string &p) {
  wire::Writer w;
  w.number(crc32(w.data), 4);
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
    } else {
    }
  check(logfd >= 0, "log open");
}
  if (logfd >= 0)
  if (lockfd >= 0)
}
  struct stat st{};
  if (st.st_size < 0 || static_cast<uint64_t>(st.st_size) > disk::max_file)
  uint64_t end = static_cast<uint64_t>(st.st_size);
  while (offset < end) {
    if (header.size() < 12)
    auto magic = h.number(4), length = h.number(4), crc = h.number(4);
      throw StorageError("corrupt log header at " + std::to_string(offset));
    auto footer = read_at(logfd, offset + 12 + length, 4);
      throw StorageError("log checksum mismatch at " + std::to_string(offset));
    for (auto o : wire::operations(frame->payload)) {
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
