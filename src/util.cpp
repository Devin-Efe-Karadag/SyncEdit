#include "util.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sys/random.h>
#include <unistd.h>
namespace ce {
void check(bool ok, const std::string &s) {
  if (!ok)
    throw std::runtime_error(s + ": " + std::strerror(errno));
}
void write_all(int fd, const std::string &s) {
  size_t n = 0;

  while (n < s.size()) {
    auto k = write(fd, s.data() + n, s.size() - n);

    if (k < 0 && errno == EINTR)
      continue;
    check(k > 0, "write");
    n += static_cast<size_t>(k);
  }
}
std::string read_file(const std::string &p) {
  std::ifstream f(p, std::ios::binary);
  check(bool(f), "open " + p);

  std::string result{std::istreambuf_iterator<char>(f), {}};
  check(!f.bad(), "read " + p);

  return result;
}
void atomic_file(const std::string &p, const std::string &s) {
  int fd = open((p + ".tmp").c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  check(fd >= 0, "open temporary");
  try {
    write_all(fd, s);
    check(fsync(fd) == 0, "fsync");
  } catch (...) {
    close(fd);
    throw;
  }
  check(close(fd) == 0, "close");
  check(rename((p + ".tmp").c_str(), p.c_str()) == 0, "rename");

  int d = open(std::filesystem::path(p).parent_path().c_str(), O_RDONLY | O_DIRECTORY);
  check(d >= 0, "directory");

  int r = fsync(d);

  int c = close(d);
  check(r == 0 && c == 0, "directory sync");
}
uint64_t random_id() {
  uint64_t x = 0;
  check(getrandom(&x, sizeof x, 0) == sizeof x, "getrandom");

  return x ? x : 1;
}
} // namespace ce
