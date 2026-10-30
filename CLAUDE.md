# Jitterbug — system-wide entropy source for Mac OS 9

Goal: an OS 9 extension that gives every app a `/dev/urandom`-equivalent,
collecting timing noise system-wide on a Blue & White Power Mac G3 and
serving it through a CSPRNG. Classic Mac OS has no OS-provided cryptographic
RNG; today each TLS library (Certainly, macTLS) gathers its own entropy or
makes the app supply a seed. See docs/PRIOR_ART.md before proposing designs.

## Status (keep this section current)
- M0 DONE: portable core (SHA-256, HMAC-DRBG, SP 800-90B health tests, pool,
  ISR-safe ring, engine) + host simulator + tests. All tests pass on x86-64,
  32-bit x86, and big-endian PPC 750 under qemu-user.
- M1 BUILDS, NEVER RUN: `mac/collector/collector.c` compiles warning-free for
  PPC and 68K with the Retro68 Docker image (GCC 16.1, Multiversal). Multiversal
  lacks `Microseconds`/`UnsignedWide`; the collector declares its own (`JBWide`,
  InterfaceLib on PPC, trap 0xA193 on 68K). Next: run it on the B&W.
- M2+ not started. See docs/ARCHITECTURE.md.

## Commands
- Host build + tests: `scripts/test-host.sh`
- Same tests big-endian on an emulated G3 (PPC 750): `scripts/test-ppc-qemu.sh` (Linux)
- Simulator: `build-host/jb_sim list | run -p bw-g3-active | dump | replay`
- Mac build (Docker image by default, or set RETRO68_TOOLCHAIN): `scripts/build-mac.sh ppc`
- Deploy to the B&W: `JB_MAC_IP=x.x.x.x scripts/deploy.sh`
- Entropy assessment of a hardware trace: `scripts/assess.sh trace.jbr`
- Regenerate icons after editing the ASCII art: `python3 mac/resources/icon/make_icons.py`

## Layout
- `include/`, `core/` — portable engine. Compiled for host AND Mac.
- `sim/`, `tools/`, `tests/` — host only. `sim/` must never be linked into a Mac target.
- `mac/` — Retro68 targets (only configured when a Retro68 toolchain is active).
  `mac/resources/` holds the generated icon family; the app's creator code is
  `JtBg` (`mac/CMakeLists.txt` and `mac/collector/collector.r` must agree).
  Retro68 never sets the Finder bundle bit; `scripts/mac-set-bundle-bit.py`
  does it after each build.
- `docs/` — architecture, entropy policy, simulation, prior art.

## Hard rules
1. `core/` is strict C89 (`-std=c89 -pedantic -Werror` on host): no `//`
   comments, declarations at block top, no `long long`, no VLAs. It must stay
   buildable by Retro68 GCC and ideally CodeWarrior/MrC.
2. Anything callable at interrupt time (`jb_ring_push` and future INIT hooks)
   must not allocate, move memory, call the Memory Manager, or call most of the
   Toolbox. Only write into preallocated memory.
3. Simulated samples are never entropy. Do not let sim output, sim-derived
   numbers, or output-distribution tests justify a credit value.
4. Credits in `core/jb_policy.c` are PLACEHOLDERS until measured on native
   OS 9 hardware (not Classic, not an emulator) per docs/ENTROPY_POLICY.md.
   Lowering a credit is always fine; raising one needs measurement data
   committed under `docs/measurements/`.
5. Seed-file contents are mixed but never credited.
6. Source IDs (`jb_sample.h`) and the `.jbr` format are stable; real hardware
   traces depend on them. Version the format if it must change.
7. Fail closed: when not seeded, `jb_engine_random` returns JB_ERR_NOT_READY.
   Never fall back to weak output.

## Platform facts worth remembering
- B&W G3: 100 MHz bus → PPC time base at 25 MHz. Low 32 bits wrap every ~172 s.
- Retro68's bundled Multiversal Interfaces lack Open Transport, MacTCP and
  Carbon. OT work (net jitter source, TCP trace upload) needs Apple's
  Universal Interfaces 3.4 (see Retro68 README; Nix `powerpc-universal` shell
  or Docker `INTERFACES=universal` can provide them).
- Retro68 supports `add_application`, PPC shared libraries (MakePEF + cfrg),
  and `add_ndrv` for PPC native drivers — an ndrv is a candidate design for a
  `.Jitterbug` Device Manager driver (see ARCHITECTURE.md).
- 68K and PPC compilers may lay out structs differently. Anything shared
  between a 68K INIT and PPC code must use an explicitly versioned, fixed layout.
- LaunchAPPLServer executes any app it receives over TCP without
  authentication. Trusted LAN only.

## License
MIT (`LICENSE`). If code is ever adapted from prior-art projects (MIT/GPL),
keep their notices; GPL code cannot be copied in.

## Open decisions for the owner
- INIT + shared library vs. ndrv driver for the system-wide component.
