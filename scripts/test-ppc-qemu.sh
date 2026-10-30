#!/usr/bin/env bash
# Run the core + simulator tests big-endian on an emulated PowerPC 750 (the
# B&W's G3 CPU) using a Linux PPC32 cross-compiler and qemu-user. This catches
# endianness and 32-bit bugs before touching real hardware. It does NOT test
# Mac OS APIs. Debian/Ubuntu: apt install gcc-powerpc-linux-gnu qemu-user
# Use Ubuntu 22.04 (glibc 2.35), where both suites pass. Debian 12's cross
# glibc puts a POWER9-only instruction (mffscrni) in cos(), so test_sim dies
# with SIGILL under -cpu 750; that is the toolchain, not Jitterbug.
set -euo pipefail
source "$(dirname "$0")/common.sh"
CC=powerpc-linux-gnu-gcc
command -v $CC >/dev/null && command -v qemu-ppc >/dev/null || {
    echo "needs $CC and qemu-ppc (apt install gcc-powerpc-linux-gnu qemu-user)"; exit 2; }
OUT="$JB_ROOT/build-ppc-qemu"; mkdir -p "$OUT"
cd "$JB_ROOT"
$CC -mcpu=750 -static -std=c89 -pedantic -Wall -Wextra -Werror -Iinclude core/*.c tests/test_core.c -lm -o "$OUT/test_core"
$CC -mcpu=750 -static -std=c99 -Iinclude core/*.c sim/jb_sim.c tests/test_sim.c -lm -o "$OUT/test_sim"
qemu-ppc -cpu 750 "$OUT/test_core"
qemu-ppc -cpu 750 "$OUT/test_sim"
