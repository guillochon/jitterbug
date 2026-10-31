#!/usr/bin/env bash
# Boot Mac OS 9.2.2 in a QEMU window to try Jitterbug by hand. Linux or WSL
# (WSLg shows the window); needs qemu-system-ppc and hfsutils on PATH:
#   sudo apt-get install qemu-system-ppc hfsutils
#
#   scripts/play-qemu-os9.sh           # reuse play.img, keeping your changes
#   scripts/play-qemu-os9.sh --fresh   # rebuild play.img from the base disk
#
# play.img is a copy of the base disk made by `test-qemu-os9.sh install`, with
# the Jitterbug extension in Extensions and JitterbugDemo + JitterbugTest in a
# "Jitterbug" folder on the disk. --fresh also refreshes them from the build dirs.
# There is no AutoTest marker, so nothing shuts the guest down for you.
# Click in the window to capture the mouse; Ctrl+Alt+G releases it.
# Emulator timing is never entropy evidence (CLAUDE.md rule 3).
set -euo pipefail
source "$(dirname "$0")/common.sh"

QDIR="${JB_QEMU_DIR:-$HOME/.cache/jitterbug-qemu}"
BASE="$QDIR/os9-base.img"
PLAY="$QDIR/play.img"
INIT="$JB_ROOT/build-mac-68k/mac/Jitterbug.bin"
APPS="$JB_ROOT/build-mac-ppc/mac"

command -v qemu-system-ppc >/dev/null && command -v hmount >/dev/null || {
    echo "needs qemu-system-ppc and hfsutils (sudo apt-get install qemu-system-ppc hfsutils)"; exit 2; }
[[ -f "$BASE" ]] || { echo "no base disk at $BASE; run: scripts/test-qemu-os9.sh install ISO"; exit 2; }

if [[ "${1:-}" == "--fresh" || ! -f "$PLAY" ]]; then
    [[ -f "$INIT" ]] || { echo "missing $INIT; run: scripts/build-mac.sh 68k"; exit 2; }
    [[ -f "$APPS/JitterbugDemo.bin" ]] || { echo "missing $APPS/JitterbugDemo.bin; run: scripts/build-mac.sh ppc"; exit 2; }
    echo "building $PLAY"
    cp "$BASE" "$PLAY"
    export HOME="$(mktemp -d)"   # hfsutils keeps its mount state in $HOME/.hcwd
    hmount "$PLAY" >/dev/null
    hcopy -m "$INIT" ':System Folder:Extensions:Jitterbug'
    hmkdir ':Jitterbug'
    hcopy -m "$APPS/JitterbugDemo.bin" ':Jitterbug:JitterbugDemo'
    hcopy -m "$APPS/JitterbugTest.bin" ':Jitterbug:JitterbugTest'
    humount
fi

exec qemu-system-ppc -M mac99,via=pmu -m 512 -g 1024x768x32 \
    -drive file="$PLAY",format=raw,media=disk -boot c -name Jitterbug
