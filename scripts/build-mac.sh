#!/usr/bin/env bash
# Cross-compile the Mac targets. Usage: scripts/build-mac.sh [ppc|68k]
# Output: build-mac-<arch>/mac/JitterbugCollector.{bin,dsk,APPL}
set -euo pipefail
source "$(dirname "$0")/common.sh"
ARCH="${1:-ppc}"
case "$ARCH" in
    ppc) TRIPLE=powerpc-apple-macos; TCFILE=retroppc.toolchain.cmake ;;
    68k) TRIPLE=m68k-apple-macos;    TCFILE=retro68.toolchain.cmake ;;
    *) echo "usage: $0 [ppc|68k]"; exit 2 ;;
esac
OUT="build-mac-$ARCH"

if [[ -n "$RETRO68_TOOLCHAIN" ]]; then
    cmake -S "$JB_ROOT" -B "$JB_ROOT/$OUT" \
        -DCMAKE_TOOLCHAIN_FILE="$RETRO68_TOOLCHAIN/$TRIPLE/cmake/$TCFILE"
    cmake --build "$JB_ROOT/$OUT" -j
else
    docker run --rm -v "$JB_ROOT":/src -w /src "$RETRO68_IMAGE" /bin/bash -c "
        set -e
        cmake -S /src -B /src/$OUT -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/$TRIPLE/cmake/$TCFILE
        cmake --build /src/$OUT -j"
fi
ls -l "$JB_ROOT/$OUT/mac/" | grep -E 'JitterbugCollector' || true
