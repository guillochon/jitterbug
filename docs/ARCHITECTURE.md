# Architecture and roadmap

## Components

| Piece | Where | Runs | Status |
|---|---|---|---|
| Engine: ring, health tests, pool, HMAC-DRBG | `core/` | host + Mac | done, tested on host |
| Simulator + CLI | `sim/`, `tools/` | host only | done |
| Collector app (raw trace capture) | `mac/collector/` | Mac app | builds; not yet run on hardware |
| Jitterbug extension (INIT: collection, engine, API) | `mac/init/` | 68K code resource, system heap | builds; not yet booted |
| SDK: `Jitterbug.h` + `JitterbugGlue.c` (public API) | `mac/sdk/` | compiled into apps, 68K and PPC | builds |
| JitterbugTest (exercises the SDK) | `mac/jbtest/` | Mac app | builds |
| QEMU boot test | `scripts/test-qemu-os9.sh` | host, Docker | written |

## Data flow

Producers (interrupt time) only stamp the PPC time base and push a sample
(source, time base, value) into a single-producer/single-consumer ring in the
system heap. Traces store each sample as a fixed 10-byte record. Everything else
happens at task time: health tests per source, mixing into the SHA-256 pool,
and (re)seeding the HMAC-DRBG once at least 256 bits are credited.

The simulator pushes into the same ring through the same functions, so host
tests exercise the code that will run on the Mac.

### The extension (M3, as built)

Retro68 builds INITs only as 68K code resources, so the whole extension is one
68K INIT; PowerPC Macs run it under the 68K emulator. At boot it allocates the
engine in the system heap, mixes the seed file, and installs:

| Hook | Runs at | Does |
|---|---|---|
| Time Manager task, every 10 ms | interrupt time | `Microseconds()` into the ring (the ring's only producer) |
| `jGNEFilter` | task time, in whichever app calls `GetNextEvent`/`WaitNextEvent` | event timing straight into the engine (`jb_engine_add_sample`), then pumps the ring |
| Gestalt `'JBug'` | any caller | returns the versioned `JBDispatchTable` (`mac/sdk/Jitterbug.h`) |
| `ShutDwnInstall` (before unmount) | shutdown | writes a fresh seed file |

Apps compile in `mac/sdk/JitterbugGlue.c`: it calls the table directly on 68K
and through Mixed Mode (`CallUniversalProc`, C stack convention) on PPC. Every
shared struct is all 32-bit fields, so both compilers agree on the layout. A
`bset`-based busy flag stops re-entrant use (say, an API call from a deferred
task while the filter is pumping); such calls get `kJBErrBusy`.

**Measurement caveat.** The 68K INIT cannot execute `mftb`, so its samples are
`Microseconds()` values, not time-base ticks as in JitterbugCollector. Their
symbols are different streams and need their own measurements before any
credit is trusted (CLAUDE.md rule 4). Until then `JBStatus` always reports
`kJBStatusPlaceholderCredits`. A PPC-native timing path (an accelerated code
resource that reads the time base) is a possible later improvement.

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

**M3 — System-wide collection.** Decided 2026-10-30: one 68K INIT with a
Gestalt API (see "The extension" above). Written and building; next is a boot
test in QEMU (`scripts/test-qemu-os9.sh`), then the B&W. The candidates were:

- *INIT + shared library.* A small 68K INIT allocates the ring in the system
  heap, installs `jGNEFilter` and a Time Manager task, publishes the ring via a
  Gestalt selector (`'JBug'`), and registers a `ShutDwnInstall` proc to write
  the seed file. Keep the INIT to collection only; struct layouts shared with
  PPC code must be fixed and versioned.
- *PPC native driver (`ndrv`).* Retro68's `add_ndrv` builds a Device Manager
  driver. A `.Jitterbug` driver whose `Read` returns random bytes is the closest
  OS 9 analogue to `/dev/random`, and is callable from 68K and PPC code alike.
  Interrupt-time collection hooks would still need an INIT or patches.

**M4 — Public API.** `JBRandomBytes`, `JBStir` (apps contribute their own
timing, never credited) and `JBStatus` now exist through `mac/sdk` rather than
a CFM library. Still to do: adapters for Certainly's
`MacTLS_SetEntropyProvider` and macTLS's seed injection, and possibly a
`.Jitterbug` driver for callers that prefer `PBRead`.

**M5 — Validation and release.** Native-hardware measurements committed under
`docs/measurements/`, external review of the credit policy, installer.

## Seed file

`Preferences:Jitterbug Seed` (32 bytes, type `JBsd`, creator `JtBg`). At boot
the INIT mixes it in with zero credit and immediately overwrites it with
SHA-256 of the old contents and the boot time, so the same bytes are never
mixed twice even if the Mac crashes before shutdown. Once the DRBG first seeds
(once per boot) and again at shutdown, it writes 32 fresh DRBG bytes. A restored
disk image would replay an old seed, which is why it is never credited.
