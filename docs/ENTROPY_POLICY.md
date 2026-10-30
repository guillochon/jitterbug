# Entropy policy

Hashing cannot create entropy, and statistical tests on DRBG output cannot show
that the input was unpredictable. Credit is therefore decided per source, from
measurements of raw samples, and kept conservative.

## Current credits are placeholders

`core/jb_policy.c` assigns each source a credit (bits per sample) and an
assumed min-entropy H used to size the health-test cutoffs. None of these have
been measured. Until they are, treat any output as experimental.

## How a credit gets set

1. Capture a raw trace on the B&W in **native OS 9** (not Classic, not an
   emulator; timing there is mediated by a host OS) with at least 1,000,000
   symbols for the source.
2. Run `scripts/assess.sh trace.jbr`. Use the `ea_non_iid` min-entropy
   estimate (H_est, bits per 8-bit symbol).
3. Set `health_h_q8` to about H_est, and `credit_q8` to at most H_est / 2.
   For sources an attacker on the LAN can observe or influence (network
   arrivals), use a much smaller fraction.
4. Repeat for an idle machine and a cold boot; use the worst case.
5. Commit the trace summary and tool output under `docs/measurements/`.

## Always-true rules

- The seed file is mixed with zero credit.
- Simulated samples are never credited, whatever they look like.
- A source that fails its health test stops earning credit until reset.
- Without enough credit the API fails closed (`JB_ERR_NOT_READY`).
