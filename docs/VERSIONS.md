# ZXKB versions

Every firmware and app build handed to D0k gets a number. Released builds are kept for fallback:

- Firmware: `Documents\Keyboard Layouts\Q3 firmware\ZXKB versions\ZXKB_Q3_v<N>.bin` (flash with QMK Toolbox).
- App: `Documents\Keyboard Layouts\ZXKB\versions\ZXKB_v<version>.exe` (quit ZXKB, copy over
  `Documents\Keyboard Layouts\ZXKB\ZXKB.exe`, start it again).
- Git tags `fw-v<N>` and `app-<version>` mark the source of each.

How to release: raise `ZXKB_FIRMWARE_VERSION` (keymap `config.h`) or `ZXKB_VERSION_STR` (`app/zxkb/res/resource.h`),
then `sh firmware/build.sh release` / `pwsh tools/install_app.ps1`. A kept number is never reused: both refuse to
replace a kept version with different contents. App builds are reproducible from 0.8 on (no build time in the exe); the kept 0.7 contains its build time, so a
rebuild of 0.7 differs from it in that stamp only.

Versioning started on 2026-11-02 with firmware v15 and app 0.7. Earlier builds were not kept; their source is in
git (firmware v11 a50ac01, v12 0836394, v13 a45b141, v14 7f249cd; app 0.1 09277f8 to 0.6 8aaae7b).

## Firmware (Keychron Q3 ISO knob)

| Version | Date | Size | Changes |
|---|---|---|---|
| v17 | 2026-11-03 | 60,688 bytes | Restarts into its bootloader when ZXKB asks (0xDC, settings kept), so ZXKB can install firmware without QMK Toolbox or holding Esc. Reports its bootloader type. LED mirror sends only LEDs that changed (protocol 4). |
| v16 | 2026-11-03 | 60,496 bytes | LED mirror: the keyboard sends the colours actually on its LEDs (after effects, brightness and Caps Lock) while the ZXKB window asks for them, for the live preview (protocol 3). |
| v15 | 2026-11-02 | 60,184 bytes | VU meter is a normal effect (picked and saved like any other); Fn + V switches between it and the previous effect. Status sent to ZXKB unasked on changes and reports "VU possible" and the firmware version (protocol 2), so ZXKB listens to audio only while the VU meter shows. |
| v14 | 2026-11-02 | 59,984 bytes | VU bar end key lights in proportion to how much the bar covers it. Not kept. |
| v13 | 2026-11-02 | 59,704 bytes | Size and intensity for typing effects; ZXKB versions of QMK's typing effects. Not kept. |
| v12 | 2026-11-02 | 59,456 bytes | ZXKB community module: PC direct mode, SignalRGB protocol, Fn + number slots, Key explosion. Not kept. |
| v11 | 2026-11-02 | 56,596 bytes | Last firmware before ZXKB (calibrated colours, flashing Caps Lock). Not kept. |

## Other firmware kept for testing

| File | Date | Size | What |
|---|---|---|---|
| Keychron_Q3_ISO_knob_standard_2025q3.bin | 2026-11-03 | 61,192 bytes | Keychron's own Q3 ISO knob firmware (keymap `keychron`), built from github.com/Keychron/qmk_firmware branch 2025q3 (shallow clone in `%USERPROFILE%\keychron_qmk`). Standard VIA firmware for testing ZXKB with keyboards that do not run ZXKB. Keychron's LED colour balance (weak red). |

## App (ZXKB.exe)

| Version | Date | Size | Changes |
|---|---|---|---|
| 0.13 | 2026-11-03 | 485,888 bytes | Plain VIA keyboards (standard firmware): recognised, lighting controlled through VIA, effect names and key layout from the keyboard's VIA definition, looked up online once by USB ID after asking. Firmware check by USB ID instead of USB name (so Keychron's own firmware for the same keyboard is accepted, other keyboards' refused); honest warning that non-ZXKB firmware may reset settings. |
| 0.12 | 2026-11-03 | 433,664 bytes | Firmware install: clearer message when the keyboard does not restart by itself after writing (unplug and replug, firmware is already written); firmware.log records the time of each phase. |
| 0.11 | 2026-11-03 | 433,152 bytes | Firmware... button in the window: lists the kept firmware files, checks a file is for this keyboard, restarts the keyboard into its bootloader (v17 on; older firmware: unplug and hold Esc) and writes it with dfu-util, with progress, then waits for the keyboard to come back. dfu-util.exe installed next to ZXKB.exe. |
| 0.10 | 2026-11-03 | 419,328 bytes | Live preview made light: keycaps and legends drawn once, each frame only paints the lights (36% -> 2.2% of one core while animating); frames with no colour change are skipped. Version number in the file properties fixed (0.9 said 0.8 there). |
| 0.9 | 2026-11-03 | 417,792 bytes | The ZXKB window runs as its own copy of ZXKB.exe, started by the tray, so closing it gives all its memory back. Live preview of the keyboard with its keycaps, lit as the keyboard is (needs firmware v16). Favourites, hotkeys and VU style changed in one are picked up by the other. |
| 0.8 | 2026-11-02 | 400,384 bytes | The ZXKB window (Stage 2, step 1): double-click the tray icon, or "Open ZXKB window" in the menu. Effect list in groups with the VU styles, lighting on/off, favourite, sliders for brightness, speed, size and intensity, Spectrum colours and a full colour picker. Opens next to the tray, follows the keyboard, freed on close. |
| 0.7 | 2026-11-02 | 390,144 bytes | VU meter styles listed with the effects and chosen like one; audio captured only while the VU meter effect shows; left click on the tray icon does nothing; version shown in the menu. |
| 0.1-0.6 | 2026-11-02 | | First builds (tray, effects, profiles, Fn slots, VU port, size/intensity, music style). Not kept. |
