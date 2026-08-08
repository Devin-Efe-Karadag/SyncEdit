    throw std::runtime_error("local data no longer exists");
  // Refuse symlinked data: export and rename must remain inside the selected profile.
      std::filesystem::absolute(profile_dir).lexically_normal())
  if (std::filesystem::canonical(target) != std::filesystem::absolute(target).lexically_normal())
  for (const auto &entry : std::filesystem::recursive_directory_iterator(target))
      throw std::runtime_error("cannot remove symlinked data");
  std::vector<std::unique_ptr<ce::Document>> opened;
    opened.push_back(
