#!/usr/bin/env bash
# Boot-test the Jitterbug extension in QEMU (PowerPC mac99, Mac OS 9.2.2).
# This catches crashes and wiring mistakes before anything goes on the B&W.
# Emulator timing says NOTHING about entropy (CLAUDE.md rule 3): never use
# these runs to justify a credit.
#
#   scripts/test-qemu-os9.sh install PATH/TO/MacOS9.iso   # once, interactive
#   scripts/test-qemu-os9.sh run [BOOTS] [--stimulus]     # scripted, headless
#
# install: creates a blank 2 GB disk and boots the install CD. In the guest:
#   1. Drive Setup: select the ATA disk, Initialize, and set the format to
#      "Mac OS Standard" (hfsutils cannot write Mac OS Extended / HFS+).
#   2. Install or restore Mac OS 9 onto it, then shut the guest down.
#   The script then checks that the disk is HFS and readable by hfsutils.
#   Uses a native qemu-system-ppc window if one is on PATH; otherwise runs
#   QEMU in Docker with VNC on 127.0.0.1:5900.
#
# run: copies the base disk, adds build-mac-68k/mac/Jitterbug to Extensions
#   and the PPC JitterbugTest (with an "AutoTest" marker) to Startup Items,
#   boots headless BOOTS times (default 2, so the second boot reads the seed
#   file the first one wrote), and prints the test log after each boot.
#   Screenshots go to $JB_QEMU_DIR/out/boot-N/.
#   With no one at the keyboard the extension may never seed, and the log
#   then shows it failing closed. --stimulus types letters into the guest at
#   host-CSPRNG intervals so the event path gets exercised; that timing is
#   test input, never entropy evidence.
#
# Disk images live in $JB_QEMU_DIR (default ~/.cache/jitterbug-qemu), never
# in the repository: they contain Apple software.
set -euo pipefail
source "$(dirname "$0")/common.sh"

QIMAGE="${JB_QEMU_IMAGE:-jitterbug-qemu}"
QDIR="${JB_QEMU_DIR:-$HOME/.cache/jitterbug-qemu}"
BASE="$QDIR/os9-base.img"
TIMEOUT="${JB_QEMU_TIMEOUT:-600}"
QEMU_MACHINE=(-M mac99,via=pmu -m 512 -g 1024x768x32)
mkdir -p "$QDIR"

# Docker on Windows needs Windows paths and no MSYS path mangling.
winpath() { if command -v cygpath >/dev/null; then cygpath -w "$1"; else echo "$1"; fi; }
export MSYS_NO_PATHCONV=1

ensure_image() {
    docker image inspect "$QIMAGE" >/dev/null 2>&1 ||
        docker build -t "$QIMAGE" "$JB_ROOT/scripts/qemu"
}

dock() {   # dock [docker-run-args...] -- command...
    local args=()
    while [[ "$1" != "--" ]]; do args+=("$1"); shift; done; shift
    docker run --rm -v "$(winpath "$QDIR")":/q -v "$(winpath "$JB_ROOT")":/src:ro \
        "${args[@]}" "$QIMAGE" bash -c "$*"
}

check_hfs() {   # check_hfs IMAGE-PATH-IN-CONTAINER
    dock -- "hmount '$1' >/dev/null && hls ':System Folder:Extensions' >/dev/null && echo 'disk OK: HFS with a System Folder'; hvol; humount"
}

cmd_install() {
    local iso="${1:?usage: $0 install PATH/TO/MacOS9.iso}"
    [[ -f "$iso" ]] || { echo "no such file: $iso"; exit 2; }
    ensure_image
    [[ -f "$BASE" ]] && { echo "$BASE already exists; delete it to reinstall"; exit 2; }
    dock -- "qemu-img create -f raw /q/os9-base.img 2G"
    if command -v qemu-system-ppc >/dev/null; then
        qemu-system-ppc "${QEMU_MACHINE[@]}" -cdrom "$iso" \
            -drive file="$BASE",format=raw,media=disk -boot d
    else
        echo "Connect a VNC viewer to 127.0.0.1:5900. Shut the guest down when done."
        dock -p 127.0.0.1:5900:5900 -v "$(winpath "$iso")":/iso/os9.iso:ro -- \
            "qemu-system-ppc ${QEMU_MACHINE[*]} -cdrom /iso/os9.iso \
             -drive file=/q/os9-base.img,format=raw,media=disk -boot d -vnc 0.0.0.0:0"
    fi
    check_hfs /q/os9-base.img
}

cmd_run() {
    local boots=2 stim="" i
    for arg in "$@"; do
        case "$arg" in
            --stimulus) stim="--stimulus" ;;
            *) boots="$arg" ;;
        esac
    done
    local init="$JB_ROOT/build-mac-68k/mac/Jitterbug.bin"
    local app="$JB_ROOT/build-mac-ppc/mac/JitterbugTest.bin"
    [[ -f "$BASE" ]] || { echo "no base disk; run: $0 install PATH/TO/MacOS9.iso"; exit 2; }
    [[ -f "$init" ]] || { echo "missing $init; run: scripts/build-mac.sh 68k"; exit 2; }
    [[ -f "$app" ]] || { echo "missing $app; run: scripts/build-mac.sh ppc"; exit 2; }
    ensure_image
    rm -rf "$QDIR/out"
    dock -- "set -e
        cp /q/os9-base.img /q/run.img
        hmount /q/run.img >/dev/null
        hcopy -m /src/build-mac-68k/mac/Jitterbug.bin ':System Folder:Extensions:Jitterbug'
        hcopy -m /src/build-mac-ppc/mac/JitterbugTest.bin ':System Folder:Startup Items:JitterbugTest'
        : > /tmp/empty
        hcopy -r /tmp/empty ':System Folder:Startup Items:Jitterbug AutoTest'
        humount"
    for ((i = 1; i <= boots; i++)); do
        echo "=== boot $i of $boots (timeout ${TIMEOUT}s${stim:+, keystroke stimulus})"
        dock -- "set -e
            qemu-system-ppc ${QEMU_MACHINE[*]} -drive file=/q/run.img,format=raw,media=disk \
                -boot c -display none -monitor unix:/tmp/mon,server,nowait &
            q=\$!
            sleep 2
            python3 /src/scripts/qemu/monitor.py /tmp/mon /q/out/boot-$i 90 $stim &
            ( sleep $TIMEOUT; kill \$q 2>/dev/null && echo 'TIMEOUT: guest did not power off' ) &
            wait \$q || true
            hmount /q/run.img >/dev/null
            echo '--- Jitterbug Log (extension startup)'
            hcopy -t ':System Folder:Preferences:Jitterbug Log' - 2>/dev/null || echo '(none: the INIT did not run)'
            echo '--- Jitterbug Test Log'
            hcopy -t ':System Folder:Startup Items:Jitterbug Test Log' - 2>/dev/null || echo '(no log written)'
            echo '--- Preferences seed file'
            hls -l ':System Folder:Preferences' | grep 'Jitterbug Seed' || echo '(no seed file)'
            hdel ':System Folder:Startup Items:Jitterbug Test Log' 2>/dev/null || true
            humount"
    done
    echo "Screenshots: $QDIR/out"
}

case "${1:-}" in
    install) shift; cmd_install "$@" ;;
    run)     shift; cmd_run "$@" ;;
    *) sed -n 2,30p "$0"; exit 2 ;;
esac
