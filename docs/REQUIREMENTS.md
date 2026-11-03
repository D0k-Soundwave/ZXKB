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
- [~] Double-click the tray icon to open the ZXKB window next to the tray (step 1 in 0.8: effects and settings; 0.9: own program, live preview), so settings can be adjusted without
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
- [~] Per-key effects (D0k, 2026-11-03; built in v20 + 0.21; flash on a chosen key tried by D0k with 0.22, works): each key its own effect over its colour, run by the keyboard: steady, flash
      between two colours (in step with each other, Spectrum FLASH rate 320 ms by default), breathe, colour cycle,
      twinkle; own colours and speed per key. Planned cost: 7 bytes per key in the keyboard's settings memory (616 on the
      Q3, up from 275), taken from the space for macros stored on the keyboard; first start of that firmware resets the
      keymap, macros and key colours. ZXKB firmware only at first
- [x] Clear gives every key black as its own colour, lighting stays on (D0k, 2026-11-03; done in 0.19)
- [x] Black in the colour boxes, to turn single keys off (D0k, 2026-11-03; done in 0.20)
- [ ] LED mirror sends a frame only when a colour changed (firmware): a still keyboard then costs nothing while the
      window is open (0.47% of one core measured 2026-11-03, from 300 unchanged packets/s reaching both copies)
- [~] Per-key colour on/off in the app on an ongoing basis (v19 + 0.17: select keys in the preview; ZXKB firmware only,
      other keyboards through streaming later): any key can be set to no colour (dark) or a colour,
      over any effect, while indicators such as Caps Lock keep their own behaviour (D0k, 2026-11-03). Until then,
      keys-off is solid colour at brightness 0 (Caps Lock ignores brightness), set 2026-11-03
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
- [~] Firmware updates from the app: keyboard enters its bootloader on command, flashed with dfu-util from the
      kept versions (replaces QMK Toolbox and holding Esc). Built in 0.11 + v17 (STM32 bootloader only so far); tested
      by D0k 2026-11-03: both installs written, one needed a replug to start
- [ ] Knob assignment (VIA encoder commands)
- [ ] On-screen notice when the profile changes
- Not doing: CPU temperature (driver), faster polling (already 1,000 Hz, the USB full-speed limit), OLED (Q3 has none)

## Other keyboards (added 2026-11-03)
- [ ] Design rule (D0k, 2026-11-03): every function usable on any QMK keyboard where possible. Each runs on the
      keyboard (ZXKB module), streamed from the app (boards with a direct protocol: ZXKB, OpenRGB or SignalRGB
      firmware), or as a standard-firmware version (VIA), whichever the board allows; the window says which
- [ ] ZX effects (and Key explosion, typing effects, VU bars) also as app effects streamed through direct mode, so
      any keyboard with a direct protocol gets them (D0k, 2026-11-03). On standard VIA firmware only whole-keyboard
      versions are possible (Border flash, Tape loading flicker). Downsides: they stop when ZXKB is not running
      (e.g. on the MiSTer), small PC cost (under 1% of one core, A), key reactions later by Windows + USB time
      (10-30 ms, A). Decided by D0k 2026-11-03: app only. The ZXKB firmware drops its own ZX effects, Key
      explosion and ZX typing effects once the streamed versions work (step 8), freeing flash and effect slots.
      Consequence: with no PC (e.g. on the MiSTer) the keyboard shows only QMK's own effects; the automatic Spectrum
      stripes in Spectrum mode go with them
- [ ] Find out what a keyboard can do when it is plugged in, and offer only that:
      - ZXKB module boards: everything; the firmware reports its features and physical layout
      - Vial boards: the keyboard hands over its own layout; keymap, macros, VIA lighting. Vial supports the Q3 ISO knob
        (vial-qmk keyboards/keychron/q3/iso_encoder/keymaps/vial, checked 2026-11-03), so it can be tested on D0k's Q3
      - VIA boards: keymap, macros, knob, basic lighting; layout from VIA's public definitions or a file
      - Boards with OpenRGB or SignalRGB QMK firmware: lighting only
      - Other brands (Razer, Corsair, Logitech...): only through the OpenRGB app, later
- [ ] Layouts for keyboards ZXKB does not already know, looked up once by USB vendor/product ID (asking first,
      as it goes online) and kept, from:
      - QMK's keyboard database, keyboards.qmk.fm (key positions and sizes, matrix, RGB LED positions, USB IDs;
        checked 2026-11-03 with the Q3's entry). Searchable by USB ID: keyboards.qmk.fm/v1/usb.json (341 KB, 786
        vendor IDs, 3,013 product IDs, M 2026-11-03); 259 product IDs belong to more than one keyboard, so the user
        picks when an ID is shared
      - VIA's keyboard definitions, github.com/the-via/keyboards (GPL-3.0; JSON per keyboard in v3/) for
        VIA-only boards
- [~] Independent of keycaps (0.15: labels from the keymap read through VIA, Windows layout; follows a language
      switch): by default keys are labelled from what they do, in the user's chosen language
      (any Windows keyboard layout, e.g. UK, US, German, French), worked out on the PC with no database.
      Checked 2026-11-03 on D0k's PC: Windows gives the character for every key in each installed layout
      (English UK with a US-style layout), and can follow a language switch. It does not know the physical shape
      (ISO/ANSI, key sizes): that comes from the keyboard or the QMK/VIA databases. The labels follow the layout,
      not the language: on D0k's PC English (UK) uses his custom layout "US - Spectrum (GBP on AltGr+4)"
      (USGBP.dll, KLID a0010809), so the labels are US ones (found 2026-11-03); the window names the layout
- [~] Keycap sets as an optional extra (0.15: KLE import, Wraith set shipped as a file, choice per keyboard): the user picks or defines the caps on their keyboard (legends, cap and
      legend colours). Import keycap sets from KLE files (keyboard-layout-editor, the common format for layouts
      and keycap designs). No keycap legend database was found (searched 2026-11-03). D0k's KAM Wraith set
      (docs/keyboard/keycaps.json) becomes the first such set rather than built in

- [ ] Works with any QMK keyboard, not just the Q3 (D0k, 2026-11-03). No design may rest on one board's limits:
      effect count, free effect slots, settings memory and flash differ per board, and the module build can drop
      QMK effects to make room. The module reports each board's limits and features; the app adapts to them.
      Where a board cannot do something itself (no ZXKB module, too little memory), the app does it from the PC
      instead (e.g. layers and zones streamed through direct mode) and says so

- [ ] Standard QMK + VIA keyboards without reflashing (D0k, 2026-11-03). Through VIA: remap, macros, layers, knob,
      the board's own effects, brightness, speed, colour. From the app: profiles, per-app profiles, playlists,
      scheduler, and whole-keyboard PC effects (one colour or brightness for the whole board: music pulse, screen
      colour, notification flash). Checked in QMK source (via.c, 2026-11-03): VIA's live lighting changes are not
      written to the keyboard's settings memory, so streaming them does not wear its flash. Rate it accepts: not
      measured. Per-key colour, the VU bars and ZXKB's own effects still need ZXKB, OpenRGB or SignalRGB firmware
- [ ] Standard firmware keeps the maker's LED colour balance (Q3: red 68, green/blue 157, so red is weak and white is
      not true white); VIA cannot change it, only ZXKB firmware can (D0k, 2026-11-03). The window says so on boards
      whose balance ZXKB knows is off
- [x] Standard Keychron firmware on the Q3 tested by D0k 2026-11-03 (0.13): switch, lookup, effects, back to v17 all worked
- [ ] Simulated preview for keyboards without the ZXKB firmware: the app recreates QMK's standard effects from the
      effect, speed, colour and brightness (read through VIA) and the LED positions (VIA/QMK definitions); typing
      effects from Windows key presses. Marked "simulated" in the window. Effects a maker added cannot be simulated
      (shown as plain keys). Only while the window is open
- [ ] Fn + number hotkeys without the ZXKB firmware: with the user's OK the app sets Fn+1..0 to F13-F22 through VIA
      (old keys saved first, undo in one click) and registers them as Windows hotkeys (no polling); Ctrl+Alt+Shift+
      number as the alternative for software that uses F13-F24. Only while ZXKB runs
- [ ] Keychron's own firmware per-key lighting (checked 2026-11-03 in github.com/Keychron/qmk_firmware, branch
      2025q3): raw HID prefix 0xA8 has per-key colour get/set (HSV, 9 LEDs per packet, kept in RAM until an RGB save
      command), plus per-key and mixed-region modes. Enabled (KEYCHRON_RGB_ENABLE) on 32 of 90 Keychron board
      families, e.g. Q1 v2, Q5, Q6, V1-V7 Max, the HE boards; not on the original Q3. Not yet known: whether the
      firmware shipped on those keyboards matches this source, and how fast it takes updates. Undocumented by
      Keychron, so it may change. If it works, those boards get per-key colour and streamed effects without reflashing

## Process
- [x] Number every firmware and app build handed to D0k and keep each for fallback (docs/VERSIONS.md), from
      firmware v15 / app 0.7 on (requested 2026-11-02; earlier builds not kept, by D0k's choice)
- [x] VU meter is a normal effect, not an override; tray left click does nothing (v15 / 0.7, tested 2026-11-02)

## Release
- [ ] GitHub repo under D0k-Soundwave, GPL-3.0, no Claude attribution
- [ ] Private for now (decided 2026-11-02); make public at first release
- [ ] Microsoft Defender flags new unsigned ZXKB builds that add themselves to startup (2026-11-02): decide on
      code signing and/or a false-positive report to Microsoft before release
- [ ] dfu-util ships with ZXKB (GPL-2.0-or-later): include its licence and a link to its source in the release
- [ ] WinUSB driver for the STM32 bootloader on a fresh PC: check what Windows does on its own, and how to install it
      if needed (D0k's PC has it from QMK Toolbox)
- [ ] README and user manual (existing Q3 keyboard manual doc to be updated for the custom firmware)
