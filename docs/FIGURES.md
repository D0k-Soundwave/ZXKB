# ZXKB figures register

Tags: **M** measured (date, method) · **P** published (source, date) · **C** computed (working shown) · **A** assumed.
Rule: no A figure becomes a constraint. When an A figure is measured, the old value is kept under
"Corrected figures" below, not quietly replaced. Constants in the code carry the same tags in their comments.

## Weight-bearing guesses (A) - read these first

| Figure | Value | What rests on it | How to firm it up |
|---|---|---|---|
| Music VU style: range / reference time / fall half-time | 18 dB / 4 s / 120 ms | How lively the Music style looks; chosen as best of a sweep on one song | D0k judges by eye on several songs |
| PC overlay timeout | 5 s | Stage 2 PC effects and SignalRGB: a PC that stops sending for longer loses the keyboard | Measure real gaps (e.g. SignalRGB start-up, PC under load) in Stage 2 |
| Save delay | 3 s after the last change | How often settings are written to the keyboard's flash; flash wear not measured | Count EEPROM writes over a day of use |
| Key explosion life / reach | 0.5-1.52 s / 42 units (~3 keys) | How the effect looks; D0k can change it with speed and size | D0k judges by eye |
| Open retries after plug-in | 5 x 400 ms | Whether ZXKB finds the keyboard after it is plugged in or reflashed (worked every time so far) | Time from device arrival to first reply |

## Machine and tools
| Figure | Value | Tag | Source / working |
|---|---|---|---|
| Logical CPUs on D0k's PC | 24 | M | `os.cpu_count()`, 2026-11-02 |
| Free disk C: / H: | 2,701 GB / 1,209 GB | M | Get-PSDrive, 2026-11-02 |
| QMK MSYS installer | 1.12.0, 600,245,854 bytes, SHA-256 21df7d55...c24c1c | M | GitHub API + sha256sum, 2026-11-02 |
| qmk_firmware checkout | 2.3 GB, master f5db962 (2026-10-29), QMK 0.34.7 | M | `du -sh`, `git log`, 2026-11-02 |
| C++ compiler | g++ 15.2.0, already in QMK MSYS | M | 2026-11-02 |

## Firmware (Keychron Q3 ISO knob, STM32L432)
| Figure | Value | Tag | Source / working |
|---|---|---|---|
| MCU flash / RAM | 256 KB / 64 KB | P | ST STM32L432KC datasheet (not re-read this session). RAM cross-check C: stacks 3,072 + .data 1,708 + .bss 5,572 + heap 55,180 = 65,532 bytes |
| v11 size / static RAM | 56,596 bytes / 7,280 bytes (.data 1,708 + .bss 5,572) | M | arm-none-eabi-size -A, 2026-11-02 |
| v12 size / static RAM | 59,456 bytes / 7,544 bytes | M | arm-none-eabi-size -A, 2026-11-02 |
| v13 size | 59,704 bytes | M | arm-none-eabi-size, 2026-11-02 |
| v14 size | 59,984 bytes | M | arm-none-eabi-size, 2026-11-02 |
| v15 size / flash used | 60,184 bytes / 23% | M / C | arm-none-eabi-size, 2026-11-02; C: 60,184 / 262,144 |
| Direct-colour buffer | 264 bytes | C | 88 LEDs x 3 bytes; equals the v11 -> v12 .bss growth (M) |
| Effect names in flash | 888 bytes | M | .rodata growth v11 -> v12 |
| Effects on the Q3 | 59: v12 50 QMK + 9 ZXKB; from v13 38 QMK + 21 ZXKB | M | probe.exe, 2026-11-02 |
| Effect limit | 63 | P | QMK rgb_config_t mode field is 6 bits (quantum/rgb_matrix/rgb_matrix_types.h, master f5db962) |
| Effect slots left on the Q3 | 4 | C | 63 - 59 |
| Packets per full frame | 10 | C | 88 LEDs / 9 per packet (SignalRGB protocol limit), rounded up |
| Direct streaming rate | 91.3 frames/s (913 packets/s) | M | probe.exe stream 5, 2026-11-02 |
| PC overlay timeout | 5 s | A | kept from v11's VU timeout; M: overlay off 6 s after data stopped |
| Brightness / speed / size / intensity step | 16 | P | QMK rgb_matrix.h RGB_MATRIX_VAL_STEP / SPD_STEP, master f5db962; Q3 does not override |
| Size / intensity defaults | 128 / 255 | C | effects scale distance by 128 / size, so 128 = QMK's own size; 255 = full |
| Size / intensity minimum | 1 / 0 (v22) | C | full range at D0k's request; size 1 because effects divide by it; was 16 / 16 (A) |
| Module EEPROM block | 29 bytes | C | 1 + 2 x 14 tunable effects |
| VU key span at row ends | +/- 7 units | C | half the Q3 key pitch of 13 units (LED map, number row) |
| VU stale time | 150 ms | A | from v9; C: 6 missed updates at 40 per second |
| Raw HID report | 32 bytes | P | QMK RAW_EPSIZE |

## App
| Figure | Value | Tag | Source / working |
|---|---|---|---|
| ZXKB.exe size | 390,144 bytes (0.7) | M | 2026-11-02 |
| Keyboard found and read | 308-342 ms | M | probe.exe, 2026-11-02 (70 round trips) |
| Reply time | about 5 ms | C | 342 ms / 70 round trips |
| Reply timeout | 300 ms | C | ~60x the reply time |
| Menu status wait | 150 ms | C | ~30x the reply time |
| Idle, no keyboard: private WS / commit / CPU | 1.8 MB / 3.2 MB / 0.000% | M | tools/measure.py 30 s, 2026-11-02 |
| Idle, keyboard connected, VU off | 2.8 MB / 4.4 MB / 0.000% of one core | M | tools/measure.py 60 s, 2026-11-02 15:36 |
| Start-up CPU (once) | 0.59 s | M | Get-Process CPU after 177 s running, 2026-11-02 |
| Busy, VU on with music | 2.1-2.6 MB / 3.5-4.2 MB / 0.156-0.312% of one core | M | tools/measure.py 60 s x2, 2026-11-02 15:42 and 15:45 |
| 0.7 + v15: Key explosion (no audio) | 3.5 MB private / 0.000% of one core | M | tools/measure.py 20 s, 2026-11-02 17:44 |
| 0.7 + v15: VU meter showing | 3.8 MB private / 0.469% of one core | M | tools/measure.py 30 s, 17:44; music playing not confirmed |
| 0.7 + v15: lighting off, VU meter chosen | 3.7 MB private / 0.000% of one core | M | tools/measure.py 30 s, 17:45; audio thread stopped on the keyboard's own status message |
| Old ZX VU, VU on with music | 28.7 MB / 771.8 MB / 1.458% of one core; 2 processes, 36 threads | M | tools/measure.py 60 s, 2026-11-02 15:44 |
| Old ZX VU, earlier sample | 70.7 MB working set, 1.41% of one core | M | Get-Process 10 s, 2026-11-02 (audio state not recorded) |
| Save delay | 3 s | A | groups a burst of clicks into one EEPROM write |
| Write timeout | 1 s | A | a write normally completes within a 1 ms USB poll |
| Open retries | 5 x 400 ms | A | time for a keyboard to answer after it appears not measured |
| Thread stacks reserved | 64 KB link, 256 KB audio | A | reserved, not committed |
| Max profiles listed per hotkey | 100 | A | menu id space |
| ZXKB window size | 456 x 708 px at 125% scaling (360 x ~566 at 100%) | M / C | window_test, 2026-11-02 |
| 0.9 tray idle, keyboard connected (v15) | 1.9 MB private working set / 3.3 MB private bytes / 0.000% of one core, 6 threads | M | tools/measure.py 60 s, 2026-11-03 17:22 |
| 0.9 ZXKB.exe size | 417,792 bytes | M | 2026-11-03 |
| 0.9 window copy while open | 15.4-17.3 MB private bytes (its own process) | M | Get-Process, 6 opens, 2026-11-03; preview unlit (v15 has no mirror) |
| 0.9 after closing the window | window copy ended every time; tray 3.20-3.26 MB private, handles 198, threads 3-4 over 5 cycles | M | Get-Process, 2026-11-03 |
| 0.9 preview, v16, Cycle left to right | 35.9% of one core (window copy), private bytes 18.9 MB | M | tools/measure.py 30 s, 2026-11-03 17:28; every frame drew all legends with software drawing |
| 0.10 preview, same effect | 2.2% of one core (both copies) | M | tools/measure.py 30 s, 2026-11-03 17:30; caps drawn once, lights only per frame |
| 0.10 preview, still keyboard (keys off) | 0.47% of one core: window copy 0.31%, tray 0.23% | M | measure.py 30 s + Get-Process 20 s, 2026-11-03; both copies receive the 300 mirror packets/s, nothing redrawn |
| 0.10 tray after the window closed | 1.8 MB private working set / 3.1 MB private bytes / 0.000% | M | measure.py 30 s, 2026-11-03 17:32 |
| Firmware v17 size | 60,688 bytes (+192 on v16) | M | build.sh, 2026-11-03 |
| 0.11 ZXKB.exe size | 433,152 bytes | M | 2026-11-03 |
| dfu-util.exe (QMK MSYS, 0.11-dev, static) | 365,056 bytes, imports only KERNEL32 and msvcrt | M | ls, objdump -p, 2026-11-03 |
| Firmware installs from the app (0.11, v16 -> v17 by hand, then v17 -> v17 by command) | 2 of 2 written ("File downloaded successfully", 60,672 bytes); keyboard restarted by itself 1 of 2 (the by-hand install stayed dark until replugged) | M | D0k + firmware.log, 2026-11-03; phases not timed (timing added in 0.12) |
| VIA definition lookup (usevia.app, Q3) | 297 ms / 453 ms, 10,470 bytes | M | boards_test, 2026-11-03 (two runs) |
| 0.13 ZXKB.exe size | 485,888 bytes (+52,224 on 0.12: VIA support, JSON, WinHTTP lookup, layouts) | M | 2026-11-03 |
| USB IDs inside firmware files | ZXKB v17 and Keychron Q3 build: 3434:0123; Keychron Q3 HE download: 3434:0B31 | M | USB device descriptor search, 2026-11-03 |
| Keychron Q3 firmware built from source | 61,176 bytes (61,192 with DFU suffix) | M | make keychron/q3/iso_encoder:keychron, 2026-11-03 |
| Firmware install timings (0.13) | by command: bootloader 0.8 s, written in 5.3 s, keyboard back at 8.7 s; by hand (unplug + Esc): bootloader 6.0 s (the user), written in 5.5 s, back at 12.2 s; restarted by itself both times | M | firmware.log, 2026-11-03 (v17 -> Keychron's, Keychron's -> v17) |
| Wait for the bootloader after the command / after unplug + Esc | 10 s / 120 s | A | not timed yet |
| dfu-util time limit / wait for the keyboard to return | 120 s / 20 s | A | QMK Toolbox writes the Q3 in a few seconds (not timed) |
| Largest firmware file accepted | 2 MB | P | largest STM32 flash among QMK-supported parts (STM32F4/H7 class); not re-checked |
| Firmware v18 size | 60,760 bytes (+72 on v17) | M | build.sh, 2026-11-03 |
| 0.15 ZXKB.exe size | 498,688 bytes | M | 2026-11-03 (built-in Q3 picture removed, keycap sets and labels added) |
| Wraith keycap set matched to VIA's Q3 layout | 89 of 89 keys by matrix position | M | gen_wraith_kle.py + check against usevia definition, 2026-11-03 |
| Keycap match distance without matrix positions | 0.6 keys from a key's centre | A | not tested on a real imported set yet |
| Firmware v19 size / static RAM | 61,212 bytes / +272 bytes .bss (per-key table) | M | build.sh, 2026-11-03 |
| Per-key colours in settings memory (Q3) | 275 bytes | C | 88 LEDs x 3 + (88 + 7) / 8 = 264 + 11 |
| 0.17 ZXKB.exe size | 506,880 bytes | M | 2026-11-03 |
| Per-key colour, one key (0.18 + v19) | keyboard holds LED 51 = 255, 255, 0 and its LED mirror reports the same; 0 other keys changed | M | probe keys / probe mirror, 2026-11-03 |
| Firmware v20 size / static RAM | 61,472 bytes / .bss 7,204 bytes | M | build.sh, 2026-11-03 |
| Per-key effects in settings memory (Q3) | 616 bytes (module block 645) | C | 88 LEDs x 7 bytes; + 29 |
| Per-key effect speeds (v22) | step 5000 x (16 / 5000) ^ (speed / 255) ms: 5 s (0) to 16 ms (255), about 2.3 % a step; 320 ms at 122; breath 5 steps, twinkle 6, colour cycle 10 | P / C | range by D0k 2026-11-03; 16 ms = QMK RGB_MATRIX_LED_FLUSH_LIMIT (P); tools/key_speed_table.py |
| Macro space left on the keyboard with v20 | 574 bytes, 16 macros | M | probe macros (VIA 0x0C / 0x0D), 2026-11-03 |
| Macro space with v22 | 486 bytes, 16 macros | M | probe macros, 2026-11-03 (worked out beforehand as 574 - 88 = 486, C) |
| Macro space with v25 | 434 bytes, 16 macros | M | probe macros, 2026-11-05 (worked out beforehand as 486 - 20 - 32 = 434, C) |
| Spare settings bytes in v24 | 32 | A | room for a few later settings without a reset; not sized against a list |
| 0.21 ZXKB.exe size | 510,464 bytes | M | 2026-11-03 |
| Key-to-LED map read by 3 programs at once | old code 18 of 18 reads wrong; 0.22 code 0 of 18 | M | probe matrix x3 in parallel x6, compared with QMK's keyboard.json, 2026-11-03 |
| Per-key flash at speed 3 | 1,031 ms between changes (firmware: 80 x 13 = 1,040 ms) | M | probe blink 54 5, 2026-11-03 |
| Fn + number set-up on the Q3 (as plain VIA) | 10 of 10 keys set on layer 1, F13 and F22 held by ZXKB (F23 free), undo 10 of 10 back | M | ZXKB_TEST_FN + probe fnplan + RegisterHotKey check, 2026-11-05 |
| 0.28 ZXKB.exe size | 526,336 bytes (546,304 with swscanf; replaced by wcstol) | M | 2026-11-05 |
| Firmware v16 size | 60,496 bytes (+312), static RAM +280 bytes (LED mirror copy 264 = 88 x 3) | M / C | arm-none-eabi-size, 2026-11-02 |
| LED mirror rate | 30 frames/s while the preview is open, changed or not | C | one 9-LED packet every 3 ms; 10 packets per frame for 88 LEDs |
| Preview window, graphics-card drawing: own RAM / reserved | 15.3 MB / 62.2 MB while open | M | window_test, 2026-11-02 |
| Preview window, software drawing: own RAM / reserved | 9.2-10.0 MB / 15.7-16.2 MB while open | M | window_test, 2026-11-02 |
| After closing the window (software drawing) | handles 280-281, GDI 28, USER 19 steady over 8 cycles; reserved 9.9 -> 12.3 MB | M | leak_test, 2026-11-02 |
| Injected into ZXKB by other software on this PC | RivaTuner (RTSSHooks64.dll), Windhawk (+ mods) | M | process module list, 2026-11-02 |
| Reproducible build | two clean builds identical | M | cmp, 2026-11-02, from 0.8 on (kept 0.7 contains its build time) |

## VU meter
| Figure | Value | Tag | Source / working |
|---|---|---|---|
| D0k's audio output | 96 kHz, 2 channels, 32-bit float | M | vu_live, 2026-11-02 |
| Analysis block / bands | 1024 frames / 16 | A / P | block from ZX VU (10.7 ms at 96 kHz); 16 levels per VU command (zxkb.h) |
| ZX VU tunables: FPS / range / headroom / curve / fall | 40 / 30 dB / 9 dB / 1.7 / x0.72 per block | A | chosen by D0k by eye with ZX VU on this PC; preferences, not measurements |
| Port vs old app | worst difference 0 of 255 over 1,406 blocks per style | M | run_vu_test.sh at 48 kHz only, 2026-11-02; 96 kHz not compared |
| Port pass mark | within 2 of 255 | C | one key on a bar ~17 (255 / 15 keys per row), so 2 cannot move a bar's edge by a key |
| AY style on ordinary music: top vs middle correlation | 0.54-0.66 | M | vu_live 20 s x3, 2026-11-02 |
| Music style: bar correlations | 0.01-0.25 | M | vu_live 20 s, 2026-11-02 |
| Swing (10-90%): Music with ZX VU values / frequency bands / Music chosen | 84 / 86 / 118 of 255 | M | vu_tune on 20 s of D0k's music, first 3 s skipped, 2026-11-02 |
| Music style range / reference / fall half-time | 18 dB / 4 s / 120 ms | A | best of the sweep on one song |
| Poll interval | 10 ms | P | Microsoft WASAPI docs: typical shared-mode period (not checked on this PC) |
| Capture buffer | 100 ms | C | 10 polls of 10 ms |
| Silence fed after | 100 ms | A | Windows sends nothing while nothing plays |
| Audio retry / device check | 2 s / 2 s | A | retry from ZX VU |

## Stage 2 weight estimates (assessment 2026-11-03, all to be measured)
| Figure | Value | Tag | Source / working |
|---|---|---|---|
| Tray core idle with all core features (per-app profiles, playlists, scheduler, notices, SDK server) | 3-6 MB private, 0.000% CPU | A | features are event- or timer-driven; measure at each step |
| ZXKB.exe with the full feature set | 1-2 MB | A | 390,144 bytes today (M) |
| Streaming a PC effect at 30 frames/s | under 1% of one core | A | 300 packets/s (C: 30 x 10); VU at 40 updates/s with audio analysis is 0.469% (M) |
| Screen ambient (desktop duplication) | unknown | A | measure before promising it as light |
| WebView2 for Lightscripts | tens of MB, several processes | A | not measured here; reason to run it only while a Lightscript is chosen |
| Python drop-ins | 19.3 MB or more while running | M / A | Python tray floor measured 2026-11-02; drop-in work on top not measured |
| Keymap layer in keyboard settings memory | 192 bytes | C | 96 matrix positions x 2 bytes |

## Corrected figures (guesses later measured)
| Figure | Guessed (A) | Measured / found | When |
|---|---|---|---|
| Per-key speed range (v20) | 80 ms to 1,280 ms per step | widened to 16 ms to 5 s at D0k's request (v21) | 2026-11-03 |
| Macro space left with v20 | about 560 bytes | 574 bytes (per-key effects take 616: about 1,190 before v19, C) | 2026-11-03 |
| ZXKB idle RAM | 2-5 MB | 1.8-2.8 MB private (working set 13-24 MB incl. shared Windows DLLs) | 2026-11-02 |
| ZXKB.exe size | under 1 MB | 385,536 bytes (0.1), 390,144 bytes (0.7) | 2026-11-02 |
| ZXKB window, RAM while open | 10-20 MB | +0.6 MB private (1.9 -> 2.5 MB); no growth on a second open and close | 2026-11-02 |
| Direct2D and UI libraries in QMK MSYS | believed present | all present: d2d1, dwrite, dxgi, d3d11, uxtheme, dwmapi, comctl32 | 2026-11-02 |
| Python version of the app, idle | 20-22 MB | never measured: option dropped (Python tray floor M: 19.3 MB) | 2026-11-02 |
| Python app download / native app | 15-20 MB / under 1 MB | Python not built; native 390 KB | 2026-11-02 |
| Effect names in flash | ~1 KB | 888 bytes | 2026-11-02 |
| ZXKB typing effects in flash | 2-4 KB | +248 bytes net (QMK's originals no longer built) | 2026-11-02 |
| qmk_firmware download | under 2 GB | 2.3 GB (the guess was wrong) | 2026-11-02 |
| C++ compiler install | a few hundred MB | nothing: already in QMK MSYS | 2026-11-02 |
| Direct streaming rate | unknown | 91.3 frames/s | 2026-11-02 |
| Old ZX VU idle RAM (old brief) | ~62 MB, no source | 70.7 MB working set, 28.7 MB private, 771.8 MB reserved | 2026-11-02 |
| Effects on the Q3 | "50 QMK + 9 ZXKB" written for all builds | true for v12 only; v13 on: 38 + 21 | 2026-11-02 |
| VU maths at 96 kHz | stated as "same results either way" | not compared; only 48 kHz was tested | 2026-11-02 |
