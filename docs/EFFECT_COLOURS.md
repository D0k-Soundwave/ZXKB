# Effect colours: audit (2026-11-06)

D0k, 2026-11-06: every effect's colours must be settable from the app's colour boxes; today many effects ignore
them. Checked against the code of each effect: QMK master in `%USERPROFILE%\qmk_firmware\quantum\rgb_matrix\animations`
and the ZXKB module (`firmware/modules/d0k/zxkb/rgb_matrix_module.inc`), for the 60 effects on D0k's Q3 (firmware v33,
list read from the keyboard).

Stock QMK / VIA keeps one colour for the whole board (hue and saturation); effects can only use that one colour and
what they work out from it. So all of the below is for keyboards with ZXKB firmware; on stock keyboards the colour
boxes stay as they are (one colour, and only effects that use it).

## How each effect uses colour now

| Group | Effects | Colour boxes today |
|---|---|---|
| A. One colour, follows the box | SOLID_COLOR, BREATHING, BAND_SAT, BAND_VAL, BAND_PINWHEEL_SAT, BAND_PINWHEEL_VAL, BAND_SPIRAL_SAT, BAND_SPIRAL_VAL, STARLIGHT, STARLIGHT_SMOOTH, RIVERFLOW, PIXEL_FRACTAL, the 12 ZX typing effects (ZX_REACTIVE_*, ZX_SPLASH, ZX_MULTISPLASH, ZX_SOLID_SPLASH, ZX_SOLID_MULTISPLASH) | work |
| B. One colour plus a second worked out by the effect | ALPHAS_MODS (second = first + the speed setting), GRADIENT_UP_DOWN and GRADIENT_LEFT_RIGHT (spread set by speed), DUAL_BEACON, STARLIGHT_DUAL_HUE, STARLIGHT_DUAL_SAT, RAINDROPS, HUE_BREATHING, HUE_PENDULUM, HUE_WAVE (hues near the first), KEY_EXPLOSION (drifts from the first) | first colour only; the second cannot be chosen |
| C. Rainbows | CYCLE_ALL, CYCLE_LEFT_RIGHT, CYCLE_UP_DOWN, CYCLE_OUT_IN, CYCLE_OUT_IN_DUAL, CYCLE_PINWHEEL, CYCLE_SPIRAL, RAINBOW_MOVING_CHEVRON, RAINBOW_BEACON, RAINBOW_PINWHEELS, FLOWER_BLOOMING | the box only moves where the rainbow starts |
| D. Random colours | JELLYBEAN_RAINDROPS, PIXEL_RAIN, PIXEL_FLOW | ignored (any colour at random) |
| E. Fixed colours | DIGITAL_RAIN (green), TYPING_HEATMAP (blue to red), SPECTRUM_STRIPES and LOGO_SWEEP (red, yellow, green, cyan), TAPE_LOADING (red/cyan, then yellow/blue), VU_METER (red, yellow, green, cyan blocks), BORDER_FLASH and ATTR_CLASH (the 7 Spectrum colours), TYPING_RIPPLE (red, yellow, green, cyan in turn) | ignored |
| No colour | PC_DIRECT (colours come from the PC), KEY_COLOURS (the keys' own colours) | not applicable |

Count: A 24, B 11, C 11, D 3, E 9 (8 after stripes and logo sweep are merged), none 2. [C] 24 + 11 + 11 + 3 + 9 + 2 = 60.

## What is needed

1. **A colour list for each effect** in the app, in place of the single colour boxes: as many colour boxes as the
   effect uses, each settable with the Spectrum colours or the full colour picker, plus "Reset to the effect's own
   colours". Per group:
   - A: 1 colour (as now).
   - B: 2 colours (the second chosen, not worked out).
   - C: 2 to 8 colours, blended in a loop in place of the rainbow; "Rainbow" kept as a choice.
   - D: 2 to 8 colours picked from at random; "Any colour" kept as a choice.
   - E: the effect's own number: Digital rain 2 (rain, head), Typing heatmap 2 (cold, hot), Spectrum stripes 1 to 8
     (default the 4 logo colours), Tape loading 4 (two pairs), VU meter 4 (the blocks), Border flash and Attribute
     clash 1 to 8 (default the 7 Spectrum colours), Typing ripple 1 to 8 (default 4).
2. **Firmware: a colour list the effects read.** The keyboard holds the current effect's list (up to 8 colours:
   [C] 8 x 3 = 24 bytes and a count). The app keeps a list for every effect and sends it when the effect changes,
   also when the effect is changed on the keyboard itself. Without ZXKB running, the keyboard keeps the last list it
   was sent. Keeping a list for every effect on the keyboard instead would need [C] 8 x 3 x 57 effects with colours = 1,368
   bytes of its 2,048-byte settings memory (P: docs/REQUIREMENTS.md, from the Q3's build), most of which is in use.
3. **ZXKB versions of 26 QMK effects**: B 10 (ALPHAS_MODS, GRADIENT_UP_DOWN, GRADIENT_LEFT_RIGHT, DUAL_BEACON,
   STARLIGHT_DUAL_HUE, STARLIGHT_DUAL_SAT, RAINDROPS, HUE_BREATHING, HUE_PENDULUM, HUE_WAVE; KEY_EXPLOSION is already
   ZXKB's own), C 11, D 3, E 2 (DIGITAL_RAIN, TYPING_HEATMAP): [C] 10 + 11 + 3 + 2 = 26. The QMK originals are
   switched off in the keymap, so the number of effects stays the same (59 of 63 after the merge), as was done for
   the typing effects (v20 on). Their maths stays QMK's; only where the colours come from changes. ZXKB's own
   effects (KEY_EXPLOSION and group E's ZX effects) are changed in place.
4. **The preview** draws every effect with its colour list (part of "preview first, then Apply").

Not possible: none found; every effect above can take a colour list once it is ZXKB's own version.
