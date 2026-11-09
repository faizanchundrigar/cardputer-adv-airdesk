#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace air {
using KeyboardReport = std::array<uint8_t, 8>;
inline KeyboardReport keyboardReport(uint8_t modifiers,
                                     const std::vector<uint8_t> &keys) {
  KeyboardReport r{};
  r[0] = modifiers;
  size_t count = 0;
  for (auto raw : keys) {
    // Library Fn sentinel is 0xff, not a HID usage. Modifiers are separate.
    if (raw > 0x65)
      continue;
    auto key = raw;
    if (!key)
      continue;
    bool duplicate = false;
    for (size_t i = 0; i < count; i++)
      if (r[i + 2] == key)
        duplicate = true;
    if (duplicate)
      continue;
    if (count == 6) {
      for (size_t i = 2; i < 8; i++)
        r[i] = 1;
      return r;
    }
    r[2 + count++] = key;
  }
  return r;
}
} // namespace air
namespace air {
// Requested Fn number-row aliases replace F1..F12 on this compact layout.
inline KeyboardReport fnSymbolReport(uint8_t modifiers,
                                     std::vector<uint8_t> keys) {
  for (auto &key : keys) {
    if (key >= 0x3a && key <= 0x45) {
      int n = key - 0x3a;
      key = n < 10 ? static_cast<uint8_t>(0x1e + n)
                   : static_cast<uint8_t>(n == 10 ? 0x2d : 0x2e);
      modifiers |= 2;
    }
  }
  return keyboardReport(modifiers, keys);
}
struct MouseKeys {
  bool left, right;
  int scroll;
  bool consumed;
};
constexpr MouseKeys mouseKeys(bool armed, bool keyboardEnabled, bool fn,
                              bool leftPos, bool rightPos, bool upPos,
                              bool downPos) {
  (void)fn; // No alternate mouse layer: modes are mutually exclusive.
  bool enabled = armed && !keyboardEnabled;
  return {enabled && leftPos, enabled && rightPos,
          enabled ? ((upPos ? 1 : 0) - (downPos ? 1 : 0)) : 0,
          enabled && (leftPos || rightPos || upPos || downPos)};
}
} // namespace air
