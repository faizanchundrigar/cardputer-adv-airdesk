#pragma once
#include <cstdint>
namespace air {
constexpr int selectionCount(int page) {
  return page == 0 ? 6 : page == 1 ? 4 : page == 2 ? 6 : 2;
}
constexpr int nextSelection(int selected, int delta, int count) {
  return (selected + delta + count) % count;
}
constexpr int navigationDelta(bool up, bool down, bool left, bool right, bool speed) {
  return (down || (!speed && right)) ? 1 : (up || (!speed && left)) ? -1 : 0;
}
constexpr int homeSelection(int selected, bool up, bool down, bool left, bool right) {
  if (up || down) {
    return selected < 2 ? (selected == 0 ? 2 : 4) : (selected < 4 ? 0 : 1);
  }
  if (left || right) {
    int start = selected < 2 ? 0 : 2;
    int count = selected < 2 ? 2 : 4;
    return start + nextSelection(selected - start, right ? 1 : -1, count);
  }
  return selected;
}
constexpr uint8_t mediaMask(int item) {
  return item >= 0 && item < 6 ? static_cast<uint8_t>(1u << item) : 0;
}
constexpr bool releaseDue(bool held, uint32_t now, uint32_t deadline) {
  return held && static_cast<int32_t>(now - deadline) >= 0;
}
}
