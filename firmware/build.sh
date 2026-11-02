#!/bin/sh
# Build the ZXKB firmware for D0k's Keychron Q3 ISO knob. Run in the QMK MSYS shell:
#   sh /h/ZXKB/firmware/build.sh            build only
#   sh /h/ZXKB/firmware/build.sh release    build, then keep a copy of this version for fallback
# Uses this folder as a QMK userspace, so nothing is copied into qmk_firmware.
# Output: ../build/ZXKB_Q3_v<N>.bin, where N is ZXKB_FIRMWARE_VERSION in the keymap's config.h.
# Released versions are kept in Documents\Keyboard Layouts\Q3 firmware\ZXKB versions\ (see docs/VERSIONS.md).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
CONFIG="$HERE/keyboards/keychron/q3/iso_encoder/keymaps/d0k/config.h"
VER=$(sed -n 's/^#define ZXKB_FIRMWARE_VERSION \([0-9]*\).*/\1/p' "$CONFIG")
[ -n "$VER" ] || { echo "ZXKB_FIRMWARE_VERSION not found in $CONFIG"; exit 1; }
NAME="ZXKB_Q3_v$VER.bin"

qmk config user.overlay_dir="$(cd "$HERE" && pwd -W)" >/dev/null
cd ~/qmk_firmware
qmk compile -kb keychron/q3/iso_encoder -km d0k
mkdir -p "$HERE/../build"
cp .build/keychron_q3_iso_encoder_d0k.bin "$HERE/../build/$NAME"
arm-none-eabi-size -A .build/keychron_q3_iso_encoder_d0k.elf | grep -E '^\.(vectors|text|rodata|data|bss) '
ls -l "$HERE/../build/$NAME"

if [ "$1" = "release" ]; then
    ARCHIVE="$(cygpath -u "$USERPROFILE")/Documents/Keyboard Layouts/Q3 firmware/ZXKB versions"
    mkdir -p "$ARCHIVE"
    if [ -f "$ARCHIVE/$NAME" ]; then
        if cmp -s "$ARCHIVE/$NAME" "$HERE/../build/$NAME"; then
            echo "v$VER is already kept, unchanged"
        else
            echo "v$VER is already kept with different contents: raise ZXKB_FIRMWARE_VERSION in $CONFIG"
            exit 1
        fi
    else
        cp "$HERE/../build/$NAME" "$ARCHIVE/$NAME"
        echo "kept: $ARCHIVE/$NAME"
    fi
fi
