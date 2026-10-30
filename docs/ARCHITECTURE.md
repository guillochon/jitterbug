# Architecture and roadmap

## Components

| Piece | Where | Runs | Status |
|---|---|---|---|
| Engine: ring, health tests, pool, HMAC-DRBG | `core/` | host + Mac | done, tested on host |
| Simulator + CLI | `sim/`, `tools/` | host only | done |
| Collector app (raw trace capture) | `mac/collector/` | Mac app | builds; not yet run on hardware |
| Collector INIT (interrupt-time hooks) | `mac/init/` | 68K code resource, system heap | not started |
| JitterbugLib (public API) | `mac/lib/` | PPC CFM shared library | not started |

## Data flow

Producers (interrupt time) only stamp the PPC time base and push a sample
(source, time base, value) into a single-producer/single-consumer ring in the
system heap. Traces store each sample as a fixed 10-byte record. Everything else
happens at task time: health tests per source, mixing into the SHA-256 pool,
and (re)seeding the HMAC-DRBG once at least 256 bits are credited.

The simulator pushes into the same ring through the same functions, so host
tests exercise the code that will run on the Mac.

## Milestones

**M0 — Host pipeline (done).** Engine + simulator + tests.

**M1 — Collector on hardware.** Build `JitterbugCollector`, run it on the B&W in
native OS 9, capture traces of at least 1,000,000 symbols per source, replay
them through `jb_sim replay`, and measure them with `scripts/assess.sh`. Note
that the 60 Hz tick-edge source needs about 4.6 hours for a million samples.
Then fit the simulator profiles to the real traces.

**M2 — More sources, still in the app.** Disk completion timing (time
`PBReadAsync` completions), sound-input LSBs via the Sound Input Manager,
Time Manager task jitter (first interrupt-time producer: proves the ring),
and Open Transport packet arrival (needs Universal Interfaces).

**M3 — System-wide collection.** Two candidate designs; decide after M2:

- *INIT + shared library.* A small 68K INIT allocates the ring in the system
  heap, installs `jGNEFilter` and a Time Manager task, publishes the ring via a
  Gestalt selector (`'JBug'`), and registers a `ShutDwnInstall` proc to write
  the seed file. Keep the INIT to collection only; struct layouts shared with
  PPC code must be fixed and versioned.
- *PPC native driver (`ndrv`).* Retro68's `add_ndrv` builds a Device Manager
  driver. A `.Jitterbug` driver whose `Read` returns random bytes is the closest
  OS 9 analogue to `/dev/random`, and is callable from 68K and PPC code alike.
  Interrupt-time collection hooks would still need an INIT or patches.

**M4 — Public API.** `JitterbugLib` exports `JBRandomBytes`, `JBStir`
(apps can contribute their own timing), and `JBStatus`. Adapters for
Certainly's `MacTLS_SetEntropyProvider` and macTLS's seed injection.

**M5 — Validation and release.** Native-hardware measurements committed under
`docs/measurements/`, external review of the credit policy, installer.

## Seed file

At shutdown and periodically (OS 9 crashes), write 32 bytes from the DRBG to
`Preferences:Jitterbug Seed`. At boot, mix it in with zero credit. Never reuse a
seed: rewrite it immediately after reading. A restored disk image would
otherwise replay the same seed, which is why it is never credited.
