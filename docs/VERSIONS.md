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
| v15 | 2026-11-02 | 60,184 bytes | VU meter is a normal effect (picked and saved like any other); Fn + V switches between it and the previous effect. Status sent to ZXKB unasked on changes and reports "VU possible" and the firmware version (protocol 2), so ZXKB listens to audio only while the VU meter shows. |
| v14 | 2026-11-02 | 59,984 bytes | VU bar end key lights in proportion to how much the bar covers it. Not kept. |
| v13 | 2026-11-02 | 59,704 bytes | Size and intensity for typing effects; ZXKB versions of QMK's typing effects. Not kept. |
| v12 | 2026-11-02 | 59,456 bytes | ZXKB community module: PC direct mode, SignalRGB protocol, Fn + number slots, Key explosion. Not kept. |
| v11 | 2026-11-02 | 56,596 bytes | Last firmware before ZXKB (calibrated colours, flashing Caps Lock). Not kept. |

## App (ZXKB.exe)

| Version | Date | Size | Changes |
|---|---|---|---|
| 0.8 | 2026-11-02 | 400,384 bytes | The ZXKB window (Stage 2, step 1): double-click the tray icon, or "Open ZXKB window" in the menu. Effect list in groups with the VU styles, lighting on/off, favourite, sliders for brightness, speed, size and intensity, Spectrum colours and a full colour picker. Opens next to the tray, follows the keyboard, freed on close. |
| 0.7 | 2026-11-02 | 390,144 bytes | VU meter styles listed with the effects and chosen like one; audio captured only while the VU meter effect shows; left click on the tray icon does nothing; version shown in the menu. |
| 0.1-0.6 | 2026-11-02 | | First builds (tray, effects, profiles, Fn slots, VU port, size/intensity, music style). Not kept. |
