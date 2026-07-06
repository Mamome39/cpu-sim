#!/usr/bin/env bash
# spike_diff.sh — diff our simulator's commit trace against Spike's.
#
# Usage:
#   tools/spike_diff.sh <elf> [build_dir]
#
# Requires:
#   spike, bench_run (in build_dir)
#
# Both traces are filtered to the program's own memory (PC >= base)
# to drop Spike's MROM boot preamble, and the trailing EBREAK is
# stripped: cpu-sim commits it, but Spike exits on the tohost store
# one instruction earlier. What remains is identical program flow.
#
# Exit codes:
#   0 — traces match
#   1 — traces differ (diff printed to stdout)
#   2 — setup error (missing tool, bad ELF, etc.)

set -euo pipefail

ELF="${1:?usage: spike_diff.sh <elf> [build_dir]}"
BUILD="${2:-./build}"
BENCH_RUN="$BUILD/bench_run"

# Program load base — matches Core's base_ and bench_run's -m region.
BASE="0x80000000"
EBREAK="(0x00100073)"   # exit instruction, present only in our trace

# ── Sanity checks ────────────────────────────────────────────────────
command -v spike &>/dev/null || {
    echo "error: 'spike' not in PATH" >&2; exit 2; }
[ -x "$BENCH_RUN" ] || {
    echo "error: bench_run not found at '$BENCH_RUN'" >&2; exit 2; }

# Portable hex->decimal conversion for awk (macOS nawk has no strtonum).
AWK_HEX2DEC='function h2d(s,  r,i,c){
    r=0; s=tolower(substr(s,3))
    for(i=1;i<=length(s);i++){
        c=index("0123456789abcdef",substr(s,i,1))-1
        r=r*16+c
    }
    return r
}'
# Keep program-region commits (PC >= base); drop the EBREAK line.
AWK_FILTER="$AWK_HEX2DEC"'
    /^core/ && h2d($4) >= h2d(base) && $0 !~ ebreak {print}'

# ── Temp files ───────────────────────────────────────────────────────
OUR_FULL=$(mktemp /tmp/cpusim_ours_full_XXXXXX.trace)
OUR=$(mktemp /tmp/cpusim_ours_prog_XXXXXX.trace)
SPIKE_FULL=$(mktemp /tmp/cpusim_spike_full_XXXXXX.trace)
SPIKE=$(mktemp /tmp/cpusim_spike_prog_XXXXXX.trace)
trap 'rm -f "$OUR_FULL" "$OUR" "$SPIKE_FULL" "$SPIKE"' EXIT

# ── Our trace ────────────────────────────────────────────────────────
"$BENCH_RUN" --trace="$OUR_FULL" "$ELF" > /dev/null
awk -v base="$BASE" -v ebreak="$EBREAK" "$AWK_FILTER" "$OUR_FULL" > "$OUR"

# ── Spike trace ──────────────────────────────────────────────────────
# Spike writes commit log to stderr.
spike --isa=rv32i -m0x80000000:0x10000 --log-commits "$ELF" \
    2>"$SPIKE_FULL" || true

awk -v base="$BASE" -v ebreak="$EBREAK" "$AWK_FILTER" "$SPIKE_FULL" > "$SPIKE"

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
