# Simulating the noise sources off-target

`jb_sim` lets you develop and test on any machine. It models each B&W source
statistically and pushes samples through the real engine code.

## Profiles

| Profile | What it models | Expected result |
|---|---|---|
| `bw-g3-idle` | idle machine on a LAN | seeds within a few seconds |
| `bw-g3-active` | someone using the machine | seeds fastest |
| `cold-boot` | first seconds after power-on, no input or network | seeds, slower |
| `emulator` | SheepShaver/QEMU-like 1 µs host-clock quantisation | health tests fail, never seeds |
| `degenerate` | stuck timer, silent audio | must never seed |

The `emulator` result matters: entropy measured in an emulator says nothing
about the real B&W, and the engine correctly refuses to trust it.

## Three ways to feed the engine

- `jb_sim run -p PROFILE` — modelled samples, generated live.
- `jb_sim dump -p PROFILE -o x.jbr` then `jb_sim replay -i x.jbr` — modelled
  samples via a file (tests the trace path).
- `jb_sim replay -i "Jitterbug Trace"` — **real samples recorded on the B&W**.
  This is the most useful mode once M1 works: debug the engine on your fast
  machine against genuine hardware behaviour.

## Caveats

The model parameters in `sim/jb_sim.c` are guesses. Replace them with values
fitted to real traces (a `tools/fit_profile.py` is a good next task). The
simulator uses a seeded non-cryptographic PRNG on purpose: runs are
reproducible, and its output must never be mistaken for entropy.
