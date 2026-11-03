VIA_ENABLE = yes
ENCODER_MAP_ENABLE = yes
# Copy every colour the LED driver sets, for ZXKB's live preview (see __wrap_snled27351_set_color in keymap.c)
EXTRALDFLAGS += -Wl,--wrap=snled27351_set_color -Wl,--wrap=snled27351_set_color_all
