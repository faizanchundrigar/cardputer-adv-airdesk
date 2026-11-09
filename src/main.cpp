#include "controls.h"
#include "display_layout.h"
#include "keyboard_report.h"
#include "menu_state.h"
#include "motion.h"
#include "ui_actions.h"
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <M5Cardputer.h>
#include <atomic>
// Composite HID: report 1 relative mouse; report 2 standard 6-key keyboard.
const uint8_t descriptor[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x01, 0x09, 0x01, 0xA1, 0x00,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03,
    0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x03, 0x05, 0x01,
    0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08,
    0x95, 0x03, 0x81, 0x06, 0xC0, 0xC0, 0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
    0x85, 0x02, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x75, 0x08, 0x95, 0x01, 0x81, 0x01,
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x75, 0x01, 0x95, 0x05, 0x91, 0x02,
    0x75, 0x03, 0x95, 0x01, 0x91, 0x01, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65,
    0x15, 0x00, 0x25, 0x65, 0x75, 0x08, 0x95, 0x06, 0x81, 0x00, 0xC0,
    // Consumer report 3: play/pause, next, previous, volume up/down, mute.
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x03,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x06,
    0x09, 0xCD, 0x09, 0xB5, 0x09, 0xB6, 0x09, 0xE9,
    0x09, 0xEA, 0x09, 0xE2, 0x81, 0x02, 0x75, 0x02,
    0x95, 0x01, 0x81, 0x03, 0xC0};
std::atomic<bool> linked{false}, linkChanged{false};
BLEHIDDevice *hid = nullptr;
BLECharacteristic *report = nullptr;
BLECharacteristic *keyboardInput = nullptr;
BLECharacteristic *consumerInput = nullptr;
bool consumerHeld = false;
uint32_t consumerReleaseAt = 0;
enum class Page { Home, Settings, Media, Shortcuts };
Page page = Page::Home;
int pageSelection = 0;
void releaseConsumer() {
  if (consumerHeld && linked && consumerInput) {
    uint8_t zero = 0;
    consumerInput->setValue(&zero, 1);
    consumerInput->notify();
  }
  consumerHeld = false;
}
void mediaPress(int item) {
  releaseConsumer();
  if (!linked || !consumerInput) return;
  uint8_t bits = air::mediaMask(item);
  consumerInput->setValue(&bits, 1);
  consumerInput->notify();
  consumerHeld = true;
  consumerReleaseAt = millis() + 80;
}
air::KeyboardReport lastKeyboard{};
bool keyboardEnabled = false, suppressKeyboard = false;
BLEAdvertising *advertising = nullptr;
air::Motion motion;
bool armed = false, calibrating = false, validBoard = false,
     advertisingOn = false;
uint8_t lastButtons = 0;
int lastScrollDirection = 0;
uint32_t lastScrollMs = 0;
uint32_t lastGoodSample = 0, lastSample = 0, lastPaint = 0, advertiseUntil = 0;

air::MenuState menu;
bool diagnosticsOpen = false;
float gyroX = 0, gyroY = 0, gyroZ = 0;
uint32_t mouseReports = 0, freshGyroSamples = 0;
unsigned lastSensorMask = 0;
int mouseDx = 0, mouseDy = 0;
bool helpOpen = false, prevUp = false, prevDown = false, prevEnter = false,
     prevLeft = false, prevRight = false;
String notice = "Choose a mode";
uint32_t calibrationAfter = 0;
void openMenu();
M5Canvas frame(&M5Cardputer.Display);
bool frameReady = false;
class Callbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    linked = true;
    linkChanged = true;
  }
  void onDisconnect(BLEServer *) override {
    linked = false;
    linkChanged = true;
  }
};
void send(uint8_t buttons, int x = 0, int y = 0, int wheel = 0) {
  if (!linked || !report)
    return;
  uint8_t bytes[] = {buttons, static_cast<uint8_t>(static_cast<int8_t>(x)),
                     static_cast<uint8_t>(static_cast<int8_t>(y)),
                     static_cast<uint8_t>(static_cast<int8_t>(wheel))};
  report->setValue(bytes, 4);
  report->notify();
  ++mouseReports;
  mouseDx = x;
  mouseDy = y;
  lastButtons = buttons;
}
void pauseCursor() {
  if (linked && lastButtons)
    send(0);
  lastButtons = 0;
  lastScrollDirection = 0;
  armed = false;
  calibrating = false;
  motion.reset();
}
void sendKeyboard(const air::KeyboardReport &bytes) {
  if (!linked || !keyboardInput)
    return;
  keyboardInput->setValue(const_cast<uint8_t *>(bytes.data()), bytes.size());
  keyboardInput->notify();
  lastKeyboard = bytes;
}
void pauseAll() {
  releaseConsumer();
  pauseCursor();
  if (linked && lastKeyboard != air::KeyboardReport{})
    sendKeyboard({});
  lastKeyboard = {};
  keyboardEnabled = false;
  suppressKeyboard = true;
}
void openMenu() {
  pauseAll();
  menu.show();
  page = Page::Home;
  pageSelection = 0;
  helpOpen = false;
  diagnosticsOpen = false;
  notice = "Inputs stopped";
}
// 240x135 landscape UI: all coordinates stay inside the physical viewport.
namespace ui {
constexpr uint16_t bg = 0x0841, surface = 0x10A3, border = 0x2946,
                   accent = 0x4E7F, white = 0xEFFF, muted = 0x8C92,
                   green = 0x5EAA, amber = 0xFDC4, selected = 0x1A28;
void text(int x, int y, const String &value, uint16_t color = white,
          int size = 1) {
  auto &d = frame;
  d.setTextSize(size);
  d.setTextColor(color);
  d.setCursor(x, y);
  d.print(value);
}
void pill(int x, int y, int w, const char *label, uint16_t color) {
  auto &d = frame;
  d.fillRoundRect(x, y, w, 16, 4, surface);
  d.drawRoundRect(x, y, w, 16, 4, color);
  text(x + 5, y + 4, label, color);
}
void keyboardIcon(int x, int y, uint16_t c) {
  auto &d = frame;
  d.drawRoundRect(x, y, 30, 18, 3, c);
  for (int row = 0; row < 2; row++)
    for (int col = 0; col < 5; col++)
      d.fillRect(x + 4 + col * 5, y + 4 + row * 5, 3, 2, c);
  d.drawFastHLine(x + 10, y + 14, 11, c);
}
void mouseIcon(int x, int y, uint16_t c) {
  auto &d = frame;
  d.drawRoundRect(x, y, 17, 24, 7, c);
  d.drawFastVLine(x + 8, y + 2, 8, c);
  d.drawFastHLine(x + 2, y + 10, 13, c);
}
} // namespace ui
void renderFrame() {
  auto &d = frame;
  d.fillScreen(ui::bg);
  d.setFont(&fonts::Font0);
  d.setTextWrap(false);
  d.fillRoundRect(8, 6, 3, 12, 1, ui::accent);
  ui::text(16, 8, "TINY DECK", ui::white);
  ui::pill(176, 4, 56,
           linked          ? "LINKED"
           : advertisingOn ? "PAIRING"
                           : "OFFLINE",
           linked ? ui::green : ui::amber);
  if (menu.open) {
    if (page != Page::Home && !calibrating && !helpOpen && !diagnosticsOpen) {
      const char *title = page == Page::Settings ? "SETTINGS" : page == Page::Media ? "MEDIA" : "APP SHORTCUTS";
      ui::text(10, 28, title, ui::accent);
      const char *settings[] = {"Calibrate gyro", "Mouse speed", "Quick guide", "Device info"};
      const char *media[] = {"Play / pause", "Next track", "Previous track", "Volume up", "Volume down", "Mute"};
      const char *apps[] = {"Bambu Studio  [setup needed]", "Chrome        [setup needed]"};
      int count = page == Page::Settings ? 4 : page == Page::Media ? 6 : 2;
      const int first = page == Page::Media ? (pageSelection / 3) * 3 : 0;
      const int last = page == Page::Media ? std::min(count, first + 3) : count;
      for (int i = first; i < last; ++i) {
        int step = page == Page::Media ? 22 : 18;
        int y = 43 + (i - first) * step;
        if (pageSelection == i) d.fillRoundRect(8, y - 2, 224, step, 3, ui::selected);
        ui::text(12, y, page == Page::Settings ? settings[i] : page == Page::Media ? media[i] : apps[i], pageSelection == i ? ui::white : ui::muted, page == Page::Shortcuts ? 1 : 2);
        if (page == Page::Settings && i == 1) ui::text(196, y, String((int)motion.gain), ui::accent, 2);
      }
      if (page == Page::Media) ui::text(210, 28, first == 0 ? "1/2" : "2/2", ui::muted);
      if (page == Page::Shortcuts) {
        ui::text(10, 77, "Mac Shortcuts hotkeys required", ui::amber);
        ui::text(10, 91, "Disabled until setup confirmed", ui::muted);
      }
      ui::text(8, 114, (!linked && page == Page::Media) ? String("Offline: G0 then PAIR") : notice.substring(0, 37), ui::amber);
      ui::text(8, 124, "Arrows move  Enter select  G0 home", ui::muted);
      return;
    }
    if (calibrating) {
      bool settling = static_cast<int32_t>(millis() - calibrationAfter) < 0;
      ui::text(16, 36, settling ? "SET IT DOWN" : "HOLD STILL", ui::white, 2);
      ui::text(16, 61,
               settling ? "One second to settle" : "Measuring your gyro",
               ui::muted);
      d.fillRoundRect(16, 82, 208, 10, 4, ui::surface);
      int fill = std::min(208, static_cast<int>(motion.samples() * 208 / 200));
      if (fill > 0)
        d.fillRoundRect(16, 82, fill, 10, 4, ui::accent);
      ui::text(16, 103, String(motion.samples()) + " / 200 samples",
               ui::accent);
      ui::text(16, 124, "G0 cancel", ui::muted);
      return;
    }
    if (helpOpen) {
      ui::text(10, 28, "QUICK GUIDE", ui::accent);
      const char *lines[] = {
          "Arrow keys      Move highlight", "Enter           Choose a mode",
          "Mouse arrows    Click / scroll", "Shift or Fn+1/2 ! and @ symbols",
          "G0 / Fn+Tab     STOP + menu",    "Opt = Command  Alt = Option"};
      for (int i = 0; i < 6; i++)
        ui::text(10, 44 + i * 11, lines[i], i == 4 ? ui::amber : ui::white);
      ui::text(10, 124, "Enter returns", ui::muted);
      return;
    }
    if (diagnosticsOpen) {
      auto cccd = report ? static_cast<BLE2902 *>(report->getDescriptorByUUID(
                               BLEUUID((uint16_t)0x2902)))
                         : nullptr;
      ui::text(10, 27, "MOTION CHECK", ui::accent);
      char line[44];
      snprintf(line, sizeof(line), "X %+.1f  Y %+.1f  Z %+.1f", gyroX, gyroY,
               gyroZ);
      ui::text(10, 44, line);
      snprintf(line, sizeof(line), "Fresh %lu  age %lu ms",
               (unsigned long)freshGyroSamples,
               (unsigned long)((micros() - lastGoodSample) / 1000));
      ui::text(10, 59, line);
      ui::text(
          10, 74,
          String("Mouse notify: ") +
              (cccd ? (cccd->getNotifications() ? "ON" : "OFF") : "unknown"));
      snprintf(line, sizeof(line), "Sent attempts %lu  delta %d,%d",
               (unsigned long)mouseReports, mouseDx, mouseDy);
      ui::text(10, 89, line);
      snprintf(line, sizeof(line), "Screen %dx%d  rot %u", d.width(),
               d.height(), M5Cardputer.Display.getRotation());
      ui::text(10, 104, line, ui::muted);
      ui::text(10, 124, "Inputs stopped | Enter returns", ui::muted);
      return;
    }
    const char *modes[] = {"KEYBOARD", "MOUSE"};
    for (int i = 0; i < 2; i++) {
      int x = 8 + i * 114;
      bool chosen = menu.selected == i;
      auto color = chosen ? ui::accent : ui::muted;
      d.fillRoundRect(x, 28, 110, 52, 6, chosen ? ui::selected : ui::surface);
      d.drawRoundRect(x, 28, 110, 52, 6, chosen ? ui::accent : ui::border);
      if (i == 0)
        ui::keyboardIcon(x + 40, 37, color);
      else
        ui::mouseIcon(x + 46, 33, color);
      ui::text(x + (110 - strlen(modes[i]) * 12) / 2, 61, modes[i],
               chosen ? ui::white : ui::muted, 2);
    }
    const char *tools[] = {"SET", "PAIR", "MEDIA", "APPS"};
    for (int i = 0; i < 4; i++) {
      int x = 8 + i * 57;
      bool chosen = menu.selected == i + 2;
      d.fillRoundRect(x, 87, 53, 19, 4, chosen ? ui::selected : ui::surface);
      d.drawRoundRect(x, 87, 53, 19, 4, chosen ? ui::accent : ui::border);
      ui::text(x + (53 - strlen(tools[i]) * 6) / 2, 93, tools[i],
               chosen ? ui::white : ui::muted);
    }
    ui::text(8, 112, notice.substring(0, 37), motion.ready ? ui::green : ui::muted);
    ui::text(8, 124, "Arrows move  Enter select  G0 stop", ui::muted);
  } else {
    const char *mode = keyboardEnabled ? "KEYBOARD"
                                       : (armed ? "AIR MOUSE" : "PAUSED");
    ui::text(12, 32, mode, ui::white, 2);
    ui::text(12, 57,
             menu.awaitRelease ? "Release ALL keys to begin"
                               : "Ready for your input",
             menu.awaitRelease ? ui::amber : ui::green);
    ui::pill(12, 75, 100, keyboardEnabled ? "KEYS ON" : "KEYS OFF",
             keyboardEnabled ? ui::green : ui::muted);
    ui::pill(128, 75, 100, armed ? "MOUSE ON" : "MOUSE OFF",
             armed ? ui::green : ui::muted);
    ui::text(12, 98,
             String("Speed ") + String((int)motion.gain) +
                 (motion.ready ? "   Gyro ready" : "   Calibrate gyro"),
             motion.ready ? ui::muted : ui::amber);
    ui::text(12, 111,
             keyboardEnabled ? "Shift capitals  Fn symbols"
                             : "Arrows: click L/R, scroll U/D",
             ui::muted);
    ui::text(12, 124, "G0 or Fn+Tab = STOP + MENU", ui::accent);
  }
}
void paint() {
  if (!frameReady)
    return;
  renderFrame();
  // Compose completely in RAM; never clear the visible panel between frames.
  frame.pushSprite(0, 0);
}
void setup() {
  motion.gain = 30;
  auto cfg = M5.config();
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  M5Cardputer.begin(cfg, true);
  // Preserve the board driver's native orientation. Only rotate if dimensions
  // are portrait; never assume a rotation number means landscape.
  auto &display = M5Cardputer.Display;
  display.setRotation(air::landscapeRotation(
      display.getRotation(), display.width(), display.height()));
  const bool landscapeOK =
      air::correctLandscape(display.width(), display.height());
  frame.setColorDepth(16);
  frameReady = frame.createSprite(240, 135) != nullptr;
  validBoard = frameReady && M5.getBoard() == m5::board_t::board_M5CardputerADV &&
               M5.Imu.isEnabled() && landscapeOK;
  if (validBoard) {
    BLEDevice::init("Tiny Deck ADV Combo");
    auto security = new BLESecurity();
    security->setAuthenticationMode(ESP_LE_AUTH_BOND);
    security->setCapability(ESP_IO_CAP_NONE);
    security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    auto server = BLEDevice::createServer();
    server->setCallbacks(new Callbacks());
    hid = new BLEHIDDevice(server);
    report = hid->inputReport(1);
    keyboardInput = hid->inputReport(2);
    consumerInput = hid->inputReport(3);
    hid->outputReport(2); // Host keyboard LEDs; accepted, not displayed.
    hid->manufacturer()->setValue("Tiny Deck prototype");
    hid->hidInfo(0, 1);
    hid->reportMap(const_cast<uint8_t *>(descriptor), sizeof(descriptor));
    hid->startServices();
    advertising = server->getAdvertising();
    advertising->setAppearance(0x03C0);
    advertising->addServiceUUID(hid->hidService()->getUUID());
    advertising->setScanResponse(true);
  }
  if (!validBoard)
    notice = "ADV or IMU not detected";
  if (!frameReady) {
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setCursor(4, 4);
    display.print("UI memory unavailable");
  }
  paint();
}
void loop() {
  M5Cardputer.update();
  if (air::releaseDue(consumerHeld, millis(), consumerReleaseAt))
    releaseConsumer();
  const bool uiInteraction = menu.open && M5Cardputer.Keyboard.isChange();
  auto k = M5Cardputer.Keyboard.keysState();
  auto physical = [&](uint8_t code) {
    for (const auto &point : M5Cardputer.Keyboard.keyList())
      if (M5Cardputer.Keyboard.getKeyValue(point).value_first == code)
        return true;
    return false;
  };
  const auto arrowMouse =
      air::mouseKeys(armed, keyboardEnabled, k.fn, physical(','), physical('/'),
                     physical(';'), physical('.'));
  const bool leftClick = arrowMouse.left, rightClick = arrowMouse.right;
  const int scrollDirection = arrowMouse.scroll;
  const bool consumedArrow = arrowMouse.consumed;
  const bool localChord = leftClick || rightClick || consumedArrow;
  if (localChord) {
    suppressKeyboard = true;
    if (linked && lastKeyboard != air::KeyboardReport{})
      sendKeyboard({});
  } else if (M5Cardputer.Keyboard.keyList().empty())
    suppressKeyboard = false;
  const bool transition = linkChanged.exchange(false);
  if (transition) {
    openMenu();
    advertisingOn = false;
    if (advertising)
      advertising->stop();
  }
  if (advertisingOn && !linked &&
      static_cast<int32_t>(millis() - advertiseUntil) >= 0) {
    advertising->stop();
    advertisingOn = false;
  }
  bool stop = M5Cardputer.BtnA.isPressed() || (k.fn && physical(KEY_TAB));
  if (stop)
    openMenu();
  if (validBoard && !stop && !transition) {
    if (menu.open) {
      const bool up = physical(';');
      const bool down = physical('.');
      const bool left = physical(',');
      const bool right = physical('/');
      if (!calibrating) {
        int count = air::selectionCount(static_cast<int>(page));
        bool speed = page == Page::Settings && pageSelection == 1 && !helpOpen && !diagnosticsOpen;
        int delta = air::navigationDelta(up && !prevUp, down && !prevDown, left && !prevLeft, right && !prevRight, speed);
        if (delta && !helpOpen && !diagnosticsOpen) {
          if (page == Page::Home) menu.selected = air::homeSelection(menu.selected, up && !prevUp, down && !prevDown, left && !prevLeft, right && !prevRight);
          else pageSelection = air::nextSelection(pageSelection, delta, count);
        }
        if (speed) {
          if (left && !prevLeft) motion.gain = std::max(5.f, motion.gain - 5);
          if (right && !prevRight) motion.gain = std::min(100.f, motion.gain + 5);
        }
        if (k.enter && !prevEnter) {
          if (helpOpen || diagnosticsOpen) {
            helpOpen = false;
            diagnosticsOpen = false;
          } else if (page == Page::Media) {
            if (linked) { mediaPress(pageSelection); notice = "Media signal requested"; }
            else notice = "Choose Pair first";
          } else if (page == Page::Shortcuts) {
            notice = "Mac setup needed; no keys sent";
          } else if (page == Page::Settings) {
            if (pageSelection == 0) {
              pauseAll(); motion.calibrate(); calibrating = true;
              calibrationAfter = millis() + 1000;
            } else if (pageSelection == 1) {
              motion.gain = motion.gain >= 100 ? 5 : motion.gain + 5;
            } else if (pageSelection == 2) helpOpen = true;
            else diagnosticsOpen = true;
          } else if (menu.selected <= 1) {
            auto mode = static_cast<air::InputMode>(menu.selected);
            if (menu.activate(mode, linked, motion.ready)) {
              pauseAll();
              keyboardEnabled = mode != air::InputMode::Mouse;
              armed = mode != air::InputMode::Keyboard;
              motion.reset();
              notice = "Release keys";
            } else
              notice = linked ? "Choose Calibrate gyro first"
                              : "Choose Pair Bluetooth first";
          } else if (menu.selected == 2) {
            page = Page::Settings; pageSelection = 0; notice = "";
          } else if (menu.selected == 3) {
            if (linked)
              notice = "Already connected";
            else {
              advertising->start();
              advertisingOn = true;
              advertiseUntil = millis() + 120000;
              notice = "Pair Tiny Deck ADV Combo on Mac";
            }
          } else if (menu.selected == 4) {
            page = Page::Media; pageSelection = 0; notice = "";
          } else {
            page = Page::Shortcuts; pageSelection = 0; notice = "";
          }
        }
      }
      prevUp = up;
      prevDown = down;
      prevLeft = left;
      prevRight = right;
    }
    auto now = micros();
    if (now - lastSample >= 10000) {
      float dt = (now - lastGoodSample) * 1e-6f;
      lastSample = now;
      auto mask = M5.Imu.update();
      lastSensorMask = mask;
      auto data = M5.Imu.getImuData();
      float gx = data.gyro.x, gy = data.gyro.y, gz = data.gyro.z;
      gyroX = gx;
      gyroY = gy;
      gyroZ = gz;
      bool ok = (mask & m5::IMU_Class::sensor_mask_gyro) != 0;
      if (!ok) {
        // A missing data-ready bit is not itself a sensor failure. Do not reuse
        // stale gyro values; stop and require calibration after a 100 ms gap.
        if (now - lastGoodSample > 100000) {
          if (calibrating)
            notice = "No fresh gyro samples - Diagnostics";
          pauseCursor();
          motion.ready = false;
        }
      } else if (!std::isfinite(gx) || !std::isfinite(gy) ||
                 !std::isfinite(gz)) {
        notice = "Invalid gyro data - Diagnostics";
        pauseCursor();
        motion.ready = false;
        calibrating = false;
      } else {
        lastGoodSample = now;
        ++freshGyroSamples;
        if (calibrating) {
          if (static_cast<int32_t>(millis() - calibrationAfter) >= 0)
            motion.sample(-gz, -gx, gy);
          if (motion.samples() >= 200) {
            calibrating = false;
            notice = motion.ready ? "Calibrated! Choose a mouse mode"
                                  : "Moved too much; calibrate again";
          }
        } else if (armed && linked && menu.mayOutput()) {
          auto s = motion.step(-gz, -gx, dt);
          uint8_t buttons = (leftClick ? 1 : 0) | (rightClick ? 2 : 0);
          int wheel = 0;
          if (scrollDirection && (scrollDirection != lastScrollDirection ||
                                  millis() - lastScrollMs >= 120)) {
            wheel = scrollDirection;
            lastScrollMs = millis();
          }
          lastScrollDirection = scrollDirection;
          if (s.x || s.y || buttons != lastButtons || wheel)
            send(buttons, s.x, s.y, wheel);
        }
      }
    }
  }
  menu.released(M5Cardputer.Keyboard.keyList().empty() && !stop && !transition);
  if (linked) {
    air::KeyboardReport next{};
    if (keyboardEnabled && menu.mayOutput() && !suppressKeyboard &&
        !calibrating && !stop && !transition)
      next =
          k.fn ? air::fnSymbolReport(k.modifiers | (k.opt ? 8 : 0), k.hid_keys)
               : air::keyboardReport(k.modifiers | (k.opt ? 8 : 0), k.hid_keys);
    if (next != lastKeyboard)
      sendKeyboard(next);
  }
  prevEnter = k.enter;
  if (uiInteraction || transition || M5Cardputer.BtnA.wasPressed() ||
      millis() - lastPaint > 250) {
    lastPaint = millis();
    paint();
  }
  delay(1);
}
