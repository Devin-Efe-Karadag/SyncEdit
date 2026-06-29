#pragma once
#include "peer_manager.hpp"
namespace ce {
class EditorUI {
  Document &doc;
  PeerManager &peers;

  std::string name, label, notice;

  size_t cursor = 0, top = 0, horizontal = 0, panel_top = 0;
  Id cursor_anchor{};

  bool panel = false;
  void vertical(int delta);
  void draw();

public:
  bool logout_requested = false;
  EditorUI(Document &d, PeerManager &p, const std::string &n, const std::string &label);
  ~EditorUI();

  bool step();
};
} // namespace ce
