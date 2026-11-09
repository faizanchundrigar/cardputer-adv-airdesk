# Small-screen design decisions

Reference research (official sources, consulted for this revision):
- M5Stack Cardputer ADV: actual 1.14-inch 240×135 ST7789V2 display.
  https://docs.m5stack.com/en/core/Cardputer-Adv
- LVGL groups: one focused control, predictable keypad navigation and activation.
  https://lvgl.io/docs/open/main-modules/indev/groups
- LVGL scrolling: bring focused controls into view rather than compressing them.
  https://github.com/lvgl/lvgl/blob/master/docs/src/common-widget-features/scrolling.mdx
- M5Stack display API/example for board-specific display access.
  https://docs.m5stack.com/en/arduino/m5cardputer/display

Applied design (using lightweight M5GFX, not adding LVGL dependencies):
- Two large exclusive mode tiles, recognizable keyboard/mouse icons and 16px-high
  labels. Single cyan focus outline plus filled selection, not color alone.
- Settings/Media use 16px-high text; Media has three visible rows with automatic
  selection-following pages and a 1/2 or 2/2 indicator. No crowded six-item text.
- Header always identifies Bluetooth state in text; active screen names current
  mode and distinguishes input-enabled state from connection. G0 home/stop legend
  is consistent. No decorative blinking or animation that delays input.
- Menu key changes and G0 trigger immediate redraw requests; 250ms background
  refresh covers status/calibration. Offscreen composition eliminates deliberately
  visible clear/redraw intermediate frames. Actual LCD latency is unmeasured.
- RGB565 text/background contrast calculated at >=4.5:1 for all text pairs tested
  (muted minimum5.83:1). This is a software color calculation, not an accessibility
  certification or a measurement of the physical LCD/viewing conditions.
- Six layout previews use equivalent coordinates and font pixels with bounds
  assertions, then visual inspection. They are not hardware screenshots. Device
  reading comfort, flicker and responsiveness still require user's physical test.
