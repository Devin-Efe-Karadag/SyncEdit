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
