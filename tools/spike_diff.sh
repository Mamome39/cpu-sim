#!/usr/bin/env bash
# spike_diff.sh — diff our simulator's commit trace against Spike's.
#
# Usage:
#   tools/spike_diff.sh <elf> [build_dir]
#
# Requires:
#   spike, riscv64-unknown-elf-nm, bench_run (in build_dir)
#
# Compares only instructions inside main() so startup/exit glue
# (la sp / la t0 tohost / sw / ebreak) is excluded from the diff.
#
# Exit codes:
#   0 — traces match
#   1 — traces differ (diff printed to stdout)
#   2 — setup error (missing tool, bad ELF, etc.)

set -euo pipefail

ELF="${1:?usage: spike_diff.sh <elf> [build_dir]}"
BUILD="${2:-./build}"
BENCH_RUN="$BUILD/bench_run"

# ── Sanity checks ────────────────────────────────────────────────────
for cmd in spike riscv64-unknown-elf-nm; do
    command -v "$cmd" &>/dev/null || {
        echo "error: '$cmd' not in PATH" >&2; exit 2; }
done
[ -x "$BENCH_RUN" ] || {
    echo "error: bench_run not found at '$BENCH_RUN'" >&2; exit 2; }

# ── Find the PC range of main() ──────────────────────────────────────
MAIN_PC=$(riscv64-unknown-elf-nm "$ELF" \
    | awk '$2 == "T" && $3 == "main" {print "0x"$1}' | head -1)
if [ -z "$MAIN_PC" ]; then
    echo "error: cannot locate 'main' symbol in $ELF" >&2
    exit 2
fi
echo "main starts at $MAIN_PC"

# Portable hex->decimal conversion for awk (macOS nawk has no strtonum).
AWK_HEX2DEC='function h2d(s,  r,i,c){
    r=0; s=tolower(substr(s,3))
    for(i=1;i<=length(s);i++){
        c=index("0123456789abcdef",substr(s,i,1))-1
        r=r*16+c
    }
    return r
}'
AWK_FILTER="$AWK_HEX2DEC"'
    /^core/ && h2d($4) >= h2d(pc) {print}'

# ── Temp files ───────────────────────────────────────────────────────
OUR_FULL=$(mktemp /tmp/cpusim_ours_full_XXXXXX.trace)
OUR=$(mktemp /tmp/cpusim_ours_main_XXXXXX.trace)
SPIKE_FULL=$(mktemp /tmp/cpusim_spike_full_XXXXXX.trace)
SPIKE=$(mktemp /tmp/cpusim_spike_main_XXXXXX.trace)
trap 'rm -f "$OUR_FULL" "$OUR" "$SPIKE_FULL" "$SPIKE"' EXIT

# ── Our trace ────────────────────────────────────────────────────────
"$BENCH_RUN" --trace="$OUR_FULL" "$ELF" > /dev/null
awk -v pc="$MAIN_PC" "$AWK_FILTER" "$OUR_FULL" > "$OUR"

# ── Spike trace ──────────────────────────────────────────────────────
# Spike writes commit log to stderr.
spike --isa=rv32i -m0x80000000:0x10000 --log-commits "$ELF" \
    2>"$SPIKE_FULL" || true

awk -v pc="$MAIN_PC" "$AWK_FILTER" "$SPIKE_FULL" > "$SPIKE"

# ── Compare ──────────────────────────────────────────────────────────
OUR_LINES=$(wc -l < "$OUR")
SPIKE_LINES=$(wc -l < "$SPIKE")
echo "our trace:   $OUR_LINES lines"
echo "spike trace: $SPIKE_LINES lines"
echo ""

if diff -u "$SPIKE" "$OUR"; then
    echo "PASS — traces match"
else
    echo ""
    echo "FAIL — traces differ (--- spike  +++ ours)"
    exit 1
fi
