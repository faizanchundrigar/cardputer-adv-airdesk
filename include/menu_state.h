#pragma once
namespace air {
enum class InputMode { Keyboard, Mouse };
struct MenuState {
  bool open = true, awaitRelease = false;
  int selected = 0;
  void show() {
    open = true;
    awaitRelease = false;
  }
  void move(int delta) { selected = (selected + delta + 6) % 6; }
  bool activate(InputMode mode, bool connected, bool calibrated) {
    if (!connected || (mode != InputMode::Keyboard && !calibrated))
      return false;
    open = false;
    awaitRelease = true;
    return true;
  }
  void released(bool allReleased) {
    if (allReleased)
      awaitRelease = false;
  }
  bool mayOutput() const { return !open && !awaitRelease; }
};
} // namespace air
