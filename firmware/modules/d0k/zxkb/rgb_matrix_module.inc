// ZXKB effects. Spectrum: stripes, tape loading, audio VU meter, border flash, attribute clash,
// typing ripple, logo sweep. Also: key explosion, ZXKB versions of QMK's typing effects with size and
// intensity settings, and PC_DIRECT (colours streamed from the PC).
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
RGB_MATRIX_EFFECT(SPECTRUM_STRIPES)
RGB_MATRIX_EFFECT(TAPE_LOADING)
RGB_MATRIX_EFFECT(VU_METER)
RGB_MATRIX_EFFECT(BORDER_FLASH)
RGB_MATRIX_EFFECT(ATTR_CLASH)
RGB_MATRIX_EFFECT(TYPING_RIPPLE)
RGB_MATRIX_EFFECT(LOGO_SWEEP)
#ifdef RGB_MATRIX_KEYREACTIVE_ENABLED
RGB_MATRIX_EFFECT(KEY_EXPLOSION)
RGB_MATRIX_EFFECT(ZX_REACTIVE_SIMPLE)
RGB_MATRIX_EFFECT(ZX_REACTIVE)
RGB_MATRIX_EFFECT(ZX_REACTIVE_WIDE)
RGB_MATRIX_EFFECT(ZX_REACTIVE_MULTIWIDE)
RGB_MATRIX_EFFECT(ZX_REACTIVE_CROSS)
RGB_MATRIX_EFFECT(ZX_REACTIVE_MULTICROSS)
RGB_MATRIX_EFFECT(ZX_REACTIVE_NEXUS)
RGB_MATRIX_EFFECT(ZX_REACTIVE_MULTINEXUS)
RGB_MATRIX_EFFECT(ZX_SPLASH)
RGB_MATRIX_EFFECT(ZX_MULTISPLASH)
RGB_MATRIX_EFFECT(ZX_SOLID_SPLASH)
RGB_MATRIX_EFFECT(ZX_SOLID_MULTISPLASH)
#endif
RGB_MATRIX_EFFECT(PC_DIRECT)

#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#include "zxkb.h"

// ZX Spectrum logo colours: red, yellow, green, cyan
static const uint8_t zx_cols[4][3] = { {204, 0, 0}, {255, 255, 0}, {0, 205, 0}, {0, 205, 205} };   // BRIGHT red, yellow, green, cyan; brightness matched by eye 2026-11-02

static inline void zx_set(uint8_t i, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t v = rgb_matrix_config.hsv.v;
    rgb_matrix_set_color(i, scale8(r, v), scale8(g, v), scale8(b, v));
}

// Diagonal Spectrum rainbow bands scrolling across the board (speed key changes scroll rate)
static bool SPECTRUM_STRIPES(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint16_t shift = scale16by8(g_rgb_timer, rgb_matrix_config.speed / 8);
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint16_t pos = g_led_config.point[i].x + (64 - g_led_config.point[i].y) + shift;
        uint8_t  band = (pos / 18) % 4;
        zx_set(i, zx_cols[band][0], zx_cols[band][1], zx_cols[band][2]);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// Tape loading border: red/cyan pilot tone bars, then flickering yellow/blue data bars
static uint16_t zx_hash(uint16_t x) { x ^= x << 7; x ^= x >> 9; x ^= x << 8; return x * 0x9E37; }
static bool TAPE_LOADING(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint32_t t = g_rgb_timer;
    bool pilot = ((t / 1000) % 8) < 3;
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t y = g_led_config.point[i].y;
        if (pilot) {
            bool on = (((y + (t / 6)) / 9) & 1);
            if (on) zx_set(i, 204, 0, 0); else zx_set(i, 0, 205, 205);
        } else {
            uint16_t stripe = (y + (t / 3)) / 4;
            bool on = zx_hash(stripe ^ (uint16_t)(t / 60)) & 0x100;
            if (on) zx_set(i, 255, 255, 0); else zx_set(i, 0, 0, 170);
        }
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// Audio VU meter: three thick bars, each two keyboard rows high, filling left to right.
//   Bar 1 = Esc/F-row + number row   (ACB mode: channel A,  bands mode: treble)
//   Bar 2 = Tab row + Caps Lock row  (ACB mode: channel B,  bands mode: mids)
//   Bar 3 = Shift row + bottom row   (ACB mode: channel C,  bands mode: bass)
// Solid Spectrum-logo colour blocks across the board, left to right: red | yellow | green | cyan (BRIGHT palette)
// Keys in neighbouring rows are staggered, so a bar that just lit keys whose centre it had passed ended on a
// different key in each row (and the space bar jumped on all at once). Instead each key covers a span, from
// half-way to its left neighbour to half-way to its right one, and lights in proportion to how much of that
// span the bar covers. Both rows of a bar then fade out at the same point.
static const uint8_t vu_bar_first[3] = { 11, 5, 0 };
static const uint8_t vu_bar_last[3]  = { 15, 10, 4 };
#define VU_HALF_KEY 7   // half a key: the Q3's key pitch is 13 units (LED map), for keys at the end of a row
static uint8_t vu_left[RGB_MATRIX_LED_COUNT], vu_right[RGB_MATRIX_LED_COUNT];
static bool    vu_spans_ready = false;
static uint8_t vu_row(uint8_t y) { return (y < 8) ? 0 : (y < 21) ? 1 : (y < 34) ? 2 : (y < 46) ? 3 : (y < 58) ? 4 : 5; }
static void vu_make_spans(void) {
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        uint8_t x = g_led_config.point[i].x, row = vu_row(g_led_config.point[i].y);
        int16_t left_x = -1, right_x = 999;   // nearest neighbours in the same row
        for (uint8_t j = 0; j < RGB_MATRIX_LED_COUNT; j++) {
            if (j == i || vu_row(g_led_config.point[j].y) != row) continue;
            uint8_t xj = g_led_config.point[j].x;
            if (xj < x && xj > left_x) left_x = xj;
            if (xj > x && xj < right_x) right_x = xj;
        }
        vu_left[i]  = left_x >= 0 ? (uint8_t)((left_x + x + 1) / 2) : (x > VU_HALF_KEY ? x - VU_HALF_KEY : 0);
        vu_right[i] = right_x < 999 ? (uint8_t)((x + right_x) / 2) : (uint8_t)(x + VU_HALF_KEY > 255 ? 255 : x + VU_HALF_KEY);
    }
    vu_spans_ready = true;
}
static bool VU_METER(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    if (params->init) { for (uint8_t b = 0; b < 16; b++) zxkb_vu_levels[b] = 0; }
    bool stale = timer_elapsed32(zxkb_vu_last_update) > 150;
    if (stale && led_min == 0) { for (uint8_t b = 0; b < 16; b++) zxkb_vu_levels[b] = (zxkb_vu_levels[b] * 7) / 8; }
    uint8_t bar_level[3];
    for (uint8_t k = 0; k < 3; k++) {
        uint8_t lvl = 0;
        for (uint8_t b = vu_bar_first[k]; b <= vu_bar_last[k]; b++) if (zxkb_vu_levels[b] > lvl) lvl = zxkb_vu_levels[b];
        bar_level[k] = lvl;
    }
    if (!vu_spans_ready) vu_make_spans();
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t x = g_led_config.point[i].x, y = g_led_config.point[i].y;
        uint8_t lvl = bar_level[vu_row(y) / 2];           // both rows of a bar show the same value
        uint16_t reach = (uint16_t)lvl * 232 / 255;
        uint8_t cover = 0;                                // how much of this key the bar covers, 0-255
        if (lvl > 8) {
            if (reach >= vu_right[i]) cover = 255;
            else if (reach > vu_left[i]) cover = (uint16_t)(reach - vu_left[i]) * 255 / (vu_right[i] - vu_left[i]);
        }
        if (cover) {
            uint8_t c = (x < 56) ? 0 : (x < 112) ? 1 : (x < 168) ? 2 : 3;   // red, yellow, green, cyan (left to right)
            zx_set(i, scale8(zx_cols[c][0], cover), scale8(zx_cols[c][1], cover), scale8(zx_cols[c][2], cover));
        } else {
            rgb_matrix_set_color(i, 0, 0, 0);
        }
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// The 7 BRIGHT Spectrum colours (no black): blue, red, magenta, green, cyan, yellow, white
static const uint8_t zx_bright[7][3] = { {0,0,170}, {204,0,0}, {200,0,200}, {0,205,0}, {0,205,205}, {255,255,0}, {195,195,195} };   // blue, red, magenta, green, cyan, yellow, white (calibrated)

// Border flash: the whole board jumps to a random border colour, like a tape loader's border
static bool BORDER_FLASH(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint16_t step = 60 + (255 - rgb_matrix_config.speed) / 2;          // 60-187 ms per colour
    uint8_t  c = zx_hash((uint16_t)(g_rgb_timer / step)) % 7;
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        zx_set(i, zx_bright[c][0], zx_bright[c][1], zx_bright[c][2]);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// Attribute clash: the board is split into character cells, each with its own INK/PAPER pair.
// Cells change colours at random and swap INK/PAPER every 320 ms, like the FLASH attribute.
static bool ATTR_CLASH(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint32_t t = g_rgb_timer;
    bool flash = (t / 320) & 1;
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t x = g_led_config.point[i].x, y = g_led_config.point[i].y;
        uint16_t cell = (x / 32) + (y / 22) * 8;
        uint16_t epoch = (uint16_t)((t + cell * 377) / 1500);          // each cell changes at its own time
        uint16_t h = zx_hash(cell * 131 + epoch);
        uint8_t ink = h % 7, paper = (ink + 1 + (h >> 4) % 6) % 7;     // never the same as ink
        bool pix = ((x / 11) + (y / 13)) & 1;                           // checker the keys inside a cell
        uint8_t c = (pix ^ flash) ? ink : paper;
        zx_set(i, zx_bright[c][0], zx_bright[c][1], zx_bright[c][2]);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// Typing ripple: each key press lights the key and sends a ring outwards.
// Colours cycle red, yellow, green, cyan with every press.
static bool TYPING_RIPPLE(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint32_t now = g_rgb_timer;
    uint8_t  size = zxkb_size(), intensity = zxkb_intensity();
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t best = 0, bc = 0;
        int16_t x = g_led_config.point[i].x, y = g_led_config.point[i].y;
        for (uint8_t r = 0; r < ZXKB_RIPPLES; r++) {
            if (zxkb_rip_led[r] == 0xFF) continue;
            uint32_t age = now - zxkb_rip_time[r];
            if (age > 900) continue;
            int16_t dx = x - g_led_config.point[zxkb_rip_led[r]].x, dy = (y - g_led_config.point[zxkb_rip_led[r]].y) * 2;
            uint16_t dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
            uint16_t radius = age * size / 512;                          // ring grows ~250 units per second at size 128
            uint16_t gap = dist > radius ? dist - radius : radius - dist;
            uint8_t v = 0;
            if (dist < 6 && age < 500) v = 255 - age / 2;                // the key itself
            else if (gap < 10) v = (uint8_t)((900 - age) * 255 / 900);   // the ring, fading out
            if (v > best) { best = v; bc = zxkb_rip_col[r]; }
        }
        best = scale8(best, intensity);
        zx_set(i, scale8(zx_cols[bc][0], best), scale8(zx_cols[bc][1], best), scale8(zx_cols[bc][2], best));
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// Logo sweep: upright red | yellow | green | cyan bands sliding left to right
static bool LOGO_SWEEP(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint16_t shift = scale16by8(g_rgb_timer, rgb_matrix_config.speed / 8) % 232;
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t band = ((g_led_config.point[i].x + 232 - shift) / 58) % 4;
        zx_set(i, zx_cols[band][0], zx_cols[band][1], zx_cols[band][2]);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#ifdef RGB_MATRIX_KEYREACTIVE_ENABLED
// Key explosion: each key press fires a small four-pointed star that spreads out from the key,
// starts white-hot, drifts in colour (from the chosen colour onwards) and fades to black.
// Speed sets how long a star lives: 0.5 s (fastest) to 1.5 s. Size sets how far it spreads (about 3 keys at
// 128), intensity how bright it is.
static bool KEY_EXPLOSION(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint16_t life  = 500 + (uint16_t)(255 - rgb_matrix_config.speed) * 4;
    uint8_t  count = g_last_hit_tracker.count;
    uint16_t reach_max = 42 * (uint16_t)zxkb_size() / 128;                    // 42 units (about 3 keys) at size 128
    uint8_t  intensity = zxkb_intensity();
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        int16_t  x = g_led_config.point[i].x, y = g_led_config.point[i].y;
        uint8_t  best = 0;
        uint16_t best_tick = 0;
        for (uint8_t j = 0; j < count; j++) {
            uint16_t tick = g_last_hit_tracker.tick[j];
            if (tick >= life) continue;
            int16_t  dx = x - g_last_hit_tracker.x[j], dy = y - g_last_hit_tracker.y[j];
            uint16_t radius = 6 + (uint32_t)tick * reach_max / life;        // grows to about 3 keys away at size 128
            if (dx > (int16_t)radius || -dx > (int16_t)radius || dy > (int16_t)radius || -dy > (int16_t)radius) continue;
            uint8_t  dist  = sqrt16((uint16_t)(dx * dx + dy * dy));
            uint8_t  ray   = cos8((uint8_t)(atan2_8(dy, dx) * 4));        // 255 along the 4 points, 0 between
            uint16_t reach = radius / 2 + (uint32_t)radius * ray / 510;     // star outline
            if (reach > 255) reach = 255;
            if (dist > reach) continue;
            uint8_t fade = 255 - (uint32_t)tick * 255 / life;
            uint8_t edge = reach ? 96 + (uint16_t)dist * 159 / reach : 255; // brightest at the spreading edge
            uint8_t v    = scale8(fade, tick < 80 ? 255 : edge);
            if (v > best) { best = v; best_tick = tick; }
        }
        if (!best) { rgb_matrix_set_color(i, 0, 0, 0); continue; }
        hsv_t   hsv = rgb_matrix_config.hsv;
        uint8_t age = (uint32_t)best_tick * 255 / life;
        hsv.h += scale8(age, 96);                                           // colour drifts as it spreads
        if (age < 40) hsv.s = scale8(hsv.s, age * 6);                       // white-hot at first
        hsv.v = scale8(scale8(best, intensity), hsv.v);
        rgb_t rgb = rgb_matrix_hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// ---------- ZXKB versions of QMK's typing effects, with size and intensity ----------
// The maths are QMK's own (quantum/rgb_matrix/animations, GPL-2.0-or-later), so at size 128 and intensity 255
// they look exactly like QMK's. Size scales the distance from the pressed key (bigger = spreads further);
// intensity scales how bright the reaction is.
typedef hsv_t (*zx_splash_f)(hsv_t hsv, int16_t dx, int16_t dy, uint8_t dist, uint16_t tick);

static int16_t zx_clamp(int32_t v) { return v < -1024 ? -1024 : v > 1024 ? 1024 : (int16_t)v; }

static bool zx_splash_runner(bool all_hits, effect_params_t* params, zx_splash_f fn) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t count = g_last_hit_tracker.count, start = all_hits ? 0 : qsub8(count, 1);
    uint8_t size = zxkb_size(), intensity = zxkb_intensity();
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        hsv_t hsv = rgb_matrix_config.hsv;
        hsv.v     = 0;
        for (uint8_t j = start; j < count; j++) {
            int32_t  dx   = ((int32_t)g_led_config.point[i].x - g_last_hit_tracker.x[j]) * 128 / size;
            int32_t  dy   = ((int32_t)g_led_config.point[i].y - g_last_hit_tracker.y[j]) * 128 / size;
            int32_t  d2   = dx * dx + dy * dy;
            uint8_t  dist = d2 >= 65025 ? 255 : sqrt16((uint16_t)d2);
            uint16_t tick = scale16by8(g_last_hit_tracker.tick[j], qadd8(rgb_matrix_config.speed, 1));
            hsv           = fn(hsv, zx_clamp(dx), zx_clamp(dy), dist, tick);
        }
        hsv.v     = scale8(scale8(hsv.v, intensity), rgb_matrix_config.hsv.v);
        rgb_t rgb = rgb_matrix_hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

static hsv_t zx_wide_math(hsv_t hsv, int16_t dx, int16_t dy, uint8_t dist, uint16_t tick) {
    uint16_t effect = tick + dist * 5;
    if (effect > 255) effect = 255;
#    ifdef RGB_MATRIX_SOLID_REACTIVE_GRADIENT_MODE
    hsv.h = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed, 8) >> 4);
#    endif
    hsv.v = qadd8(hsv.v, 255 - effect);
    return hsv;
}
static hsv_t zx_cross_math(hsv_t hsv, int16_t dx, int16_t dy, uint8_t dist, uint16_t tick) {
    uint16_t effect = tick + dist;
    dx              = dx < 0 ? dx * -1 : dx;
    dy              = dy < 0 ? dy * -1 : dy;
    dx              = dx * 16 > 255 ? 255 : dx * 16;
    dy              = dy * 16 > 255 ? 255 : dy * 16;
    effect += dx > dy ? dy : dx;
    if (effect > 255) effect = 255;
#    ifdef RGB_MATRIX_SOLID_REACTIVE_GRADIENT_MODE
    hsv.h = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed, 8) >> 4);
#    endif
    hsv.v = qadd8(hsv.v, 255 - effect);
    return hsv;
}
static hsv_t zx_nexus_math(hsv_t hsv, int16_t dx, int16_t dy, uint8_t dist, uint16_t tick) {
    uint16_t effect = tick - dist;
    if (effect > 255) effect = 255;
    if (dist > 72) effect = 255;
    if ((dx > 8 || dx < -8) && (dy > 8 || dy < -8)) effect = 255;
#    ifdef RGB_MATRIX_SOLID_REACTIVE_GRADIENT_MODE
    hsv.h = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed, 8) >> 4) + dy / 4;
#    else
    hsv.h = rgb_matrix_config.hsv.h + dy / 4;
#    endif
    hsv.v = qadd8(hsv.v, 255 - effect);
    return hsv;
}
static hsv_t zx_splash_math(hsv_t hsv, int16_t dx, int16_t dy, uint8_t dist, uint16_t tick) {
    uint16_t effect = tick - dist;
    if (effect > 255) effect = 255;
    hsv.h += effect;
    hsv.v = qadd8(hsv.v, 255 - effect);
    return hsv;
}
static hsv_t zx_solid_splash_math(hsv_t hsv, int16_t dx, int16_t dy, uint8_t dist, uint16_t tick) {
    uint16_t effect = tick - dist;
    if (effect > 255) effect = 255;
    hsv.v = qadd8(hsv.v, 255 - effect);
    return hsv;
}
static bool ZX_REACTIVE_WIDE(effect_params_t* params) { return zx_splash_runner(false, params, &zx_wide_math); }
static bool ZX_REACTIVE_MULTIWIDE(effect_params_t* params) { return zx_splash_runner(true, params, &zx_wide_math); }
static bool ZX_REACTIVE_CROSS(effect_params_t* params) { return zx_splash_runner(false, params, &zx_cross_math); }
static bool ZX_REACTIVE_MULTICROSS(effect_params_t* params) { return zx_splash_runner(true, params, &zx_cross_math); }
static bool ZX_REACTIVE_NEXUS(effect_params_t* params) { return zx_splash_runner(false, params, &zx_nexus_math); }
static bool ZX_REACTIVE_MULTINEXUS(effect_params_t* params) { return zx_splash_runner(true, params, &zx_nexus_math); }
static bool ZX_SPLASH(effect_params_t* params) { return zx_splash_runner(false, params, &zx_splash_math); }
static bool ZX_MULTISPLASH(effect_params_t* params) { return zx_splash_runner(true, params, &zx_splash_math); }
static bool ZX_SOLID_SPLASH(effect_params_t* params) { return zx_splash_runner(false, params, &zx_solid_splash_math); }
static bool ZX_SOLID_MULTISPLASH(effect_params_t* params) { return zx_splash_runner(true, params, &zx_solid_splash_math); }

// Per-key reactive effects (QMK's effect_runner_reactive): only intensity applies, as they light just the key
typedef hsv_t (*zx_reactive_f)(hsv_t hsv, uint16_t offset, uint8_t intensity);
static bool zx_reactive_runner(effect_params_t* params, zx_reactive_f fn) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint16_t max_tick  = 65535 / qadd8(rgb_matrix_config.speed, 1);
    uint8_t  intensity = zxkb_intensity();
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint16_t tick = max_tick;
        for (int8_t j = g_last_hit_tracker.count - 1; j >= 0; j--) { // most recent hit on this key
            if (g_last_hit_tracker.index[j] == i && g_last_hit_tracker.tick[j] < tick) {
                tick = g_last_hit_tracker.tick[j];
                break;
            }
        }
        uint16_t offset = scale16by8(tick, qadd8(rgb_matrix_config.speed, 1));
        rgb_t    rgb    = rgb_matrix_hsv_to_rgb(fn(rgb_matrix_config.hsv, offset, intensity));
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}
static hsv_t zx_reactive_simple_math(hsv_t hsv, uint16_t offset, uint8_t intensity) {
    if (offset > 255) offset = 255;
#    ifdef RGB_MATRIX_SOLID_REACTIVE_GRADIENT_MODE
    hsv.h = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed, 8) >> 4);
#    endif
    hsv.v = scale8(scale8(255 - offset, intensity), hsv.v);
    return hsv;
}
static hsv_t zx_reactive_math(hsv_t hsv, uint16_t offset, uint8_t intensity) { // intensity = how far the colour shifts
    if (offset > 255) offset = 255;
#    ifdef RGB_MATRIX_SOLID_REACTIVE_GRADIENT_MODE
    hsv.h = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed, 8) >> 4);
#    endif
    hsv.h += scale8(scale8(255 - offset, 64), intensity);
    return hsv;
}
static bool ZX_REACTIVE_SIMPLE(effect_params_t* params) { return zx_reactive_runner(params, &zx_reactive_simple_math); }
static bool ZX_REACTIVE(effect_params_t* params) { return zx_reactive_runner(params, &zx_reactive_math); }
#endif // RGB_MATRIX_KEYREACTIVE_ENABLED

// PC direct: per-LED colours streamed from the PC (ZXKB app or SignalRGB), scaled by the brightness setting
static bool PC_DIRECT(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t v = rgb_matrix_config.hsv.v;
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        rgb_matrix_set_color(i, scale8(zxkb_direct[i][0], v), scale8(zxkb_direct[i][1], v), scale8(zxkb_direct[i][2], v));
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#endif
