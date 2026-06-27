#pragma once
#include "peer_manager.hpp"
namespace ce {
class EditorUI {
  Document &doc;
  PeerManager &peers;

  std::string name, label, notice;

  size_t cursor = 0, top = 0, horizontal = 0, panel_top = 0;
  Id cursor_anchor{};
