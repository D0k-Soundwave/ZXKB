#!/bin/sh
# VU comparison: old ZX VU maths (real legacy code) vs ZXKB's C++ port, on the same recording.
# Run in the QMK MSYS shell: sh run_vu_test.sh <work folder>
# PYTHON: a Windows Python with numpy (the old app's maths need it); defaults to "py".
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
W="${1:?work folder}"
PY="${PYTHON:-py}"
mkdir -p "$W"
"$PY" "$HERE/make_test_audio.py" "$W/test.f32"
g++ -O2 -std=c++17 -Wall -Wextra -static -o "$W/vu_compare.exe" "$HERE/vu_compare.cpp" "$HERE/../src/vumath.cpp"
rc=0
for s in bands abc; do
    "$PY" "$HERE/vu_reference.py" "$W/test.f32" $s "$HERE/../../legacy/zx_vu.py" > "$W/old_$s.txt"
    "$W/vu_compare.exe" "$W/test.f32" $s > "$W/new_$s.txt"
    echo "$s:"; "$PY" "$HERE/compare.py" "$W/old_$s.txt" "$W/new_$s.txt" || rc=1
done
[ $rc = 0 ] && echo "PASS" || echo "FAIL"
exit $rc
