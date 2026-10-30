#!/usr/bin/env bash
# Get a Retro68 toolchain. Default: pull the official prebuilt Docker image
# (68K + PowerPC, Multiversal Interfaces). Pass --native to build from source
# instead (slow: GCC and binutils are built twice).
set -euo pipefail
source "$(dirname "$0")/common.sh"

if [[ "${1:-}" != "--native" ]]; then
    command -v docker >/dev/null || { echo "docker not found; install it or run: $0 --native"; exit 1; }
    docker pull "$RETRO68_IMAGE"
    echo "OK. scripts/build-mac.sh will use $RETRO68_IMAGE."
    exit 0
fi

SRC="${RETRO68_SRC:-$HOME/Retro68}"
BUILD="${RETRO68_BUILD:-$HOME/Retro68-build}"
[[ -d "$SRC" ]] || git clone --recursive https://github.com/autc04/Retro68.git "$SRC"
git -C "$SRC" submodule update --init
mkdir -p "$BUILD"
# Keep 68K: the INIT (milestone 3) is a 68K code resource. Carbon is not needed.
# For Open Transport headers (TCP trace upload, net-jitter source) you will need
# Apple's Universal Interfaces in $SRC/InterfacesAndLibraries before building.
( cd "$BUILD" && "$SRC/build-toolchain.bash" --no-carbon )
echo
echo "Done. Add to your shell profile:"
echo "  export RETRO68_TOOLCHAIN=$BUILD/toolchain"
echo "  export PATH=\"\$RETRO68_TOOLCHAIN/bin:\$PATH\""
