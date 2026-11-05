# ZXKB Stage 2 plan

Written 2026-11-03, after D0k confirmed the requirements list complete. Replaces the first Stage 2 plan
(window, preview, editors). Every step ends with numbered builds handed to D0k to try, kept for fallback
(docs/VERSIONS.md), with measured RAM and CPU. D0k flashes; nothing is flashed for him.

## Rules every step follows
- Any QMK keyboard, not the Q3. No design rests on one board's limits; the keyboard reports what it has.
- Tray core: 0.000% CPU at idle (anything above that means polling), RAM measured at each release.
- Window features cost nothing once the window is closed. Costly sources run only while chosen.
- A feature a board cannot do itself is done from the PC where possible, and the window says so.

## Steps

1. **Window, step 1** - done (app 0.8).

2. **Window as its own program, and the live preview** - done (firmware v16, app 0.9 and 0.10)
   - Same ZXKB.exe started a second time in window mode, so there is still one file to download.
   - The tray stays the only program talking to the keyboard; the window asks it through Windows messages.
   - Live preview with LED mirror (built, not released). Measure preview CPU at 30 frames/s.
   - Check: closing the window gives all its memory back; tray RAM unchanged.

3. **Firmware updates from the app** - done (firmware v17, app 0.11 and 0.12)
   - Done early because every later step needs flashing: no more QMK Toolbox or holding Esc.
   - The app sends the keyboard into its bootloader and flashes a kept version with dfu-util (GPL, shipped
     alongside ZXKB.exe). Choose any kept version to go back.
   - Downside: the bootloader needs the WinUSB driver. D0k's PC has it from QMK Toolbox; what a fresh PC
     needs is not yet checked and will be before release.

4. **Any keyboard: capability detection, layouts, labels** - done (firmware v18, app 0.13-0.30; Vial and Keychron
   per-key commands not tried: no such keyboard here) (firmware v17, app 0.12)
   - The module reports its limits and features: effects present, free effect slots, settings memory size,
     layers, matrix size, knob, and the physical layout (added by the build from QMK's own data).
   - The app recognises ZXKB module, Vial, VIA, OpenRGB-firmware and SignalRGB-firmware boards and offers only
     what each can do.
   - Unknown boards: layout looked up once by USB ID from QMK's database or VIA's definitions (asks first, as
     it goes online), then kept on disk.
   - Key labels from the chosen Windows keyboard language. Keycap sets as an extra: KLE import; D0k's Wraith set
     becomes the first set instead of being built in.
   - LED mirror sends only changed frames (still keyboard costs nothing while the window is open).
   - Standard QMK + VIA keyboards without reflashing: keyboard functions, the board's own effects, whole-keyboard
     PC effects, simulated preview, Fn + number as F13-F22 hotkeys. Tested on the Q3 with standard firmware.
   - Downside: real tests only on keyboards D0k owns. Other boards are checked by building the module for a few
     of them (no flashing) and by reading their published definitions.

5. **Keyboard functions** (firmware v18, app 0.13), mostly through VIA, already in the firmware
   - Remap any key, macros (editor, library, import/export), hold-a-key layers, knob assignment.
   - NKRO, Windows key repeat, Num Lock / layer indicators, lights-off timeout.
   - Firmware work: game-mode lock (Alt+Tab, Alt+F4), SOCD, adjustable debounce, macro play modes and
     on-the-fly recording, profiles stored on the keyboard (settings memory enlarged where the board has room).
   - Profile import, export and backup.

6. **Lighting depth** (firmware v19, app 0.14)
   - Layers and zones as one combined effect on the keyboard (one effect slot), streamed from the PC on boards
     without the module or room.
   - More effect controls: direction, two or more colours, full colour picker.
   - Per-key colours brought forward (D0k, 2026-11-03): select keys in the preview, colour / off / follow effect
     (v19, 0.17). Still to come here: painting by dragging, saved key-colour sets.
   - Per-key paint editor, including per-key colour on/off over any effect (indicators keep their own
     behaviour), gradient builder, calibration tool with reset to defaults.

7. **Automation in the tray** (app 0.15)
   - Per-app profiles (Windows reports when the front program changes: no polling), playlists, scheduler.
   - App-only key actions: launch app or URL, paste text, timer. On-screen notice on profile change.

8. **PC effects** (app 0.16)
   - Streamed effect engine; the ZX effects, Key explosion, typing effects and VU bars as streamed effects for any
     keyboard with a direct protocol (ZXKB, OpenRGB, SignalRGB firmware). Then screen ambient, system load (read directly: Windows' performance counters are
     broken on D0k's PC), beat pulse, notification flash, GPU temperature.
   - Each measured on its own; screen ambient has no estimate yet and is measured before being called light.

## Left for Stage 3 (unchanged)
Drop-in effects (Python format and SignalRGB Lightscripts), OpenRGB SDK server, game event reactions,
Hue and Nanoleaf. The two drop-in formats are the heavy items (see FIGURES.md); their costs will be set out
for D0k to decide before Stage 3 starts.

## Guesses this plan rests on (from FIGURES.md, all A)
- Tray core with every step 7 feature: 3-6 MB private, 0.000% CPU.
- Streaming at 30 frames/s: under 1% of one core.
- ZXKB.exe with everything: 1-2 MB.
