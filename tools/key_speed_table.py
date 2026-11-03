"""Per-key effect step times (firmware key_step_ms, app proto::key_step_ms): 5000 ms x (16 / 5000) ^ (speed / 255).

    python tools/key_speed_table.py

16 ms is QMK's RGB_MATRIX_LED_FLUSH_LIMIT (the fastest the LEDs are redrawn); 5 s the slow end D0k asked for; speed 122
gives 320 ms, the Spectrum FLASH rate. Prints the C table to paste into firmware/modules/d0k/zxkb/zxkb.c.
"""
table = [round(5000 * (16 / 5000) ** (s / 255)) for s in range(256)]
assert table[0] == 5000 and table[122] == 320 and table[255] == 16
print(',\n'.join(', '.join(str(v) for v in table[i:i + 16]) for i in range(0, 256, 16)))
