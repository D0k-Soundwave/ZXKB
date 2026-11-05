VIA_ENABLE = yes
ENCODER_MAP_ENABLE = yes
# adjustable debounce: the ZXKB module's copy of QMK's standard debounce, with the time set from the app
DEBOUNCE_TYPE = custom
OPT_DEFS += -DZXKB_DEBOUNCE
# Copy every colour the LED driver sets, for ZXKB's live preview (see __wrap_snled27351_set_color in keymap.c)
EXTRALDFLAGS += -Wl,--wrap=snled27351_set_color -Wl,--wrap=snled27351_set_color_all
