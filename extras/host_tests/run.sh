#!/bin/sh
# Compiles and runs the host tests of the phase 1 fixes on a PC (Linux,
# macOS, WSL), without any board: Arduino and ACAN are replaced by stubs
# that simulate the clock and the CAN bus.
#   sh extras/host_tests/run.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src"
OUT="${TMPDIR:-/tmp}/railuino_host_tests"
c++ -std=gnu++11 -Wall -Wextra -DARDUINO_ARCH_ESP32 \
    -I"$HERE/stubs" -I"$SRC" \
    "$HERE/tests.cpp" "$SRC/TrackController.cpp" "$SRC/TrackMessage.cpp" "$SRC/TrackReporterS88.cpp" \
    -o "$OUT"
"$OUT"
# The AVR flavour (ACAN2515) is only compiled, to catch platform-specific errors.
for f in TrackController TrackMessage TrackReporterS88; do
    c++ -std=gnu++11 -Wall -Wextra -DARDUINO_ARCH_AVR -I"$HERE/stubs" -I"$SRC" -c "$SRC/$f.cpp" -o /dev/null
done
echo "AVR flavour compiles"

# Examples: each sketch is compiled (not linked) with both flavours, to check
# that it only needs <Railuino.h>. ESP32-only sketches (WiFi) are skipped.
for ino in "$HERE"/../../examples/*/*/*.ino; do
    name=$(basename "$ino" .ino)
    case "$name" in WebControl | TcpCommand) continue ;; esac
    for arch in ARDUINO_ARCH_ESP32 ARDUINO_ARCH_AVR; do
        printf '#include <Arduino.h>\n#include "%s"\n' "$ino" > "$OUT.example.cpp"
        c++ -std=gnu++11 -Wall -Wextra -D$arch -I"$HERE/stubs" -I"$SRC" -c "$OUT.example.cpp" -o /dev/null
    done
    echo "example $name compiles"
done
