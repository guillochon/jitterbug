# Boot Mac OS 9.2.2 in a native Windows QEMU window to try Jitterbug by hand.
# Native QEMU locks the pointer while grabbed; under WSLg it cannot, so the
# guest cursor resets at the window edge. Needs QEMU for Windows
# (winget install SoftwareFreedomConservancy.QEMU) and, to build play.img,
# a WSL distro with hfsutils (see play-qemu-os9.sh).
#
#   scripts\play-qemu-os9.ps1           # reuse play.img, keeping your changes
#   scripts\play-qemu-os9.ps1 -Fresh    # rebuild play.img from the base disk
#
# Click in the window to grab the mouse; Ctrl+Alt+G releases it.
# Emulator timing is never entropy evidence (CLAUDE.md rule 3).
param([switch]$Fresh, [string]$Distro = "Ubuntu-22.04")
$ErrorActionPreference = "Stop"

$qdir = if ($env:JB_QEMU_DIR) { $env:JB_QEMU_DIR } else { Join-Path $HOME ".cache\jitterbug-qemu" }
$play = Join-Path $qdir "play.img"
$qemu = (Get-Command qemu-system-ppc -ErrorAction SilentlyContinue).Source
if (-not $qemu) { $qemu = "C:\Program Files\qemu\qemu-system-ppc.exe" }
if (-not (Test-Path $qemu)) { throw "QEMU not found; run: winget install SoftwareFreedomConservancy.QEMU" }

if ($Fresh -or -not (Test-Path $play)) {
    $root = wsl -d $Distro wslpath -a ((Resolve-Path "$PSScriptRoot\..").Path -replace '\\', '/')
    $wqdir = wsl -d $Distro wslpath -a ($qdir -replace '\\', '/')
    wsl -d $Distro --cd $root -- env JB_QEMU_DIR=$wqdir bash scripts/play-qemu-os9.sh --fresh --no-launch
    if ($LASTEXITCODE -ne 0) { throw "building play.img failed" }
}

# SDL, not the default GTK window: SDL's relative-mouse mode keeps the host
# pointer captured, so the guest cursor reaches the menu bar and screen edges.
& $qemu -M mac99,via=pmu -m 512 -g 1024x768x32 -display sdl `
    -drive "file=$play,format=raw,media=disk" -boot c -name Jitterbug
