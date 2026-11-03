# ZXKB requirements

Status: `[x]` done · `[~]` in source, not yet built/flashed · `[ ]` to do · `[?]` decision pending.
Rule: do not build until D0k confirms the list is complete.

## Across the board
- [ ] As small and lightweight as possible everywhere: app RAM, CPU, disk/download size, and firmware
      flash/RAM (requested 2026-11-02). Measure and report real figures at every stage.
- [~] App core in native C++ (Win32 only, no runtime), decided 2026-11-02 over slimmed Python for idle weight

## Firmware
- [~] PC direct mode: ZXKB streams a colour per LED (for drop-ins, PC effects, OpenRGB SDK server)
- [~] SignalRGB protocol module included (so SignalRGB can be tried without reflashing)
- [~] Key explosion effect: pressed key fires a small star that spreads, changes colour and fades to black
- [~] Fn+1 to Fn+0 report to ZXKB as hotkey slots
- [~] Package as a QMK community module so any QMK keyboard can add it; board reports LED positions
- [x] Calibrated Spectrum colours in all Spectrum effects and the VU meter (v11)
- [x] Hardware balance red 192 / green 72 / blue 58 as firmware default (v11)
- [x] Caps Lock flashing magenta/white (320 ms) (v11)
- [x] Live LED current tune commands 0xD3/0xD4/0xD5 (v10)
- [x] Spectrum effects: stripes, tape loading, VU, border flash, attribute clash, typing ripple, logo sweep (v9/v10)

## ZXKB app (renamed from ZX VU)
- [~] Every effect by name in the tray: QMK built-ins, Spectrum (done); PC-driven, drop-ins (later stages)
- [~] Effect settings (speed, Spectrum colours) and favourites in the tray; a settings panel window is Stage 2
- [~] Double-click the tray icon to open the ZXKB window next to the tray (step 1 in 0.8: effects and settings), so settings can be adjusted without
      right-clicking every time (requested 2026-11-02); opened on demand and fully released on close.
      It is a full RGB keyboard tool, not a cut-down panel: everything the keyboard and ZXKB can do is in it
      (clarified 2026-11-02). It runs as its own small program, started on double-click, so closing it gives
      every byte back; the tray core stays at ~2 MB (decided 2026-11-03: in-process, ~8 MB stayed after closing)
- [ ] Layers: stack effects (e.g. rainbow base, typing reactions on top, fixed colours on chosen keys above)
      (2026-11-02; needs firmware work)
- [ ] Per-app profiles: switch profile when a given program or game is in front, and back after (2026-11-02)
- [ ] Effects on zones: an effect on part of the board only, another elsewhere (2026-11-02; needs firmware work)
- [ ] More effect controls where an effect allows: direction, two or more colours, full colour picker (2026-11-02)
- [ ] Effect intensity settings, e.g. how large and how bright typing effects are (requested 2026-11-02);
      scope pending: ZXKB's own effects only, or ZXKB versions of QMK's typing effects too
- [~] Overall brightness control: steps (Brighter / Dimmer), decided 2026-11-02
- [~] Profiles: save/load whole setup by name
- [ ] Playlists: cycle effects on a timer
- [ ] Scheduler: switch profiles at set times
- [~] Fn+number hotkey slots, each assignable to an effect or profile
- [ ] Live preview window with the Wraith legends
- [ ] Per-key paint editor
- [ ] Gradient builder (multi-point colour waves)
- [ ] Drop-in effects: ZXKB Python format in an `Effects` folder
- [ ] Drop-in effects: SignalRGB Lightscripts (HTML canvas via WebView2; `<meta>` properties become settings;
      audio visualisers fed by ZXKB audio; game integrations not at first)
- [ ] PC-streamed effects (effects worked out by ZXKB and streamed live through direct mode): Stage 2,
      decided 2026-11-02. Direct mode measured at 91 frames/s on the Q3.
- [~] VU: third style for ordinary music (treble / mids / bass, each bar with its own level), requested
      2026-11-02: AY channels style mirrors its top two bars on normal stereo music (measured)
- [x] VU: edge key of a bar lights in proportion (v14). The two rows of a bar still end at different points
      because the keys are staggered; D0k accepted this on 2026-11-02
- [~] Built-in PC effects: VU meter (bands + ACB) ported, identical output; screen ambient, system load (CPU/RAM/GPU), beat pulse,
      notification flash
- [ ] OpenRGB SDK server
- [ ] Calibration tool built in, with reset to defaults (current tool: `tools/zx_colour_cal.py`)
- [ ] Keyboard-independent: VIA-based detection; supports boards with the ZXKB module, SignalRGB or OpenRGB firmware
- [ ] Starts with Windows (tray), lightweight; report measured idle/busy RAM and CPU

## Keyboard functions beyond lighting (added 2026-11-03, from the commercial-suite feature list)
Mechanical switches only. Most use QMK's VIA support already in the firmware; firmware work is marked.
- [ ] Remap any key: key, shortcut, mouse action, media, brightness, layer or profile switch; disable a key
- [ ] Game-mode lock: Win key (exists) plus blocking Alt+Tab / Alt+F4 (firmware work)
- [ ] App-only actions on any key: launch app or URL, paste text, timer (work only while ZXKB runs)
- [ ] Macros: record in the app, edit keys and delays, stored on the keyboard (work without the app);
      on-the-fly recording by a key; play once / repeat while held / toggle / sequence (firmware work);
      library with import and export
- [ ] Hold-a-key second layer (QMK layers; each extra layer uses 192 bytes of the keyboard's settings memory)
- [ ] Profiles on the keyboard (work without the app): lighting profiles cheap; whole keymaps need the settings
      memory enlarged (2,048 bytes now; flash is 77% free); profile-cycle key
- [ ] Profile import, export and backup
- [ ] Indicator colours: Num Lock, active layer or profile (Caps Lock done)
- [ ] Lights-off after N minutes without a key press, set from the app
- [ ] GPU temperature lighting via NVIDIA / AMD libraries (CPU temperature not done: needs a kernel driver that
      Defender flags)
- [ ] Game event reactions, per game, for games that publish their state (e.g. CS2, Dota 2): later
- [ ] Sync with Hue and Nanoleaf (their local network APIs): later
- [ ] SOCD (last opposing key wins): QMK community module (port of getreuer's SOCD Cleaner)
- [ ] Debounce time adjustable from the app (QMK fixes it at 5 ms; firmware work)
- [ ] NKRO toggle and Windows key repeat delay / rate in the app
- [ ] Firmware updates from the app: keyboard enters its bootloader on command, flashed with dfu-util from the
      kept versions (replaces QMK Toolbox and holding Esc)
- [ ] Knob assignment (VIA encoder commands)
- [ ] On-screen notice when the profile changes
- Not doing: CPU temperature (driver), faster polling (already 1,000 Hz, the USB full-speed limit), OLED (Q3 has none)

## Other keyboards (added 2026-11-03)
- [ ] Find out what a keyboard can do when it is plugged in, and offer only that:
      - ZXKB module boards: everything; the firmware reports its features and physical layout
      - Vial boards: the keyboard hands over its own layout; keymap, macros, VIA lighting
      - VIA boards: keymap, macros, knob, basic lighting; layout from VIA's public definitions or a file
      - Boards with OpenRGB or SignalRGB QMK firmware: lighting only
      - Other brands (Razer, Corsair, Logitech...): only through the OpenRGB app, later

## Process
- [x] Number every firmware and app build handed to D0k and keep each for fallback (docs/VERSIONS.md), from
      firmware v15 / app 0.7 on (requested 2026-11-02; earlier builds not kept, by D0k's choice)
- [x] VU meter is a normal effect, not an override; tray left click does nothing (v15 / 0.7, tested 2026-11-02)

## Release
- [ ] GitHub repo under D0k-Soundwave, GPL-3.0, no Claude attribution
- [ ] Private for now (decided 2026-11-02); make public at first release
- [ ] Microsoft Defender flags new unsigned ZXKB builds that add themselves to startup (2026-11-02): decide on
      code signing and/or a false-positive report to Microsoft before release
- [ ] README and user manual (existing Q3 keyboard manual doc to be updated for the custom firmware)
