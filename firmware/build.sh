#!/bin/sh
# Build the ZXKB firmware for D0k's Keychron Q3 ISO knob. Run in the QMK MSYS shell:
#   sh /h/ZXKB/firmware/build.sh
# Uses this folder as a QMK userspace, so nothing is copied into qmk_firmware.
# The finished .bin is copied to ../build/.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
qmk config user.overlay_dir="$(cd "$HERE" && pwd -W)" >/dev/null
cd ~/qmk_firmware
qmk compile -kb keychron/q3/iso_encoder -km d0k
mkdir -p "$HERE/../build"
cp .build/keychron_q3_iso_encoder_d0k.bin "$HERE/../build/"
arm-none-eabi-size -A .build/keychron_q3_iso_encoder_d0k.elf | grep -E '^\.(vectors|text|rodata|data|bss) '
ls -l "$HERE/../build/keychron_q3_iso_encoder_d0k.bin"
