#pragma once
namespace air {
constexpr bool mayArm(bool connected, bool calibrated, bool calibrating,
                      bool leftHeld, bool rightHeld, bool stop,
                      bool linkTransition) {
  return connected && calibrated && !calibrating && !leftHeld && !rightHeld &&
         !stop && !linkTransition;
}
} // namespace air
