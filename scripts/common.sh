# Shared settings, sourced by the other scripts.
JB_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RETRO68_IMAGE="${RETRO68_IMAGE:-ghcr.io/autc04/retro68}"
# Set RETRO68_TOOLCHAIN to a native install (…/Retro68-build/toolchain) to skip Docker.
RETRO68_TOOLCHAIN="${RETRO68_TOOLCHAIN:-}"
# IP address of the B&W G3 running LaunchAPPLServer (TCP mode).
JB_MAC_IP="${JB_MAC_IP:-}"
