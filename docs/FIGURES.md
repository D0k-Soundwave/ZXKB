# ZXKB figures register

Tags: **M** measured (date, method) · **P** published (source, date) · **C** computed (working shown) · **A** assumed.
When an A figure is measured, its old value stays here, struck through.

## Machine and tools
| Figure | Value | Tag | Source / working |
|---|---|---|---|
| Logical CPUs on D0k's PC | 24 | M | `os.cpu_count()`, 2026-11-02 |
| Free disk C: / H: | 2,701 GB / 1,209 GB | M | Get-PSDrive, 2026-11-02 |
| QMK MSYS installer | 1.12.0, 600,245,854 bytes, SHA-256 21df7d55...c24c1c | M | GitHub API + sha256sum, 2026-11-02 |
| qmk_firmware checkout | 2.3 GB, master f5db962 (2026-10-29), QMK 0.34.7 | M | `du -sh`, `git log`, 2026-11-02 |
| C++ compiler install size | not needed (QMK MSYS includes g++ 15.2.0) | M | 2026-11-02 (was A: "a few hundred MB") |

## Firmware (Keychron Q3 ISO knob, STM32L432)
| Figure | Value | Tag | Source / working |
|---|---|---|---|
| MCU flash / RAM | 256 KB / 64 KB | P | ST STM32L432KC datasheet; matches the ChibiOS linker map |
| v11 firmware size | 56,596 bytes | M | arm-none-eabi-size, built 2026-11-02 |
| v11 static RAM | 7,280 bytes (.data 1,708 + .bss 5,572) | M | arm-none-eabi-size -A, 2026-11-02 |
| v12 (ZXKB module) firmware size | 59,456 bytes (file 59,472) | M | arm-none-eabi-size, 2026-11-02 |
| v12 flash used | 23% | C | 59,456 / 262,144 |
| v12 static RAM | 7,544 bytes | M | .data 1,708 + .bss 5,836, 2026-11-02 |
| Direct-colour buffer | 264 bytes | C | 88 LEDs x 3 bytes; equals the .bss growth |
| Effect names in flash | 888 bytes | M | .rodata growth v11 -> v12 (was A: ~1 KB) |
| Effects reported | 59 (50 QMK + 9 ZXKB) | M | probe.exe, 2026-11-02 |
| Packets per full frame | 10 | C | 88 LEDs / 9 per packet, rounded up |
| Direct streaming rate | 91.3 frames/s (913 packets/s) | M | probe.exe stream 5, 2026-11-02 (was A: unknown) |
| PC overlay timeout | 5 s | A | kept from v11's VU timeout; works (M: overlay off 6 s after data stopped) |
| RGB brightness / speed step | 16 | P | QMK quantum/rgb_matrix/rgb_matrix.h defaults, master f5db962; Q3 does not override |

## App
| Figure | Value | Tag | Source / working |
|---|---|---|---|
| ZXKB.exe size | 385,536 bytes | M | 2026-11-02 (was A: "under 1 MB") |
| Keyboard found and read | 342 ms | M | probe.exe, 2026-11-02 |
| Idle, no keyboard: private WS / commit / CPU | 1.8 MB / 3.2 MB / 0.000% | M | tools/measure.py 30 s, 2026-11-02 |
| Idle, keyboard connected, VU off | 2.8 MB / 4.4 MB / 0.000% of one core | M | tools/measure.py 60 s, 2026-11-02 15:36 (was A: "2-5 MB") |
| Start-up CPU (once) | 0.59 s | M | Get-Process CPU after 177 s running, 2026-11-02 |
| Busy, VU on with music | 2.1-2.6 MB / 3.5-4.2 MB / 0.156-0.312% of one core | M | tools/measure.py 60 s x2, 2026-11-02 15:42 and 15:45 |
| Old ZX VU, VU on with music | 28.7 MB / 771.8 MB / 1.458% of one core; 2 processes, 36 threads | M | tools/measure.py 60 s, 2026-11-02 15:44 |
| Old ZX VU, earlier sample | 70.7 MB working set, 1.41% of one core | M | Get-Process 10 s, 2026-11-02 (audio state not recorded) |
| Old brief's "~62 MB idle" | superseded | A | no source recorded; replaced by the M rows above |
| VU port vs old app | worst difference 0 of 255 over 1,406 blocks per style | M | app/zxkb/test/run_vu_test.sh, 2026-11-02 |
| VU pass mark | within 2 of 255 | C | one key on a bar ~17 (255 / 15 keys per row), so 2 cannot move a bar's edge by a key |
| VU send rate cap | 40 per second | A | D0k approved (FPS 40) |
| Save delay | 3 s after the last change | A | avoids an EEPROM write per click; not a constraint |
| Silence fed after | 100 ms without audio | A | Windows sends nothing while nothing plays; 10 polls of 10 ms |
| D0k's audio output rate | 96 kHz, 2 channels, 32-bit float | M | vu_live, 2026-11-02. The old-vs-new VU comparison ran at 48 kHz only; 96 kHz not compared |
| AY style on ordinary music: top vs middle bar correlation | 0.54-0.66 | M | vu_live 20 s x3, 2026-11-02 |
| Music style: bar correlations | 0.01-0.25 | M | vu_live 20 s, 2026-11-02 |
| Music style swing, ZX VU values (30 dB, 0.03/block, x0.72/block) | 84 of 255 | M | vu_tune on 20 s of D0k's music, first 3 s skipped |
| Frequency-bands swing (reference for "enough movement") | 86 of 255 | M | same recording |
| Music style swing, chosen values | 118 of 255 | M | same recording |
| Music style range / reference time / fall half-time | 18 dB / 4 s / 120 ms | A | best of the sweep on one song; to be judged by eye by D0k |
| VU key span at row ends | +/- 7 units | C | half the Q3 key pitch of 13 units (LED map, number row) |
| Firmware v14 size | 59,984 bytes | M | arm-none-eabi-size, 2026-11-02 |
| Effect slots left on the Q3 | 4 | C | QMK maximum 63 (P: rgb_config_t mode is 6 bits) - 59 effects |
