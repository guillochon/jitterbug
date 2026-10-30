#!/usr/bin/env bash
# Send an app to the B&W and launch it via LaunchAPPLServer over TCP.
#   JB_MAC_IP=192.168.1.42 scripts/deploy.sh [path/to/App.bin]
# On the Mac: run LaunchAPPLServer (built by Retro68) with TCP enabled.
# SECURITY: LaunchAPPLServer runs ANY app sent to it, unauthenticated.
# Only use it on a trusted LAN behind a firewall.
set -euo pipefail
source "$(dirname "$0")/common.sh"
APP="${1:-$JB_ROOT/build-mac-ppc/mac/JitterbugCollector.bin}"
[[ -n "$JB_MAC_IP" ]] || { echo "set JB_MAC_IP to the B&W's IP address"; exit 2; }
[[ -f "$APP" ]] || { echo "no such file: $APP (run scripts/build-mac.sh first)"; exit 2; }

if command -v LaunchAPPL >/dev/null; then
    LaunchAPPL -e tcp --tcp-address "$JB_MAC_IP" "$APP"
elif [[ -n "$RETRO68_TOOLCHAIN" && -x "$RETRO68_TOOLCHAIN/bin/LaunchAPPL" ]]; then
    "$RETRO68_TOOLCHAIN/bin/LaunchAPPL" -e tcp --tcp-address "$JB_MAC_IP" "$APP"
else
    REL="${APP#$JB_ROOT/}"
    docker run --rm --network host -v "$JB_ROOT":/src -w /src "$RETRO68_IMAGE" \
        /Retro68-build/toolchain/bin/LaunchAPPL -e tcp --tcp-address "$JB_MAC_IP" "/src/$REL"
fi
