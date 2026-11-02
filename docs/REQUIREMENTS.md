# ZXKB requirements

Status: `[x]` done · `[~]` in source, not yet built/flashed · `[ ]` to do · `[?]` decision pending.
Rule: do not build until D0k confirms the list is complete.

## Firmware
- [ ] PC direct mode: ZXKB streams a colour per LED (for drop-ins, PC effects, OpenRGB SDK server)
- [ ] SignalRGB protocol module included (so SignalRGB can be tried without reflashing)
- [ ] Key explosion effect: pressed key fires a small star that spreads, changes colour and fades to black
- [ ] Fn+1 to Fn+0 report to ZXKB as hotkey slots
- [ ] Package as a QMK community module so any QMK keyboard can add it; board reports LED positions
- [~] Calibrated Spectrum colours in all Spectrum effects and the VU meter
- [~] Hardware balance red 192 / green 72 / blue 58 as firmware default
- [~] Caps Lock flashing magenta/white (320 ms)
- [x] Live LED current tune commands 0xD3/0xD4/0xD5 (v10)
- [x] Spectrum effects: stripes, tape loading, VU, border flash, attribute clash, typing ripple, logo sweep (v9/v10)

## ZXKB app (renamed from ZX VU)
- [ ] Every effect by name in the tray: QMK built-ins, Spectrum, PC-driven, drop-ins
- [ ] Effect settings panel (speed, colours, direction...) and favourites
- [?] Overall brightness control: presets (25/50/75/100) or steps
- [ ] Profiles: save/load whole setup by name
- [ ] Playlists: cycle effects on a timer
- [ ] Scheduler: switch profiles at set times
- [ ] Fn+number hotkey slots, each assignable to an effect or profile
- [ ] Live preview window with the Wraith legends
- [ ] Per-key paint editor
- [ ] Gradient builder (multi-point colour waves)
- [ ] Drop-in effects: ZXKB Python format in an `Effects` folder
- [ ] Drop-in effects: SignalRGB Lightscripts (HTML canvas via WebView2; `<meta>` properties become settings;
      audio visualisers fed by ZXKB audio; game integrations not at first)
- [ ] Built-in PC effects: VU meter (bands + ACB), screen ambient, system load (CPU/RAM/GPU), beat pulse,
      notification flash
- [ ] OpenRGB SDK server
- [ ] Calibration tool built in, with reset to defaults (current tool: `tools/zx_colour_cal.py`)
- [ ] Keyboard-independent: VIA-based detection; supports boards with the ZXKB module, SignalRGB or OpenRGB firmware
- [ ] Starts with Windows (tray), lightweight; report measured idle/busy RAM and CPU

## Release
- [ ] GitHub repo under D0k-Soundwave, GPL-3.0, no Claude attribution
- [?] Public from day one, or private until first release
- [ ] README and user manual (existing Q3 keyboard manual doc to be updated for the custom firmware)
