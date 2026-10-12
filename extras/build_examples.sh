#!/bin/sh
# Compiles every example for every board it supports, with PlatformIO.
# Compile chaque exemple pour chaque carte qu'il prend en charge.
#   sh extras/build_examples.sh
# Uses the environments of platformio.ini and overrides src_dir with
# PLATFORMIO_SRC_DIR. A missing secrets.h is created from secrets.h.example
# for the build, then removed.
# Builds go to their own directory (PLATFORMIO_BUILD_DIR), so that VS Code,
# which may rebuild its index in .pio/build at the same time, cannot
# delete files in the middle of a build. A failed build is retried once.

cd "$(dirname "$0")/.." || exit 1
failed=""
export PLATFORMIO_BUILD_DIR="${TMPDIR:-/tmp}/railuino_examples_build"
LOG="${TMPDIR:-/tmp}/railuino_build.log"

build() { PLATFORMIO_SRC_DIR="$1" pio run -s -e "$2" > "$LOG" 2>&1; }

for dir in examples/*/*/; do
    dir=${dir%/}
    name=$(basename "$dir")
    case "$name" in
        WebControl | TcpCommand | Mcp2515OnEsp32) envs="esp32dev" ;;
        *) envs="esp32dev uno megaatmega2560" ;;
    esac

    temp_secrets=""
    if [ -f "$dir/secrets.h.example" ] && [ ! -f "$dir/secrets.h" ]; then
        cp "$dir/secrets.h.example" "$dir/secrets.h"
        temp_secrets="$dir/secrets.h"
    fi

    for env in $envs; do
        printf '%-45s %-16s ' "$dir" "$env"
        if build "$dir" "$env" || build "$dir" "$env"; then
            echo "OK"
        else
            echo "FAILED"
            failed="$failed\n  $dir ($env)"
            tail -20 "$LOG"
        fi
    done

    [ -n "$temp_secrets" ] && rm -f "$temp_secrets"
done

if [ -n "$failed" ]; then
    printf "\nFailed:$failed\n"
    exit 1
fi
echo "All examples compile."
