#include "controls.h"
#include "display_layout.h"
#include "keyboard_report.h"
#include "menu_state.h"
#include "motion.h"
#include "ui_actions.h"
#include <cassert>
#include <limits>
int main() {
  assert(air::landscapeRotation(0, 240, 135) == 0);
  assert(air::landscapeRotation(1, 135, 240) == 2);
  assert(air::landscapeRotation(3, 135, 240) == 0);
  assert(air::correctLandscape(240, 135));
  assert(!air::correctLandscape(135, 240));
  auto sym = air::fnSymbolReport(0, {0xff, 0x3a});
  assert(sym[0] == 2 && sym[2] == 0x1e);
  sym = air::fnSymbolReport(0, {0x3b});
  assert(sym[0] == 2 && sym[2] == 0x1f);
  sym = air::fnSymbolReport(0, {0x45});
  assert(sym[0] == 2 && sym[2] == 0x2e);
  auto mouse = air::mouseKeys(true, false, false, true, false, false, false);
  assert(mouse.left && !mouse.right);
  mouse = air::mouseKeys(true, true, false, true, false, false, false);
  assert(!mouse.left);
  mouse = air::mouseKeys(true, true, true, false, true, true, false);
  assert(!mouse.right && !mouse.scroll);
  mouse = air::mouseKeys(false, false, true, true, true, true, false);
  assert(!mouse.left && !mouse.right && !mouse.scroll);
  mouse = air::mouseKeys(true, false, false, false, false, true, true);
  assert(!mouse.scroll);
  assert(mouse.consumed);
  mouse = air::mouseKeys(false, false, true, true, false, false, false);
  assert(!mouse.left);
  assert(
      !air::mayArm(true, true, false, mouse.left || true, false, false, false));
  air::MenuState ui;
  assert(!ui.mayOutput());
  ui.move(-1);
  assert(ui.selected == 5);
  ui.move(1);
  assert(ui.selected == 0);
  assert(!ui.activate(air::InputMode::Mouse, true, false));
  assert(ui.open);
  assert(!ui.activate(air::InputMode::Keyboard, false, true));
  assert(ui.activate(air::InputMode::Keyboard, true, false));
  assert(!ui.mayOutput());
  ui.released(false);
  assert(!ui.mayOutput());
  ui.released(true);
  assert(ui.mayOutput());
  ui.show();
  assert(!ui.mayOutput());
  assert(ui.activate(air::InputMode::Mouse, true, true));
  assert(!ui.mayOutput());
  ui.released(true);
  assert(ui.mayOutput());
  auto kr = air::keyboardReport(2, {4, 0x28, 0x2a});
  assert(kr[0] == 2 && kr[2] == 4 && kr[3] == 0x28 && kr[4] == 0x2a);
  kr = air::keyboardReport(0, {0xff, 0x50});
  assert(kr[0] == 0 && kr[2] == 0x50 && kr[3] == 0);
  kr = air::keyboardReport(8, {4, 4});
  assert(kr[0] == 8 && kr[2] == 4 && kr[3] == 0);
  kr = air::keyboardReport(0, {4, 5, 6, 7, 8, 9, 10});
  for (int i = 2; i < 8; i++)
    assert(kr[i] == 1);
  kr = air::keyboardReport(0, {});
  for (auto b : kr)
    assert(b == 0);
  assert(air::mayArm(true, true, false, false, false, false, false));
  assert(!air::mayArm(true, true, false, false, false, true, false));
  assert(!air::mayArm(true, true, false, false, false, false, true));
  assert(!air::mayArm(true, true, false, true, false, false, false));
  assert(!air::mayArm(true, true, false, false, true, false, false));
  assert(!air::mayArm(false, true, false, false, false, false, false));
  assert(!air::mayArm(true, false, false, false, false, false, false));
  assert(!air::mayArm(true, true, true, false, false, false, false));
  air::Motion m;
  assert(m.step(20, 20, .01).x == 0);
  m.calibrate();
  for (int i = 0; i < 200; i++)
    m.sample(1, -1);
  assert(m.ready);
  for (int i = 0; i < 100; i++) {
    auto s = m.step(1, -1, .01);
    assert(s.x == 0 && s.y == 0);
  }
  auto s = m.step(100, 0, .01);
  assert(s.x > 0 && s.x <= 40);
  m.reset();
  assert(m.step(20, 20, 1).x == 0);
  assert(m.step(NAN, 1, .01).x == 0);
  m.calibrate();
  for (int i = 0; i < 200; i++)
    m.sample(i % 2 ? 7 : -7, 0);
  assert(!m.ready);
  m.calibrate();
  for (int i = 0; i < 200; i++)
    m.sample(9, 0);
  assert(!m.ready);
  m.calibrate();
  for (int i = 0; i < 200; i++)
    m.sample(0, 0);
  assert(m.ready);
  for (int i = 0; i < 100; i++) {
    s = m.step(0, 0, .01);
    assert(!s.x && !s.y);
  }
  m.calibrate();
  for (int i = 0; i < 200; i++)
    m.sample(0, 0, i % 2 ? 7 : -7);
  assert(!m.ready);
  for (int page = 0; page < 4; ++page) {
    int count = air::selectionCount(page);
    assert(air::nextSelection(0, -1, count) == count - 1);
    assert(air::nextSelection(count - 1, 1, count) == 0);
  }
  assert(air::selectionCount(0) == 6);
  assert(air::navigationDelta(true, false, false, false, false) == -1);
  assert(air::navigationDelta(false, false, false, true, false) == 1);
  assert(air::navigationDelta(false, false, true, false, true) == 0);
  assert(air::navigationDelta(false, true, false, false, true) == 1);
  for (int i = 0; i < 6; ++i) assert(air::mediaMask(i) == (1u << i));
  assert(air::mediaMask(-1) == 0 && air::mediaMask(6) == 0);
  assert(!air::releaseDue(false, 80, 80));
  assert(!air::releaseDue(true, 79, 80));
  assert(air::releaseDue(true, 80, 80));
  assert(air::releaseDue(true, 81, 80));
  assert(!air::releaseDue(true, 0xfffffff0u, 10));
  assert(air::releaseDue(true, 11, 10));
  assert(air::homeSelection(0, false, false, true, false) == 1);
  assert(air::homeSelection(1, false, false, false, true) == 0);
  assert(air::homeSelection(1, false, true, false, false) == 4);
  assert(air::homeSelection(5, true, false, false, false) == 1);
  assert(air::homeSelection(2, false, false, true, false) == 5);
  assert(air::homeSelection(5, false, false, false, false) == 5);
  return 0;
}
