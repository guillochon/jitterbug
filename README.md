<p align="center"><img src="mac/resources/icon/preview/icl8.png" width="128" height="128" alt="Jitterbug icon: a puzzle-piece extension icon with a pixel-art figeater beetle"></p>

# Jitterbug

A system-wide cryptographic random source for Mac OS 9, targeting the Blue & White
Power Mac G3. Think `/dev/urandom` for classic Mac OS: an extension collects timing
noise from the whole system, and any app can ask it for random bytes.

**Status:** early. The portable engine and a host-side simulator work and are tested.
The first hardware tool (a sample collector app) builds with Retro68 but has not
yet been run on a real Mac.

## Quick start

```sh
# 1. Host side: build the engine, simulator and tests
scripts/test-host.sh

# 2. Play with the simulated B&W noise sources
build-host/jb_sim list
build-host/jb_sim run -p cold-boot -t 60
build-host/jb_sim run -p degenerate -t 60     # must never seed
build-host/jb_sim run -p bw-g3-active -t 60 -n 10000000 -o out.bin   # for dieharder/PractRand

# 3. Mac side: get Retro68 (Docker image by default) and cross-compile
scripts/setup-retro68.sh
scripts/build-mac.sh ppc

# 4. Run on the B&W (LaunchAPPLServer running there, TCP mode, trusted LAN only)
JB_MAC_IP=192.168.1.42 scripts/deploy.sh

# 5. Bring "Jitterbug Trace" back to the dev machine, then:
build-host/jb_sim replay -i "Jitterbug Trace"
scripts/assess.sh "Jitterbug Trace"
```

## How it fits together

```
 interrupt time                task time                         callers
 ┌──────────────┐  push  ┌──────────┐  ┌─────────┐  ┌──────┐  ┌──────────┐
 │ jGNEFilter,  │ ─────▶ │ SPSC ring│─▶│ SP800-90B│─▶│SHA-256│─▶│HMAC-DRBG │─▶ JBRandomBytes()
 │ Time Mgr, OT │        └──────────┘  │ health   │  │ pool  │  └──────────┘
 └──────────────┘                      └─────────┘  └──────┘
   (real Mac)          ▲ jb_sim feeds the same ring with modelled or replayed samples
```

The Finder icon is drawn as ASCII art in `mac/resources/icon/make_icons.py`, which
generates `mac/resources/JitterbugIcons.r` and the previews.

See `docs/ARCHITECTURE.md`, `docs/ENTROPY_POLICY.md`, `docs/SIMULATION.md`,
and `docs/PRIOR_ART.md`.

## License

MIT. See `LICENSE`.
