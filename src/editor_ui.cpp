#include "editor_ui.hpp"
#include <algorithm>
#include <ncurses.h>
namespace ce {
EditorUI::EditorUI(Document &d, PeerManager &p, const std::string &n, const std::string &l)
    : doc(d), peers(p), name(n), label(l) {
  if (!initscr())
    throw std::runtime_error("ncurses initialization failed");
  if (raw() == ERR || noecho() == ERR || keypad(stdscr, TRUE) == ERR ||
      nodelay(stdscr, TRUE) == ERR) {
    endwin();
    throw std::runtime_error("terminal setup failed");
  }
  set_escdelay(25);
  notice = doc.store.recovery_note;

  if (notice.empty())
    notice = "Listening: " + peers.listening + " | Ctrl-P: connection details";
}
  cursor = start + std::min(column, end - start);
}
void EditorUI::draw() {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  erase();

  if (rows < 6 || cols < 24) {
    mvaddnstr(0, 0, "Resize terminal to edit", cols);
    refresh();
    return;
  }

  auto put = [cols](int row, const std::string &text, int style = A_NORMAL) {
    attrset(style);
    mvhline(row, 0, ' ', cols);
    mvaddnstr(row, 1, text.c_str(), cols - 2);
    attrset(A_NORMAL);
  };

  auto &r = doc.crdt.rope;

  const size_t height = static_cast<size_t>(rows - 4), width = static_cast<size_t>(cols - 8);

  const size_t line = r.line_of(cursor), column = cursor - r.line_start(line);
  top = std::clamp(top, line >= height ? line - height + 1 : 0, line);
  horizontal = std::clamp(horizontal, column >= width ? column - width + 1 : 0, column);
  put(0, name + "  |  profile: " + label, A_REVERSE | A_BOLD);

  std::string state =
      peers.count() ? (peers.synced() ? "Synced" : "Syncing") : "No peers connected";
  put(1,
      state + "  |  Peers: " + std::to_string(peers.count()) + "  |  Line " +
          std::to_string(line + 1) + ", Col " + std::to_string(column + 1),
      A_BOLD);
  if (panel) {
    auto states = peers.status();

    const size_t panel_height = height > 1 ? height - 1 : 0;
    put(2, "PEERS  |  Up/Down scroll  |  Ctrl-P or Esc returns to editing", A_UNDERLINE);

    size_t limit = states.size() > panel_height ? states.size() - panel_height : 0;
    panel_top = std::min(panel_top, limit);

    for (size_t y = 0; y < panel_height && y + panel_top < states.size(); ++y)
      put(2 + static_cast<int>(y), states[y + panel_top]);
  } else {
    const size_t last = r.line_of(r.size());

    for (size_t y = 0; y < height && top + y <= last; ++y) {
      size_t ln = top + y, begin = r.line_start(ln), end = r.line_start(ln + 1);

      if (end > begin && r.at(end - 1).value == '\n')
        --end;
      int row = 2 + static_cast<int>(y);
      attrset(ln == line ? A_BOLD : A_NORMAL);

      auto number = std::to_string(ln + 1);
      mvaddnstr(row, std::max(1, 6 - static_cast<int>(number.size())), number.c_str(), 5);
      attrset(A_NORMAL);

      if (horizontal < end - begin) {
        auto text = r.range(begin + horizontal, std::min(width, end - begin - horizontal));
        mvaddnstr(row, 7, text.c_str(), static_cast<int>(width));
      }
    }
  }
