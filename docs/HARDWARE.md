# Hardware and recovery

Target: M5Stack Cardputer ADV with ESP32-S3, built-in IMU, keyboard and 240×135
LCD. The firmware checks ADV board identity, IMU availability and landscape
geometry before HID initialization. No extra wiring or external screen required.

Official hardware reference: https://docs.m5stack.com/en/core/Cardputer-Adv

Before any write, identify the physical board, chip/flash size and selected port.
Preserve its current firmware and settings with a locally stored, verified full
backup or a known-good manufacturer recovery method. Backups can contain secrets
and Bluetooth bonds: never put them in a public repository. Do not bypass device
security restrictions or assume that USB vendor metadata identifies the board.

The development prototype used an 8MB Arduino default partition layout. An app
at0x10000 is appropriate ONLY if the actual installed layout is verified compatible.
This repository does not prescribe an unverified generic flashing command. Full
PlatformIO uploads can alter partitions and settings. Do not erase everything as
routine troubleshooting, and do not remove host/device bonds automatically.

Official ROM download entry: power off, hold G0, connect USB-C data cable, then
release G0 after power-on. Follow official instructions for the precise revision.
After a write, some USB reset paths may leave the device in download mode. Close
serial tools, unplug USB, release G0, power off/on on battery, then reconnect.

Post-install checklist on a safe desktop:
1. Landscape screen, clear selected item, stable redraw, no unexpected resets.
2. Physical-arrow navigation and G0 from each screen; no host input in menus
   except explicitly selected Media actions.
3. Explicit Pair, keyboard typing, Shift/Fn symbols and key release.
4. Still calibration, mouse movement, click/drag/release, scroll and G0 stop.
5. Disconnect/reconnect leaves input stopped; mode activation waits for release.
6. Each Media action, user-approved re-pair only if host report discovery needs it.
7. Battery/USB operation and acceptable responsiveness under sustained input.

User reports are not a substitute for all these checks. No production safety or
security certification is claimed.
