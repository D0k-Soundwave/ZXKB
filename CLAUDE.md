# ZXKB: project brief for Claude Code

ZXKB is a lightweight Windows tray app plus QMK firmware module for per-key RGB keyboards.
It began as "ZX VU" (an audio VU meter for D0k's ZX Spectrum keyboard) and grew into a general
lighting controller. It is headed for public release on GitHub (account `D0k-Soundwave`).

## Owner and working rules (read first)
- The owner is **D0k**. Always call him D0k. He directs; he does not write code himself.
- **Collect all requirements before building.** Do not build, flash or release until D0k says the
  list is complete. Record new requests in `docs/REQUIREMENTS.md` as they arrive.
- **No Claude attribution anywhere**: no co-author lines in commits, no credit in README or code.
- Licence: **GPL-3.0** for the whole repo (firmware builds on QMK, which is GPL).
- British English. Direct, no filler. One step at a time. GUI over CLI for anything D0k operates.
- Do not guess facts: check docs/online. Flag it if a plan is getting overcomplicated.
- Keep it lightweight: measure idle/busy RAM and CPU and report real figures.

## Hardware (reference board)
- Keychron Q3 UK ISO with knob: `keychron/q3/iso_encoder`, STM32L432, 2x SNLED27351 LED drivers,
  88 RGB LEDs, `CAPS_LOCK_LED_INDEX` 49. USB VID 0x3434 PID 0x0123. VIA raw HID: usage page 0xFF60, usage 0x61.
- Keycaps: KAM Wraith ZX Spectrum (grey + black sets mixed). Also used with the MiSTer ZX Spectrum core.
- Side switch: active = Windows layers (2/3), inactive ("Mac") = Spectrum layers (0/1).
- Flashing: QMK Toolbox (runs elevated). Hold **Esc while plugging in** = DFU + EEPROM clear.
  STM32 DFU 0483:DF11 with WinUSB driver.
- Build: upstream `qmk_firmware` master (in `%USERPROFILE%\qmk_firmware`) with QMK MSYS (`C:\QMK_MSYS`, the official
  Windows QMK environment; do not use WSL). `firmware/` is a QMK userspace: run `sh /h/ZXKB/firmware/build.sh`
  in the QMK MSYS shell; it sets `user.overlay_dir`, builds, and copies the .bin to `build/`. Nothing is copied
  into qmk_firmware. QMK MSYS also has g++ and make (MINGW64) for the app.

## Keyboard reference
- `docs/keyboard/keycaps.json|md|png`: which Wraith cap sits on every physical key (legends, cap colour, position).
- `docs/keyboard/layout.json|md`: physical layout, LED index (88 LEDs, Caps Lock 49), QMK keycode on all 4 layers, function in Windows / Spectrum / MiSTer core / Fn. Pictures: `windows.png`, `fn.png`, `spectrum.png`.
- Regenerate after keymap changes: `python tools/gen_keyboard_docs.py firmware/keymaps/d0k/keymap.c docs/keyboard` (HTML pictures; render to PNG with Playwright).

## Colour calibration (done 2026-11-02, by eye against the true Spectrum BRIGHT palette)
- Root cause found: Keychron ships SNLED27351 current tune red 0x44 (68), green/blue 0x9D (157),
  which starves red: no real white or yellow. Tune register i drives CB(i+1); on this board
  CB1/4/7/10 = green, CB2/5/8/11 = blue, CB3/6/9/12 = red.
- D0k's hardware balance: **red 192, green 72, blue 58** (live on the keyboard, firmware default in source).
- With that, every colour matches with its pure palette mix; only brightness differs:
  red 204,0,0 · yellow 255,255,0 · green 0,205,0 · cyan 0,205,205 · blue 0,0,170 · magenta 200,0,200 · white 195,195,195.
- Raw values: `calibration/ZX_colours.json`. Tool: `tools/zx_colour_cal.py` (to be folded into ZXKB).

## Firmware: current state
- `firmware/modules/d0k/zxkb`: ZXKB QMK community module (any QMK RGB matrix board). Effects
  (`rgb_matrix_module.inc`): SPECTRUM_STRIPES, TAPE_LOADING, VU_METER (3 thick bars, left-to-right
  red|yellow|green|cyan blocks), BORDER_FLASH, ATTR_CLASH, TYPING_RIPPLE, LOGO_SWEEP, KEY_EXPLOSION, PC_DIRECT.
  Keycodes ZK_1..ZK_0 (hotkey slots), ZK_VU (VU toggle). Hooks `via_command_kb` (or `raw_hid_receive` without VIA).
- `firmware/keyboards/keychron/q3/iso_encoder/keymaps/d0k`: Q3 keymap using the module. Layers SPEC_BASE, SPEC_FN,
  WIN_BASE, WIN_FN. Spectrum layer maps for the MiSTer core (Shift = Caps Shift, Ctrl = Symbol Shift,
  ZX_EXT = Ctrl+LShift, ZX_LOAD types LOAD "" with 60 ms per key). Fn layers: media/brightness on F-row, RM_*
  lighting keys, Fn+1..0 = hotkey slots, Fn+V = switch to/from the VU meter effect, Fn+N = NKRO, Win lock.
- Auto Spectrum stripes when the switch selects Spectrum mode (previous effect restored after). The VU meter is a
  normal effect fed by ZXKB (dark without it); PC direct mode is an overlay, only in Windows mode.
- Caps Lock: flashing magenta/white every 320 ms (Spectrum FLASH rate).
- Raw HID (32-byte reports, `zxkb.h`):
  - `0xD0` + 16 levels: VU data. `0xD1` + 0/1: VU mode request. `0xD2` + n (1-6): Spectrum effect (old app).
  - `0xD3` + r,g,b / `0xD4` / `0xD5`: LED driver current tune set / save / get (keymap, replies).
  - `0xD8` + slot: keyboard -> app, Fn+number pressed.
  - `0xD9` 0x00 status (protocol 2: also sent unasked on changes; byte 15 VU possible, byte 16 firmware version),
    0x01 + mode -> name, 0x02 + first LED -> 9 x (x, y, flags).
  - `0xDA` size and intensity of the current effect: 0x00 get, 0x01 set, 0x02 save.
  - SignalRGB 0x21-0x28 (protocol 1.0.6); 0x25 enable / 0x24 stream / 0x26 disable is also ZXKB's direct mode.
  - Standard VIA lighting (0x07/0x08/0x09, channel 3) for effect, brightness, speed, colour, save.
  - Overlays drop back to the board's own effect after 5 s without data.
- Versions: see `docs/VERSIONS.md`. Every build handed to D0k gets a number (`ZXKB_FIRMWARE_VERSION` in the
  keymap config.h, `ZXKB_VERSION_STR` in app/zxkb/res/resource.h) and is kept for fallback via
  `sh firmware/build.sh release` and `pwsh tools/install_app.ps1`. Never reuse a released number.
- The Q3 can hold at most 63 effects (QMK 6-bit mode field); it has 59.

## App: current state
- `app/zxkb`: ZXKB, native C++ (Win32 only, no runtime): `make -C /h/ZXKB/app/zxkb` in QMK MSYS -> `build/ZXKB.exe`.
  `src/hidlink` (find ZXKB boards by raw HID + 0xD9 probe, effect names, LED positions, reconnect),
  `src/vumath` (VU maths ported from zx_vu.py; `test/run_vu_test.sh` compares it with the real legacy code:
  worst difference 0), `src/vu` (WASAPI loopback, only while the VU is on), `src/settings` (`%APPDATA%\ZXKB\zxkb.ini`,
  `profiles\*.ini`, Startup-folder shortcut), `src/effects` (menu names and groups), `src/main` (tray and menus),
  `src/window` + `src/preview` (the ZXKB window and its live preview). The window is a second copy of ZXKB.exe
  (`ZXKB.exe --window`, started by the tray on double-click) with its own keyboard connection; the tray copy alone
  feeds the VU meter and answers Fn + number; each tells the other (message "ZXKB.settings") to reread settings.
  `src/flash`: Firmware... dialog (bootloader command 0xDC, then `dfu-util -a 0 -d 0483:DF11 -s 0x08000000:leave -D`;
  dfu-util.exe sits next to ZXKB.exe, copied from QMK MSYS by tools/install_app.ps1; log in %APPDATA%\ZXKB\firmware.log).
  `src/boards` + `src/json` + `src/layout`: plain VIA keyboards (Board::VIA): VIA definition from
  usevia.app/definitions/v3/<vid*65536+pid>.json (asked first, kept in %APPDATA%\ZXKB\boards), effect names and
  layout from it. Test switch: environment ZXKB_TEST_VIA=1 makes the app treat a ZXKB keyboard as plain VIA.
  `src/labels` (key labels: QMK keycode -> HID usage -> scan code -> Windows layout), `src/keycaps` (KLE keycap sets in
  <exe>\keycaps and %APPDATA%\ZXKB\keycaps, choice per USB ID in zxkb.ini [keycaps]). The Wraith set is
  app/zxkb/keycaps/*.json, written by `python tools/gen_wraith_kle.py` (matrix positions from QMK's Q3 keyboard.json).
  Keychron's own firmware source: %USERPROFILE%\keychron_qmk (branch 2025q3), `make keychron/q3/iso_encoder:keychron`.
  Build from PowerShell with TMP set to a writable folder (g++ otherwise tries C:\WINDOWS for temp files).
- Installed copy: `Documents\Keyboard Layouts\ZXKB\ZXKB.exe`, started with Windows by a shortcut in the Startup
  folder (`shell:startup\ZXKB.lnk`), switched from ZX VU on 2026-11-02. Copy a new build there after quitting ZXKB.
- Do not use the HKCU Run key: on 2026-11-02 Microsoft Defender quarantined ZXKB.exe as
  `Behavior:Win32/Persistence.A!ml` (new unsigned exe + Run key) and removed the key. The Startup shortcut ran
  clean (no detection over 2 min). Proper fix at release: code signing and/or a false-positive report to Microsoft.
- `app/legacy/zx_vu.py`: the old ZX VU tray app (Python; PyInstaller onefile `ZX_VU.exe`; settings in
  `%APPDATA%\ZX_VU.json`). Its exe is still in `Documents\Keyboard Layouts\ZX VU\` but no longer starts with Windows.
- VU tunables D0k approved: FPS 40, RANGE_DB 30, HEADROOM_DB 9, CURVE 1.7, FALL 0.72, reference = moving average.
- Measure with `python tools/measure.py ZXKB.exe 60` (private working set is the honest RAM figure).

## Target design (agreed)
See `docs/REQUIREMENTS.md` for the full checklist and status. Architecture:
- Always-running core (tray, HID link, active effect, profiles/playlists/scheduler, OpenRGB SDK
  server) kept near today's ~62 MB RAM idle.
- Windows (preview, settings, paint editor, gradient builder, calibration) open on demand and are
  fully released on close.
- Costly sources only while used: screen capture (DXGI duplication), audio analysis, WebView2 for
  SignalRGB Lightscripts.
- Keyboard-independent: ZXKB firmware packaged as a QMK **community module**; the keyboard reports
  its LED positions to ZXKB. Also speaks SignalRGB / OpenRGB firmware protocols for boards that have them.
  Stock-VIA-only support (built-in effects via VIA custom values) possibly later.
- Build in stages, sending each to D0k to try: (1) firmware + core app with effects, profiles,
  hotkeys; (2) editors + preview; (3) drop-ins, Lightscripts, SDK server.
