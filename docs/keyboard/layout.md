# Keyboard layout and functions

Generated from `firmware/keymaps/d0k/keymap.c` by `tools/gen_keyboard_docs.py`.
LED = RGB matrix index (Caps Lock = 49). Spectrum = what the keyboard sends with the switch on Mac;
Core = how the MiSTer ZX Spectrum core interprets it. Fn = same in both modes.

Knob turn: Spectrum `KC_LEFT, KC_RGHT`, Windows `KC_VOLD, KC_VOLU`, Fn `RM_VALD, RM_VALU`.

| Key | Wraith cap | LED | Windows | Spectrum | Core | Fn |
|---|---|---|---|---|---|---|
| esc | ESC / EJECT | 0 | Esc | Esc | BREAK |  |
| f1 | F1 | 1 | F1 | F1 | Tape pause | Screen brightness down |
| f2 | F2 | 2 | F2 | F2 | Tape previous part | Screen brightness up |
| f3 | F3 | 3 | F3 | F3 | Tape next part | (nothing) |
| f4 | F4 | 4 | F4 | F4 | CPU 3.5 MHz | (nothing) |
| f5 | F5 | 5 | F5 | F5 | CPU 7 MHz | Lights dimmer |
| f6 | F6 | 6 | F6 | F6 | CPU 14 MHz | Lights brighter |
| f7 | F7 | 7 | F7 | F7 | CPU 28 MHz | Previous track |
| f8 | F8 | 8 | F8 | F8 | CPU 56 MHz | Play/Pause |
| f9 | F9 | 9 | F9 | F9 | Pause / continue | Next track |
| f10 | F10 | 10 | F10 | F10 | 48K BASIC | Mute |
| f11 | F11 | 11 | F11 | F11 | +D snapshot / file browser | Volume down |
| f12 | F12 | 12 | F12 | F12 | MiSTer OSD menu | Volume up |
| knob | KNOB |  | Mute | Mute |  | Lights on/off |
| prt | PRINT | 13 | Print Screen | F11 | +D snapshot / file browser (F11) |  |
| scr | LOCK | 14 | Scroll Lock | F12 | MiSTer OSD menu (F12) |  |
| pau | PAUSE | 15 | Pause | F9 | Pause / continue (F9) | Next effect |
| grv | ` / ~ | 16 | ` ~ | ` ~ |  |  |
| n1 | 1 / ! | 17 | 1 | 1 |  | ZK_1 |
| n2 | 2 / @ | 18 | 2 | 2 |  | ZK_2 |
| n3 | 3 / # | 19 | 3 | 3 |  | ZK_3 |
| n4 | 4 / $ | 20 | 4 | 4 |  | ZK_4 |
| n5 | 5 / % | 21 | 5 | 5 |  | ZK_5 |
| n6 | 6 / ^ | 22 | 6 | 6 |  | ZK_6 |
| n7 | 7 / & | 23 | 7 | 7 |  | ZK_7 |
| n8 | 8 / * | 24 | 8 | 8 |  | ZK_8 |
| n9 | 9 / ( | 25 | 9 | 9 |  | ZK_9 |
| n0 | 0 / ) | 26 | 0 | 0 |  | ZK_0 |
| min | - / _ | 27 | - _ | - _ |  |  |
| eq | = / + | 28 | = + | = + |  |  |
| bks | DELETE / FORMAT | 29 | Backspace | Backspace | DELETE |  |
| ins | BLUE / EDIT | 30 | Insert | Shift+1 (Spectrum EDIT) |  |  |
| hom | RED / LOCK | 31 | Home | Shift+2 (CAPS LOCK) |  |  |
| pgu | MAG / TR.VID | 32 | Page Up | Shift+3 (TRUE VIDEO) |  |  |
| tab | TAB | 33 | Tab | Tab | GRAPHICS | Lights on/off |
| q | Q / PLOT | 34 | Q | Q |  | Next effect |
| w | W / DRAW | 35 | W | W |  | Lights brighter |
| e | E / REM | 36 | E | E |  | Hue + |
| r | R / RUN | 37 | R | R |  | Saturation + |
| t | T / RAND | 38 | T | T |  | Effect speed + |
| y | Y / RETURN | 39 | Y | Y |  |  |
| u | U / IF | 40 | U | U |  |  |
| i | I / INPUT | 41 | I | I |  |  |
| o | O / POKE | 42 | O | O |  |  |
| p | P / PRINT | 43 | P | P |  |  |
| lbr | [ / { | 44 | [ { | [ { | ( |  |
| rbr | ] / } | 45 | ] } | ] } | ) |  |
| del | GREEN / INV.VID | 46 | Delete | Shift+4 (INV VIDEO) |  |  |
| end | GRAPH / CAT | 47 | End | Shift+9 (GRAPHICS) |  |  |
| pgd | BLACK / DELETE | 48 | Page Down | Shift+0 (DELETE) |  |  |
| cap | CAPS LOCK / FN | 49 | Caps Lock | Caps Lock | CAPS LOCK |  |
| a | A / NEW | 50 | A | A |  | Previous effect |
| s | S / SAVE | 51 | S | S |  | Lights dimmer |
| d | D / DIM | 52 | D | D |  | Hue - |
| f | F / FOR | 53 | F | F |  | Saturation - |
| g | G / GOTO | 54 | G | G |  | Effect speed - |
| h | H / GOSUB | 55 | H | H |  |  |
| j | J / LOAD | 56 | J | J |  |  |
| k | K / LIST | 57 | K | K |  |  |
| l | L / LET | 58 | L | L |  |  |
| sem | ; / : | 59 | ; : | ; : |  |  |
| quo | ' / " | 60 | ' " | ' " | " (Symbol Shift + P) |  |
| hsh | \ / \| | 61 | \ \| (non-US #, with the US-based Windows layout) | \ \| (non-US #, with the US-based Windows layout) | * |  |
| ent | ENTER | 62 | Enter | Enter |  |  |
| lsh | CAPS / SHIFT | 63 | Left Shift | Left Shift | CAPS SHIFT |  |
| iso | \ / \| | 64 | \ \| (ISO key, with the US-based Windows layout) | Shift+1 (Spectrum EDIT) |  |  |
| z | Z / COPY | 65 | Z | Z |  |  |
| x | X / CLEAR | 66 | X | X |  |  |
| c | C / COUNT | 67 | C | C |  |  |
| v | V / CLS | 68 | V | V |  | VU meter on/off |
| b | B / BORDER | 69 | B | B |  |  |
| n | N / NEXT | 70 | N | N |  | NKRO on/off |
| m | M / PAUSE | 71 | M | M |  | Record a macro (start / stop) |
| com | , / < | 72 | , < | , < |  |  |
| dot | . / > | 73 | . > | . > |  |  |
| sl | / / ? | 74 | / ? | / ? |  |  |
| rsh | (blank) | 75 | Right Shift | Right Ctrl | SYMBOL SHIFT (sends Right Ctrl) |  |
| up | ⇧ | 76 | Up | Up | Cursor up |  |
| lct | PEEK / OUT | 77 | Left Ctrl | Left Ctrl | SYMBOL SHIFT |  |
| lwn | CODE / IN | 78 | Windows | EXTENDED MODE (Ctrl + Left Shift) |  | Game mode on/off |
| lal | RND / MERGE | 79 | Alt | Alt |  |  |
| spc | (blank) | 80 | Space | Space |  |  |
| ral | RND / MERGE | 81 | AltGr | AltGr |  |  |
| rwn | CODE / IN | 82 | Windows | Types LOAD "" + Enter (60 ms per key) |  | Game mode on/off |
| fn | EDIT / DEF FN | 83 | Fn (hold for WIN_FN) | Fn (hold for SPEC_FN) |  |  |
| rct | PEEK / OUT | 84 | Right Ctrl | Right Ctrl | SYMBOL SHIFT |  |
| lft | ⇦ | 85 | Left | Left | Cursor left |  |
| dn | ⇩ | 86 | Down | Down | Cursor down |  |
| rt | ⇨ | 87 | Right | Right | Cursor right |  |
