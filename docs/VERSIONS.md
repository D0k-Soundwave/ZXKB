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
| v22 | 2026-11-03 | 61,956 bytes | Full ranges (D0k): each key's speed is a whole byte, 256 steps from 5 s to 16 ms (speed 122 = 320 ms); size 1-255 and intensity 0-255 (both stopped at 16 before). 8 bytes a key (module block version 5, 733 bytes on the Q3): the first start resets the keymap, macros and key colours. Protocol 8. |
| v21 | 2026-11-03 | 61,496 bytes | Per-key effect speeds 16 ms to 5 s per step (was 80 ms to 1.28 s); speed 12 stays 320 ms. Same settings layout: key colours are kept. |
| v20 | 2026-11-03 | 61,472 bytes | Per-key effects (protocol 7): each key Steady, Flash (two colours, all in step; speed 12 = Spectrum FLASH 320 ms), Breathe, Colour cycle or Twinkle, with its own speed (0-15) and two colours; 7 bytes a key (module block version 4, 645 bytes on the Q3), so the first start resets the keymap, macros and key colours. |
| v19 | 2026-11-03 | 61,212 bytes | Per-key colours kept on the keyboard (0xDD, protocol 6): any key its own colour or off, over any effect, scaled by brightness; Caps Lock still on top. Settings memory grows by 275 bytes (module block version 3), so the first start resets the VIA keymap and macros to the firmware defaults. |
| v18 | 2026-11-03 | 60,760 bytes | Reports which LED sits under each key (0xD9 0x03, protocol 5), so ZXKB can light any keyboard drawn from its published layout. |
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
| 0.25 | 2026-11-03 | 512,000 bytes | Key speed 0-255; Size down to 1, Intensity down to 0; every slider: arrow keys move one step, a click on the bar 1/32 of the range. Needs firmware v22 for key colours. |
| 0.24 | 2026-11-03 | 511,488 bytes | Key speed shows the step time (ms or s) instead of a percentage, using the v21 speed table. |
| 0.23 | 2026-11-03 | 510,976 bytes | With keys selected, the Speed slider on the right becomes Key speed (0-15) for them; the separate key speed slider under the preview is gone (D0k moved the effect's Speed, which does nothing visible on Solid colour). |
| 0.22 | 2026-11-03 | 510,464 bytes | Fix: the window copy could take the tray copy's replies while both read the keyboard (Windows gives every reply to both), so its key-to-LED map came out wrong and a selected key lit another key (D0k: G lit "-", and the up arrow in the preview). Every reply must now echo the effect, LED or map position asked for. |
| 0.21 | 2026-11-03 | 510,464 bytes | Second row under the preview for the selected keys: Key effect, Speed, and buttons 1 / 2 choosing which of the keys' two colours the colour boxes set. Needs firmware v20. |
| 0.20 | 2026-11-03 | 506,880 bytes | Black added to the colour boxes, for keys (turns the selected keys off); greyed out for the effect colour, which has no black. |
| 0.19 | 2026-11-03 | 506,880 bytes | Clear gives every key black as its own colour (lighting stays on, keys can then be coloured from the app); clicking an empty spot in the preview deselects. |
| 0.18 | 2026-11-03 | 506,880 bytes | With keys selected, the colour swatches and More colours... colour those keys (in 0.17 they still set the effect's colour for the whole board, which is what D0k hit; only a separate Colour... button coloured keys). The selection outline sits on the cap, so the key's light stays visible. |
| 0.17 | 2026-11-03 | 506,880 bytes | Click keys in the preview to select them (Ctrl+click for more), then Colour..., Off or Follow effect; Select all, Clear. The picker offers the calibrated Spectrum colours first. The preview builds its key list only when the board, size or labels change, not every frame. |
| 0.16 | 2026-11-03 | 500,736 bytes | Key labels name the Windows keyboard layout itself, not the language (D0k's English (UK) uses his custom "US - Spectrum (GBP on AltGr+4)" layout); any installed layout can be chosen for the labels, or "as Windows types", which follows a layout switch. |
| 0.15 | 2026-11-03 | 498,688 bytes | One preview for every keyboard: layout from the keyboard's VIA definition (looked up once after asking), lit through the firmware's key-to-LED map (v18), with labels from what each key does in the Windows keyboard language (keymap read through VIA) or a keycap set. Keycap sets are KLE files; D0k's KAM Wraith set is the first, no longer built in. Import a KLE file from the window. The built-in Q3 picture is gone. |
| 0.14 | 2026-11-03 | 485,888 bytes | The effect list ignores typed letters (it used to jump to the effect starting with the letter, changing the keyboard's effect while trying typing effects). |
| 0.13 | 2026-11-03 | 485,888 bytes | Plain VIA keyboards (standard firmware): recognised, lighting controlled through VIA, effect names and key layout from the keyboard's VIA definition, looked up online once by USB ID after asking. Firmware check by USB ID instead of USB name (so Keychron's own firmware for the same keyboard is accepted, other keyboards' refused); honest warning that non-ZXKB firmware may reset settings. |
| 0.12 | 2026-11-03 | 433,664 bytes | Firmware install: clearer message when the keyboard does not restart by itself after writing (unplug and replug, firmware is already written); firmware.log records the time of each phase. |
| 0.11 | 2026-11-03 | 433,152 bytes | Firmware... button in the window: lists the kept firmware files, checks a file is for this keyboard, restarts the keyboard into its bootloader (v17 on; older firmware: unplug and hold Esc) and writes it with dfu-util, with progress, then waits for the keyboard to come back. dfu-util.exe installed next to ZXKB.exe. |
| 0.10 | 2026-11-03 | 419,328 bytes | Live preview made light: keycaps and legends drawn once, each frame only paints the lights (36% -> 2.2% of one core while animating); frames with no colour change are skipped. Version number in the file properties fixed (0.9 said 0.8 there). |
| 0.9 | 2026-11-03 | 417,792 bytes | The ZXKB window runs as its own copy of ZXKB.exe, started by the tray, so closing it gives all its memory back. Live preview of the keyboard with its keycaps, lit as the keyboard is (needs firmware v16). Favourites, hotkeys and VU style changed in one are picked up by the other. |
| 0.8 | 2026-11-02 | 400,384 bytes | The ZXKB window (Stage 2, step 1): double-click the tray icon, or "Open ZXKB window" in the menu. Effect list in groups with the VU styles, lighting on/off, favourite, sliders for brightness, speed, size and intensity, Spectrum colours and a full colour picker. Opens next to the tray, follows the keyboard, freed on close. |
| 0.7 | 2026-11-02 | 390,144 bytes | VU meter styles listed with the effects and chosen like one; audio captured only while the VU meter effect shows; left click on the tray icon does nothing; version shown in the menu. |
| 0.1-0.6 | 2026-11-02 | | First builds (tray, effects, profiles, Fn slots, VU port, size/intensity, music style). Not kept. |
