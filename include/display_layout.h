#pragma once
namespace air {
constexpr int displayWidth = 240, displayHeight = 135;
constexpr unsigned landscapeRotation(unsigned current, int width, int height) {
  return width < height ? (current + 1) & 3 : current;
}
constexpr bool correctLandscape(int width, int height) {
  return width == displayWidth && height == displayHeight;
}
constexpr bool inside(int x, int y, int w, int h) {
  return x >= 0 && y >= 0 && x + w <= displayWidth && y + h <= displayHeight;
}
static_assert(inside(8, 28, 72, 52) && inside(160, 28, 72, 52));
static_assert(inside(188, 87, 44, 19));
static_assert(inside(8, 124, 224, 8));
} // namespace air
