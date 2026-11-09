#pragma once
#include <algorithm>
#include <cmath>
namespace air {
struct Step {
  int x = 0, y = 0;
};
class Motion {
  float bx = 0, by = 0, fx = 0, fy = 0, rx = 0, ry = 0;
  double sx = 0, sy = 0, sz = 0, sq = 0;
  unsigned n = 0;
  bool bad = false;

public:
  bool ready = false;
  float gain = 10;
  void reset() { fx = fy = rx = ry = 0; }
  void calibrate() {
    ready = false;
    sx = sy = sz = sq = 0;
    n = 0;
    bad = false;
    reset();
  }
  unsigned samples() const { return n; }
  bool sample(float x, float y, float z = 0) {
    if (n >= 200)
      return ready;
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
      bad = true;
      return false;
    }
    if (std::abs(x) > 8 || std::abs(y) > 8 || std::abs(z) > 8)
      bad = true;
    sx += x;
    sy += y;
    sz += z;
    sq += x * x + y * y + z * z;
    ++n;
    if (n == 200) {
      bx = sx / n;
      by = sy / n;
      double variance =
          std::max(0., sq / n - bx * bx - by * by - (sz / n) * (sz / n));
      ready = !bad && variance < 1.;
      reset();
    }
    return ready;
  }
  Step step(float x, float y, float dt) {
    if (!ready || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(dt) || dt <= 0 || dt > .05f) {
      reset();
      return {};
    }
    x -= bx;
    y -= by;
    float a = 1 - std::exp(-dt / .035f);
    fx += a * (x - fx);
    fy += a * (y - fy);
    auto axis = [&](float f, float &r) {
      if (std::abs(f) < .5f) {
        r = 0;
        return 0;
      }
      r += std::clamp(f, -150.f, 150.f) * gain * dt;
      int v = std::clamp(static_cast<int>(r), -40, 40);
      r -= v;
      if (std::abs(r) > 40)
        r = 0;
      return v;
    };
    return {axis(fx, rx), axis(fy, ry)};
  }
};
} // namespace air
