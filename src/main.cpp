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
        names.push_back(value);
    } catch (const std::exception &) {
    }
  }

  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());

  return names;
}
std::vector<std::string> profiles_at(const std::filesystem::path &root) {
  return catalog<std::filesystem::directory_iterator>(
      root / "profiles", 64, [](const auto &path) { return path / "profile_name"; });
}
std::vector<std::string> documents_at(const std::filesystem::path &root,
                                      const std::string &profile) {
  return catalog<std::filesystem::recursive_directory_iterator>(
      root / "profiles" / encode_component(profile) / "documents", 255, [](const auto &path) {
        return path.filename() == "document" && path.parent_path().filename() == "state"
                   ? path
                   : std::filesystem::path{};
      });
}
struct SignalFD {
  int fd = -1;
  sigset_t old{};
  SignalFD() {
    sigset_t set;
    ce::check(sigemptyset(&set) == 0, "signal set");

    for (int s : {SIGINT, SIGTERM, SIGHUP, SIGQUIT})
      ce::check(sigaddset(&set, s) == 0, "signal add");
    ce::check(sigprocmask(SIG_BLOCK, &set, &old) == 0, "signal mask");
    fd = signalfd(-1, &set, SFD_NONBLOCK | SFD_CLOEXEC);
    ce::check(fd >= 0, "signalfd");
  }
  ~SignalFD() {
    if (fd >= 0)
      close(fd);
    sigprocmask(SIG_SETMASK, &old, nullptr);
  }

  bool stop() {
    signalfd_siginfo info{};

    auto r = read(fd, &info, sizeof info);

    if (r < 0) {
      ce::check(errno == EAGAIN || errno == EINTR, "signal read");

      return false;
    }
    ce::check(r == sizeof info, "signal read length");

    return true;
  }
};

enum class Exit { Closed, LoggedOut, Interrupted };
Exit edit(const std::string &dir, const std::string &name, const std::string &profile,
          std::vector<std::string> peers, const std::string &listen, bool headless,

          bool verify_log) {
  auto session = std::filesystem::path(dir) / "peers.conf";

  if (std::filesystem::exists(session)) {
    if (std::filesystem::file_size(session) > 16384)
      throw std::runtime_error("saved peers too large");
    std::istringstream file(ce::read_file(session.string()));

    std::string address;

    while (std::getline(file, address))
      if (!address.empty() && std::find(peers.begin(), peers.end(), address) == peers.end())
        peers.push_back(address);
  }

  if (peers.size() > 64)
    throw std::runtime_error("at most 64 configured peers");
  SignalFD signals;
  ce::Document doc(dir, name, verify_log, profile);

  auto save_peers = [&session](const std::vector<std::string> &addresses) {
    std::string contents;

    for (const auto &address : addresses)
      contents += address + '\n';
    ce::atomic_file(session.string(), contents);
  };
  ce::PeerManager network(doc, name, listen, peers, save_peers);
  network.storage_path = std::filesystem::absolute(dir).string();

  std::cerr << "Document: " << name << "\nStorage: " << dir << "\nListening: " << network.listening
            << '\n';
  std::unique_ptr<ce::EditorUI> ui;

  if (!headless)
    ui = std::make_unique<ce::EditorUI>(doc, network, name, doc.store.display_name);
  else {
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    ce::check(flags >= 0 && fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == 0,
              "stdin nonblocking");
  }

  std::string input;

  bool running = true;

  auto saved = std::chrono::steady_clock::now();

  bool interrupted = false;

  while (running) {
    if (signals.stop()) {
      interrupted = true;
      break;
    }
    network.poll(20);

    if (ui)
      running = ui->step();
    else {
      char b[4096];

      auto n = read(STDIN_FILENO, b, sizeof b);

      if (n > 0)
        input.append(b, static_cast<size_t>(n));
      else if (n < 0)
        ce::check(errno == EAGAIN || errno == EINTR, "stdin read");
      if (input.size() > 65536)
        throw std::runtime_error("command too long");
      size_t p;

      while ((p = input.find('\n')) != std::string::npos) {
        std::string line = input.substr(0, p);
        input.erase(0, p + 1);

        std::istringstream s(line);

        std::string cmd;
        s >> cmd;

        if (cmd == "quit")
          running = false;
        else if (cmd == "checkpoint" || cmd == "save")
          doc.save(cmd == "checkpoint");
        else if (cmd == "insert" || cmd == "erase") {
          size_t index;

          if (!(s >> index))
            throw std::runtime_error(cmd + " needs index");
          if (cmd == "erase") {
            doc.erase(index);
            continue;
          }

          std::string text;

          std::getline(s, text);

          if (!text.empty() && text.front() == ' ')
            text.erase(0, 1);
          for (char c : text)
            doc.insert(index++, c);
        } else
          throw std::runtime_error("unknown headless command");
      }
    }

    auto now = std::chrono::steady_clock::now();

    if (now - saved > std::chrono::seconds(1)) {
      doc.save();
      saved = now;
    }
  }
  doc.save(true);

  return interrupted ? Exit::Interrupted
                     : (ui && ui->logout_requested ? Exit::LoggedOut : Exit::Closed);
}

std::string ask(const std::string &prompt) {
  std::cout << prompt << std::flush;

  std::string answer;

  if (!std::getline(std::cin, answer))
    return {};
  return answer;
}
std::string menu(const std::string &title, const std::vector<std::string> &items,
                 const std::string &empty, const std::string &actions) {
  std::cout << "\n  " << title << "\n  ----------------------------------------\n";

  if (items.empty())
    std::cout << "  " << empty << '\n';
  for (size_t i = 0; i < items.size(); ++i)
    std::cout << "  " << i + 1 << ". " << items[i] << '\n';
  auto choice = ask("\n  " + actions + "\n  Choose: ");
  // Only menu commands are normalized; profile and document names retain their case.

  if (choice.size() == 1 && choice[0] >= 'A' && choice[0] <= 'Z')
    choice[0] += 'a' - 'A';
  return choice;
}
bool valid(const std::string &value, size_t limit) {
  return !value.empty() && value.size() <= limit &&
         std::all_of(value.begin(), value.end(),
                     [](unsigned char c) { return c >= 32 && c <= 126; });
}
std::string numbered(const std::string &choice, const std::vector<std::string> &items) {
  if (choice.empty() || choice.find_first_not_of("0123456789") != std::string::npos)
    return {};
  try {
    auto n = std::stoull(choice);

    if (n > 0 && n <= items.size())
      return items[n - 1];
  } catch (const std::exception &) {
  }

  return {};
}
std::string select_profile(const std::filesystem::path &root) {
  for (;;) {
    auto profiles = profiles_at(root);

    auto choice = menu("Profiles", profiles, "Create a profile before opening documents.",
                       "N. Create profile   Q. Quit");
    if (choice.empty() || choice == "q")
      return {};
    if (choice == "n") {
      auto name = ask("New profile name: ");

      if (!valid(name, 64)) {
        std::cout << "Use 1-64 printable characters.\n";
        continue;
      }

      auto path = root / "profiles" / encode_component(name);

      if (std::filesystem::exists(path / "profile_name")) {
        std::cout << "That profile exists; select it from the list.\n";
        continue;
      }

      std::filesystem::create_directories(path / "documents");
      ce::atomic_file((path / "profile_name").string(), name);

      return name;
    }

    auto selected = numbered(choice, profiles);

    if (!selected.empty())
      return selected;
    std::cout << "Choose a listed number or N.\n";
  }
}
std::filesystem::path document_path(const std::filesystem::path &root, const std::string &profile,
                                    const std::string &name) {
  auto path = root / "profiles" / encode_component(profile) / "documents";

  auto encoded = encode_component(name);

  for (size_t i = 0; i < encoded.size(); i += 120)
    path /= encoded.substr(i, 120);
  return path / "state";
}
// Removal is a same-filesystem rename, retaining logs for manual recovery.
bool remove_local(const std::filesystem::path &root, const std::string &profile,
                  const std::string &document = {}) {
  auto profile_dir = root / "profiles" / encode_component(profile);

  auto target = document.empty() ? profile_dir : document_path(root, profile, document);

  std::cout << "Remove " << (document.empty() ? "profile " : "document ")
            << (document.empty() ? profile : document) << " from profile " << profile << "?\n"
            << "Other peers keep their copies and synchronized edits.\n"
            << "Some local edits may not have reached them. Data goes to the recovery folder.\n";
  auto option = ask("Export plain text first? [Y/n, Enter = yes]: ");

  if (!std::cin)
    return false;
  bool export_text = option.empty() || option == "y" || option == "Y";

  if (!export_text && option != "n" && option != "N")
    return false;
  if (ask("Type REMOVE to confirm (anything else cancels): ") != "REMOVE")
    return false;
  ProfileLock lock(profile_dir, true);

  if (!std::filesystem::exists(target))
    throw std::runtime_error("local data no longer exists");
  // Refuse symlinked data: export and rename must remain inside the selected profile.

  if (std::filesystem::canonical(profile_dir) !=
      std::filesystem::absolute(profile_dir).lexically_normal())
    throw std::runtime_error("cannot remove a symlinked profile");
  if (std::filesystem::canonical(target) != std::filesystem::absolute(target).lexically_normal())
    throw std::runtime_error("cannot remove symlinked document data");
  for (const auto &entry : std::filesystem::recursive_directory_iterator(target))
    if (entry.is_symlink())
      throw std::runtime_error("cannot remove symlinked data");
  auto names = document.empty() ? documents_at(root, profile) : std::vector<std::string>{document};

  std::vector<std::unique_ptr<ce::Document>> opened;

  for (const auto &name : names)
    opened.push_back(
        std::make_unique<ce::Document>(document_path(root, profile, name).string(), name, true));
  auto token = std::to_string(ce::random_id());

  auto recovery = root / "recovery" / token;

  std::filesystem::create_directories(root / "recovery");

  if (std::filesystem::exists(recovery))
    throw std::runtime_error("recovery name collision; retry");
  if (export_text) {
    auto output = root / "exports" / token;

    if (!std::filesystem::create_directories(output))
      throw std::runtime_error("export name collision");
    for (size_t i = 0; i < names.size(); ++i) {
      auto dir = document_path(output, profile, names[i]);

      std::filesystem::create_directories(dir);
      ce::atomic_file((dir / "document_name").string(), names[i]);
      ce::atomic_file((dir / "document.txt").string(), opened[i]->crdt.text());
    }

    std::cout << "Exported to: " << output.string() << '\n';
  }
  ce::atomic_file((root / "recovery" / (token + ".info")).string(),
                  "Original path: " + target.string() + "\nProfile: " + profile +
                      "\nDocument: " + document + "\n");
  std::filesystem::rename(target, recovery);

  std::cout << "Removed locally. Recoverable at: " << recovery.string() << '\n';

  return true;
}
} // namespace
int main(int argc, char **argv) {
  try {
    std::string initial_document;

    std::vector<std::string> initial_peers;

    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];

      if (arg == "--help") {
        std::cout << "Usage: syncedit [DOCUMENT] [--join IPv4:PORT]\n"
                     "Create or select a profile, then open its documents.\n"
                     "Ctrl-Q closes a document; Ctrl-L logs out.\n";
        return 0;
      }

      if (arg == "--join") {
        if (++i == argc)
          throw std::runtime_error("--join needs IPv4:PORT");
        initial_peers.push_back(argv[i]);
      } else if (!arg.empty() && arg.front() != '-' && initial_document.empty())
        initial_document = arg;
      else
        throw std::runtime_error("unknown argument " + arg + " (see --help)");
    }

    if (!initial_document.empty() && !valid(initial_document, 255))
      throw std::runtime_error("invalid document name");
    bool headless = std::getenv("SYNCEDIT_TEST_HEADLESS") != nullptr;

    bool verify = std::getenv("SYNCEDIT_TEST_VERIFY_LOG") != nullptr;

    std::string listen;

    if (const char *value = std::getenv("SYNCEDIT_TEST_LISTEN"))
      listen = value;
    // Explicitly isolated fixtures bypass menus, including the existing ncurses PTY tests.

    if (const char *dir = std::getenv("SYNCEDIT_TEST_DATA_DIR")) {
      if (initial_document.empty())
        throw std::runtime_error("test document required");
      edit(dir, initial_document, "", initial_peers, listen, headless, verify);

      return 0;
    }

    if (headless || !isatty(STDIN_FILENO))
      throw std::runtime_error("open syncedit in an interactive terminal");
    auto root = storage_root();

    for (;;) {
      auto profile = select_profile(root);

      if (profile.empty())
        return 0;
      bool logout = false;

      while (!logout) {
        if (!std::filesystem::exists(root / "profiles" / encode_component(profile) /
                                     "profile_name"))
          break;
        auto documents = documents_at(root, profile);

        std::string name;

        std::vector<std::string> peers;

        if (!initial_document.empty()) {
          name = std::exchange(initial_document, {});
          peers = std::move(initial_peers);
          initial_peers.clear();
        } else {
          auto choice = menu(
              "Documents for " + profile, documents, "No documents yet. Create or join one.",
              "N. New   J. Join   R. Remove document   D. Delete profile   L. Log out   Q. Quit");
          if (choice.empty() || choice == "q")
            return 0;
          if (choice == "l")
            break;
          if (choice == "r" || choice == "d") {
            try {
              auto selected = choice == "r"
                                  ? numbered(ask("Document number to remove: "), documents)
                                  : std::string{};
              if (choice == "r" && selected.empty()) {
                std::cout << "Choose a listed document.\n";
                continue;
              }

              if (remove_local(root, profile, selected) && choice == "d")
                logout = true;
            } catch (const std::exception &error) {
              std::cout << "Removal cancelled: " << error.what() << '\n';
            }
            continue;
          }

          bool create = choice == "n";

          bool join = choice == "j";

          if (create || join) {
            name = ask("Document name: ");

            if (!valid(name, 255)) {
              std::cout << "Use 1-255 printable characters.\n";
              continue;
            }

            if (create && std::find(documents.begin(), documents.end(), name) != documents.end()) {
              std::cout << "Document already exists; select it from the list.\n";
              continue;
            }

            if (join) {
              auto address = ask("Peer IPv4:port: ");

              if (address.empty())
                continue;
              peers.push_back(address);
            }
          } else {
            name = numbered(choice, documents);

            if (name.empty()) {
              std::cout << "Choose a listed number or action.\n";
              continue;
            }
          }

          if (!initial_peers.empty()) {
            peers.insert(peers.end(), initial_peers.begin(), initial_peers.end());
            initial_peers.clear();
          }
        }
        try {
          ProfileLock lock(root / "profiles" / encode_component(profile), false);

          auto result = edit(document_path(root, profile, name).string(), name, profile, peers,
                             listen, false, verify);
          if (result == Exit::Interrupted)
            return 0;
          logout = result == Exit::LoggedOut;
        } catch (const std::exception &error) {
          std::cout << "Could not open document: " << error.what() << '\n';
        }
      }

      std::cout << "Logged out.\n";
    }
  } catch (const std::exception &e) {
    std::cerr << "syncedit: " << e.what() << '\n';

    return 1;
  }
}
