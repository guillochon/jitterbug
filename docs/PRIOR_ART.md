# Prior art (checked 2026-10-30)

No system-wide entropy extension for classic Mac OS was found. Existing work
gathers entropy inside individual libraries or apps:

- **Certainly** (github.com/lumberbarons/certainly; minorbug/Certainly is an
  older copy) — BearSSL over Open Transport, built with Retro68. Treats OS 9
  as having no CSPRNG and requires the app to supply a trusted seed
  (`MacTLS_SeedTrusted`), or register a provider callback via
  `MacTLS_SetEntropyProvider`. That callback is a natural place for Jitterbug
  to plug in. The minorbug copy only has the older `MacTLS_AddEntropy`.
- **macTLS / macEntropy** (github.com/mplsllc/macTLS, `MACENTROPY_SCOPE.md`) —
  SHA-256 accumulator, seed file in Preferences, OT packet-arrival jitter,
  validated on a real G3 in May 2026. Lives inside macTLS; a background
  daemon/collector is explicitly out of scope there. CodeWarrior 8, C89.
- **jackwillis/weather** (github.com/jackwillis/weather) — a Retro68 app
  actively working (late Oct 2026) on OS 9 entropy for its own TLS: first-run
  "move the mouse" collection, measuring time-base/event timing on hardware,
  crediting zero bits until measured. Their issue #197 rightly notes that
  output-distribution tests are not min-entropy estimates. Worth coordinating
  with; measurements could be shared.
- **MacSSL** (github.com/bbenchoff/MacSSL) — mbedTLS port for CodeWarrior.
- **OpenSSL's old MacOS `Randomizer.cpp`** — historical classic-Mac entropy
  gathering, useful as a reference for sources.
- **Crypto Ancienne** — TLS for old systems, including an MPW tool.

Before copying any code from these, check the file's license and keep notices.
