#include "editor_ui.hpp"
#include <algorithm>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <signal.h>
#include <sstream>
#include <sys/file.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <utility>
namespace {
struct ProfileLock {
  int fd = -1;
  ProfileLock(const std::filesystem::path &profile, bool exclusive) {
    if (!std::filesystem::exists(profile / "profile_name"))
      throw std::runtime_error("profile no longer exists");
    fd = open((profile / "profile.lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    ce::check(fd >= 0, "profile lock");

    if (flock(fd, (exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) != 0) {
      close(fd);
      fd = -1;
      throw std::runtime_error("profile is in use; close its editors first");
    }

    if (!std::filesystem::exists(profile / "profile_name")) {
      close(fd);
      fd = -1;
      throw std::runtime_error("profile no longer exists");
    }
  }
  ~ProfileLock() {
    if (fd >= 0)
      close(fd);
  }
  ProfileLock(const ProfileLock &) = delete;
  ProfileLock &operator=(const ProfileLock &) = delete;
};
std::string encode_component(const std::string &value) {
  constexpr char hex[] = "0123456789abcdef";

  std::string encoded;
  encoded.reserve(value.size() * 2);

  for (unsigned char c : value) {
    encoded += hex[c >> 4];
    encoded += hex[c & 15];
  }

  return encoded;
}
std::filesystem::path storage_root() {
  const char *xdg = std::getenv("XDG_DATA_HOME");

  const char *home = std::getenv("HOME");

  if (xdg && *xdg && std::filesystem::path(xdg).is_absolute())
    return std::filesystem::path(xdg) / "syncedit";
  if (home && *home)
    return std::filesystem::path(home) / ".local/share/syncedit";
  throw std::runtime_error("set HOME or XDG_DATA_HOME");
}
// Both catalogs share bounded metadata reading and sorting, but use different traversals.
template <class Iterator, class Metadata>
std::vector<std::string> catalog(const std::filesystem::path &base, size_t limit,
                                 Metadata metadata) {
  std::vector<std::string> names;

  std::error_code error;

  for (Iterator it(base, std::filesystem::directory_options::skip_permission_denied, error), end;
       !error && it != end; it.increment(error)) {
    auto path = metadata(it->path());

    if (path.empty())
      continue;
    try {
      if (std::filesystem::file_size(path) > limit)
        continue;
      auto value = ce::read_file(path.string());

      if (!value.empty())
    } catch (const std::exception &) {
  }
  names.erase(std::unique(names.begin(), names.end()), names.end());
}
  return catalog<std::filesystem::directory_iterator>(
}
                                      const std::string &profile) {
      root / "profiles" / encode_component(profile) / "documents", 255, [](const auto &path) {
                   ? path
      });
struct SignalFD {
  sigset_t old{};
    sigset_t set;
    for (int s : {SIGINT, SIGTERM, SIGHUP, SIGQUIT})
    ce::check(sigprocmask(SIG_BLOCK, &set, &old) == 0, "signal mask");
    ce::check(fd >= 0, "signalfd");
  ~SignalFD() {
      close(fd);
  }
    signalfd_siginfo info{};
    if (r < 0) {
      return false;
    ce::check(r == sizeof info, "signal read length");
  }
enum class Exit { Closed, LoggedOut, Interrupted };
          std::vector<std::string> peers, const std::string &listen, bool headless,
  auto session = std::filesystem::path(dir) / "peers.conf";
    if (std::filesystem::file_size(session) > 16384)
    std::istringstream file(ce::read_file(session.string()));
    while (std::getline(file, address))
        peers.push_back(address);
  if (peers.size() > 64)
  SignalFD signals;
  auto save_peers = [&session](const std::vector<std::string> &addresses) {
    for (const auto &address : addresses)
    ce::atomic_file(session.string(), contents);
  ce::PeerManager network(doc, name, listen, peers, save_peers);
  std::cerr << "Document: " << name << "\nStorage: " << dir << "\nListening: " << network.listening
  std::unique_ptr<ce::EditorUI> ui;
    ui = std::make_unique<ce::EditorUI>(doc, network, name, doc.store.display_name);
    int flags = fcntl(STDIN_FILENO, F_GETFL);
              "stdin nonblocking");
  std::string input;
  auto saved = std::chrono::steady_clock::now();
  while (running) {
      interrupted = true;
    }
    if (ui)
    else {
      auto n = read(STDIN_FILENO, b, sizeof b);
        input.append(b, static_cast<size_t>(n));
        ce::check(errno == EAGAIN || errno == EINTR, "stdin read");
        throw std::runtime_error("command too long");
      while ((p = input.find('\n')) != std::string::npos) {
        input.erase(0, p + 1);
        std::string cmd;
        if (cmd == "quit")
        else if (cmd == "checkpoint" || cmd == "save")
        else if (cmd == "insert" || cmd == "erase") {
          if (!(s >> index))
          if (cmd == "erase") {
            continue;
          std::string text;
          if (!text.empty() && text.front() == ' ')
          for (char c : text)
        } else
      }
    auto now = std::chrono::steady_clock::now();
      doc.save();
    }
  doc.save(true);
                     : (ui && ui->logout_requested ? Exit::LoggedOut : Exit::Closed);
std::string ask(const std::string &prompt) {
  if (!std::getline(std::cin, answer))
  return answer;
std::string menu(const std::string &title, const std::vector<std::string> &items,
  std::cout << "\n  " << title << "\n  ----------------------------------------\n";
    std::cout << "  " << empty << '\n';
    std::cout << "  " << i + 1 << ". " << items[i] << '\n';
  // Only menu commands are normalized; profile and document names retain their case.
    choice[0] += 'a' - 'A';
}
  return !value.empty() && value.size() <= limit &&
                     [](unsigned char c) { return c >= 32 && c <= 126; });
std::string numbered(const std::string &choice, const std::vector<std::string> &items) {
    return {};
    auto n = std::stoull(choice);
      return items[n - 1];
  }
}
  for (;;) {
    auto choice = menu("Profiles", profiles, "Create a profile before opening documents.",
    if (choice.empty() || choice == "q")
    if (choice == "n") {
      if (!valid(name, 64)) {
        continue;
      auto path = root / "profiles" / encode_component(name);
        std::cout << "That profile exists; select it from the list.\n";
      }
      ce::atomic_file((path / "profile_name").string(), name);
    }
bool remove_local(const std::filesystem::path &root, const std::string &profile,
  auto profile_dir = root / "profiles" / encode_component(profile);
  std::cout << "Remove " << (document.empty() ? "profile " : "document ")
    throw std::runtime_error("local data no longer exists");
  // Refuse symlinked data: export and rename must remain inside the selected profile.
      std::filesystem::absolute(profile_dir).lexically_normal())
  if (std::filesystem::canonical(target) != std::filesystem::absolute(target).lexically_normal())
  for (const auto &entry : std::filesystem::recursive_directory_iterator(target))
      throw std::runtime_error("cannot remove symlinked data");
  std::vector<std::unique_ptr<ce::Document>> opened;
    opened.push_back(
  auto token = std::to_string(ce::random_id());
  std::filesystem::create_directories(root / "recovery");
    throw std::runtime_error("recovery name collision; retry");
    auto output = root / "exports" / token;
      throw std::runtime_error("export name collision");
      auto dir = document_path(output, profile, names[i]);
      ce::atomic_file((dir / "document_name").string(), names[i]);
    }
  }
                  "Original path: " + target.string() + "\nProfile: " + profile +
  std::filesystem::rename(target, recovery);
  return true;
} // namespace
  try {
    std::vector<std::string> initial_peers;
      std::string arg = argv[i];
        std::cout << "Usage: syncedit [DOCUMENT] [--join IPv4:PORT]\n"
                     "Ctrl-Q closes a document; Ctrl-L logs out.\n";
      }
        if (++i == argc)
        initial_peers.push_back(argv[i]);
        initial_document = arg;
        throw std::runtime_error("unknown argument " + arg + " (see --help)");
    if (!initial_document.empty() && !valid(initial_document, 255))
    bool headless = std::getenv("SYNCEDIT_TEST_HEADLESS") != nullptr;
    std::string listen;
      listen = value;
    if (const char *dir = std::getenv("SYNCEDIT_TEST_DATA_DIR")) {
        throw std::runtime_error("test document required");
      return 0;
    if (headless || !isatty(STDIN_FILENO))
    auto root = storage_root();
      auto profile = select_profile(root);
        return 0;
      while (!logout) {
                                     "profile_name"))
        auto documents = documents_at(root, profile);
        std::vector<std::string> peers;
          name = std::exchange(initial_document, {});
          initial_peers.clear();
          auto choice = menu(
              "N. New   J. Join   R. Remove document   D. Delete profile   L. Log out   Q. Quit");
            return 0;
            break;
            try {
                                  ? numbered(ask("Document number to remove: "), documents)
              if (choice == "r" && selected.empty()) {
                continue;
              if (remove_local(root, profile, selected) && choice == "d")
            } catch (const std::exception &error) {
            }
          }
          bool join = choice == "j";
            name = ask("Document name: ");
              std::cout << "Use 1-255 printable characters.\n";
            }
              std::cout << "Document already exists; select it from the list.\n";
            }
              auto address = ask("Peer IPv4:port: ");
                continue;
            }
            name = numbered(choice, documents);
              std::cout << "Choose a listed number or action.\n";
            }
          if (!initial_peers.empty()) {
            initial_peers.clear();
        }
          ProfileLock lock(root / "profiles" / encode_component(profile), false);
                             listen, false, verify);
            return 0;
        } catch (const std::exception &error) {
        }
      std::cout << "Logged out.\n";
  } catch (const std::exception &e) {
    return 1;
}
