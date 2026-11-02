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
| Size / intensity minimum | 16 | A | keeps an effect from vanishing |
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

## Corrected figures (guesses later measured)
| Figure | Guessed (A) | Measured / found | When |
|---|---|---|---|
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
