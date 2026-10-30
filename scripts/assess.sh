#!/usr/bin/env bash
# Estimate min-entropy per source from a .jbr trace with NIST's SP 800-90B tool.
#   scripts/assess.sh trace.jbr
# Builds ea_non_iid on first use (needs: g++, make, libbz2, libdivsufsort,
# jsoncpp, gmp, mpfr, openssl dev packages - see the NIST repo README).
set -euo pipefail
source "$(dirname "$0")/common.sh"
TRACE="${1:?usage: $0 trace.jbr}"
NIST="$JB_ROOT/third_party/SP800-90B_EntropyAssessment"
OUT="$JB_ROOT/analysis/$(basename "$TRACE" .jbr)"

if [[ ! -x "$NIST/cpp/ea_non_iid" ]]; then
    mkdir -p "$JB_ROOT/third_party"
    [[ -d "$NIST" ]] || git clone --depth 1 https://github.com/usnistgov/SP800-90B_EntropyAssessment.git "$NIST"
    make -C "$NIST/cpp" non_iid
fi
python3 "$JB_ROOT/tools/jbr2nist.py" "$TRACE" "$OUT"
for f in "$OUT"/*.bin; do
    echo "=== $(basename "$f" .bin) ==="
    "$NIST/cpp/ea_non_iid" -v "$f" 8 | tail -5
done
