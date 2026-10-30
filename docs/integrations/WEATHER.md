# Integrating Jitterbug with Weather ("What It Be Like?")

Status: proposal, 2026-10-30. Nothing here is implemented in Weather. On the
Jitterbug side the extension, SDK and a demo app work in QEMU (Mac OS 9.2.2,
`mac99`) but have not yet run on real hardware.
Written for the maintainers of [jackwillis/weather](https://github.com/jackwillis/weather)
after reading their seed design at `main` (`docs/OS9-SEEDS.md`,
`docs/OS9-ENTROPY-MEASUREMENT.md`, issues #120, #121, #193, #197).

## What each project has today

**Weather** is a C++17 Retro68 app with BearSSL TLS 1.2. On OS 9 it refuses TLS
until it has a managed 32-byte seed (`Weather TLS Seed` in Preferences, with an
exclusive lock and checked Seed/Next rotation). Today that seed is provisioned
from a Linux CSPRNG. Issue #120 plans a first-run "A Little Randomness" harvest
targeting 128 credited bits. It also plans a typed 32-hex-digit path. All
timing sources are credited 0 bits until #121/#193 produce reviewed native
measurements.

**Jitterbug** aims to be the OS-level piece that OS 9 lacks: an extension that
collects timing noise system-wide, from boot onwards, and serves random bytes
to any app. Its current state:

| Piece | State |
|---|---|
| Portable engine (C89): SHA-256 pool, HMAC-DRBG (SP 800-90A), SP 800-90B repetition-count and adaptive-proportion health tests, interrupt-safe ring | done; tested on x86 and big-endian PPC 750 under qemu |
| Host simulator, with a modelled `emulator` profile that never seeds | done (but see the QEMU row) |
| JitterbugCollector (raw `.jbr` trace capture app) | builds with Retro68; not yet run on hardware |
| System-wide extension (68K INIT) and public API (`mac/sdk`) | boots in QEMU: installs, seeds, serves random bytes to a PowerPC app through Mixed Mode, carries its seed file across boots; not yet run on hardware |
| JitterbugDemo (`mac/demo`): a small app showing the API (coin, die, key, password) | builds; written as code to copy from |
| QEMU boot test (`scripts/test-qemu-os9.sh`) | automated: installs the build, boots headless, reads the logs back |
| Entropy credits per source | placeholders; zero evidence yet. Under QEMU the placeholder credits were enough to seed from timer jitter alone, which is exactly why they must be replaced by native measurements before anyone relies on the output |

Jitterbug's rules match Weather's: credit only from native OS 9 measurements
(never Classic or emulators); fail closed when not seeded; never credit seed
files.

## Why integrate

Weather's #120 harvest has to start from nothing at first launch, in a dialog,
from one app's events. A system-wide collector has been running since boot. It
also sees sources an app cannot easily reach: interrupt-time event arrival,
disk completions, Time Manager jitter, and later Open Transport. Many apps
could share one well-reviewed collector instead of each building its own.

The integration must not cost Weather any of its guarantees. Weather must work
unchanged when Jitterbug is absent. Weather keeps its seed store, lock and
rotation. Weather does not trust Jitterbug's credits until it has reviewed them.

## Proposed integration, in three stages

### Stage 1: zero-credit supplemental input (low risk)

When Jitterbug is installed and ready, Weather reads 32 bytes from it at
startup and mixes them into its HMAC_DRBG as additional input, next to the
managed seed and the supplemental timing it already mixes. The credit is 0.
Provenance, threshold and UI are unchanged.

Mixing extra input into HMAC-DRBG cannot weaken it, even if that input is
known to an attacker. So this stage needs no entropy review, only code review.
It is useful defence in depth: it helps if the managed seed is cloned or
restored from a snapshot, which `OS9-SEEDS.md` notes nothing else recovers from.

### Stage 2: Jitterbug as a first-run seed source (after measurement)

Once Jitterbug has credits backed by committed native measurements (its
`docs/ENTROPY_POLICY.md` procedure), and the Weather owner has reviewed them,
Weather could take its first-run seed from Jitterbug, recorded with its own
provenance (say `system`, next to #120's `provisioned` and `collected`). The
existing rules carry over:

- A provisioned seed still wins. Nothing merges silently.
- If Jitterbug reports not-ready, Weather shows its own harvest dialog as
  planned. Weather never waits on, or falls back to, unseeded output.
- The seed goes through the existing checked rotation. Weather does not read
  Jitterbug again on later launches, except to mix in more bytes as in stage 1.

### Stage 3: shared measurement (can start now)

Both projects need native OS 9 timing data with the same SP 800-90B tooling
(`ea_non_iid`). Weather has a Power Mac G4 and measures events per app.
Jitterbug targets a Blue & White G3 and measures system-wide sources. Sharing
methodology, raw traces and reviewer time helps both:

- Jitterbug can add a converter from Weather's EntropyProbe CSV v2 to `.jbr`,
  so Weather captures replay through Jitterbug's health tests with
  `jb_sim replay`. Jitterbug traces could convert to Weather's format the other
  way.
- G4 captures from Weather would be the first data on another machine for
  Jitterbug's credits. G3 captures from Jitterbug would cover a second model
  for Weather's #121 "other G3/G4 models" gap.
- Preregistered symbol mappings (#193's approach) are worth agreeing on, so the
  two projects' estimates are comparable.

## Interface Weather would code against

The extension is one 68K INIT that publishes a versioned function table
through Gestalt `'JBug'`. Apps compile in two small C89 files,
[`mac/sdk/Jitterbug.h`](../../mac/sdk/Jitterbug.h) and
[`mac/sdk/JitterbugGlue.c`](../../mac/sdk/JitterbugGlue.c) (MIT). They make no
link-time dependency: when the extension is absent every call returns
`kJBErrNotPresent`. On PowerPC the glue reaches the 68K code through
`CallUniversalProc`.

```c
Boolean  JBIsPresent(void);
/* Fill out with len bytes, or fail closed: kJBErrNotReady (buffer zeroed). */
OSStatus JBRandomBytes(void *out, UInt32 len);
/* Fixed, all-32-bit layout: flags, pool bits, reseeds, health failures... */
OSStatus JBStatus(JBStatusRec *s);
/* Contribute the app's own timing. Mixed in, never credited. */
OSStatus JBStir(const void *data, UInt32 len);
```

`JBStatus` sets `kJBStatusPlaceholderCredits` until Jitterbug's credits are
backed by committed native measurements. Weather can key stage 2 on that flag
clearing, and stay at stage 1 otherwise.

On the Weather side this would be one small adapter in `src/platform/os9`,
behind the existing seam, returning unavailable, not-ready or 32 bytes. The
glue is C, so it compiles as-is under Retro68 next to Weather's C++17. A
`.Jitterbug` Device Manager driver (`PBRead` for bytes) remains possible later
if callers want an API with no glue at all. Feedback on that is welcome.

## Trust and failure notes

- On OS 9 every extension runs with full privileges, so a hostile Jitterbug
  could already read Weather's seed file. Integration adds no new trust
  boundary, but stage 2 does make Weather depend on Jitterbug's correctness.
  That is why stage 2 waits on measurements and review.
- A clone or restore of the whole disk duplicates Jitterbug's seed file too.
  Jitterbug never credits its seed file, and seeds only from fresh timing after
  boot. Stage 1 bytes from it therefore differ after a restore, which helps
  Weather's managed seed in exactly that case.
- Weather logs only success/error and booleans for seed state. The adapter
  must follow that rule and never log bytes read from Jitterbug.

## Licensing

Jitterbug is MIT. Weather is GPL-3.0-or-later. Weather may use Jitterbug's API
or code. The reverse is not true: Jitterbug will not copy GPL code. Stage 1 and
stage 2 need no Jitterbug code in Weather's tree, only the adapter Weather
writes. If Weather ever vendors the C89 engine (for example, to share the
health tests), keep Jitterbug's copyright and MIT notice in the files and in
`THIRD_PARTY.md`.

## Open questions for both maintainers

1. Is stage 1 acceptable for Weather v0.1, or only after v0.1?
2. Is the Gestalt + glue interface easy enough to adopt, or would a driver
   (`PBRead` on `.Jitterbug`) suit Weather better?
3. Would Weather share native G4 EntropyProbe captures, and in what form?
4. Should the two projects aim for one shared symbol mapping and
   assessment script, or keep separate ones and compare results?
