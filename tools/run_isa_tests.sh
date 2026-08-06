#!/usr/bin/env bash
# run_isa_tests.sh — build and run the vendored rv32ui ISA test suite.
#
# Usage:
#   tools/run_isa_tests.sh [build_dir] [test_name ...]
#
# With no test names, runs the whole suite. Otherwise runs just the
# named tests (e.g. `tools/run_isa_tests.sh build add sub sll`).
#
# Each test is assembled against tests/isa/env/ (our CSR-free
# replacement for riscv-test-env), then checked two ways:
#   1. Spike runs it standalone — a nonzero exit means the test's own
#      assertions failed, i.e. the test or toolchain is broken.
#   2. spike_diff.sh compares our commit trace against Spike's,
#      line for line — this is what actually tests the simulator.
#
# Requires:
#   spike, riscv64-unknown-elf-gcc, bench_run (in build_dir)
#
# Exit codes:
#   0 — all tests pass
#   1 — one or more tests failed
#   2 — setup error (missing tool, etc.)

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ISA_DIR="$REPO_ROOT/tests/isa"

# First non-flag arg is the build dir only if it isn't a test name.
BUILD="${1:-$REPO_ROOT/build}"
if [ $# -gt 0 ] && [ -f "$ISA_DIR/rv32ui/$1.S" ]; then
    BUILD="$REPO_ROOT/build"
else
    shift || true
fi

OUT="$BUILD/isa"
BENCH_RUN="$BUILD/bench_run"

# Per-test wall-clock cap. A simulator bug can easily turn a test into
# an infinite loop; without this the suite would hang instead of fail.
TIMEOUT_SECS=30

# ── Tests we deliberately do not run ─────────────────────────────────
# Keep in sync with the "Excluded tests" section of tests/isa/README.md.
#
#   fence_i — needs FENCE.I from Zifencei; cpu-sim decodes plain FENCE
#             only, and does not model an instruction cache that self-
#             modifying code could go stale in. Does not assemble.
#   ma_data — misaligned load/store. Spike raises a misaligned-access
#             trap here rather than emulating it, so the test needs the
#             mtvec handler upstream's environment installs. Ours is
#             CSR-free, so the trap has nowhere to go and Spike spins
#             at the first misaligned load — no usable reference trace,
#             independent of what cpu-sim does.
EXCLUDE="fence_i ma_data"

# ── Sanity checks ────────────────────────────────────────────────────
command -v spike &>/dev/null || {
    echo "error: 'spike' not in PATH" >&2; exit 2; }
command -v riscv64-unknown-elf-gcc &>/dev/null || {
    echo "error: 'riscv64-unknown-elf-gcc' not in PATH" >&2; exit 2; }
[ -x "$BENCH_RUN" ] || {
    echo "error: bench_run not found at '$BENCH_RUN'" >&2; exit 2; }

mkdir -p "$OUT"

# Run a command with a hard wall-clock limit. macOS has no coreutils
# `timeout`, so this is the portable stand-in.
run_limited() {
    local secs="$1"; shift
    "$@" &
    local pid=$!
    ( sleep "$secs"; kill -9 "$pid" 2>/dev/null ) &
    local watchdog=$!
    wait "$pid" 2>/dev/null
    local rc=$?
    kill -9 "$watchdog" 2>/dev/null
    wait "$watchdog" 2>/dev/null
    return $rc
}

# ── Test list ────────────────────────────────────────────────────────
if [ $# -gt 0 ]; then
    TESTS=("$@")
else
    TESTS=()
    for f in "$ISA_DIR"/rv32ui/*.S; do
        name="$(basename "$f" .S)"
        skip=0
        for e in $EXCLUDE; do
            [ "$name" = "$e" ] && skip=1
        done
        [ $skip -eq 0 ] && TESTS+=("$name")
    done
fi

# ── Run ──────────────────────────────────────────────────────────────
PASSED=0
FAILED=0
FAILED_NAMES=""

for name in "${TESTS[@]}"; do
    src="$ISA_DIR/rv32ui/$name.S"
    elf="$OUT/$name.elf"

    if [ ! -f "$src" ]; then
        printf '%-10s  \033[31mNO-SUCH-TEST\033[0m\n' "$name"
        FAILED=$((FAILED + 1)); FAILED_NAMES="$FAILED_NAMES $name"
        continue
    fi

    # 1. Assemble.
    if ! riscv64-unknown-elf-gcc \
            -march=rv32i -mabi=ilp32 \
            -nostdlib -nostartfiles \
            -I"$ISA_DIR/env" -I"$ISA_DIR/macros" \
            -T "$ISA_DIR/env/link.ld" \
            "$src" -o "$elf" 2>"$OUT/$name.build.log"; then
        printf '%-10s  \033[31mBUILD-FAIL\033[0m  (see %s)\n' \
            "$name" "$OUT/$name.build.log"
        FAILED=$((FAILED + 1)); FAILED_NAMES="$FAILED_NAMES $name"
        continue
    fi

    # 2. Spike standalone — does the test pass on the reference model?
    run_limited "$TIMEOUT_SECS" \
        spike --isa=rv32i -m0x80000000:0x10000 "$elf" \
        >/dev/null 2>&1
    spike_rc=$?
    if [ $spike_rc -ne 0 ]; then
        printf '%-10s  \033[31mSPIKE-FAIL\033[0m  (exit %d — testnum %d)\n' \
            "$name" "$spike_rc" "$((spike_rc >> 1))"
        FAILED=$((FAILED + 1)); FAILED_NAMES="$FAILED_NAMES $name"
        continue
    fi

    # 3. Commit-trace diff — this is the part that tests cpu-sim.
    if run_limited "$TIMEOUT_SECS" \
            "$REPO_ROOT/tools/spike_diff.sh" "$elf" "$BUILD" \
            >"$OUT/$name.diff.log" 2>&1; then
        printf '%-10s  \033[32mPASS\033[0m\n' "$name"
        PASSED=$((PASSED + 1))
    else
        printf '%-10s  \033[31mTRACE-DIFF\033[0m  (see %s)\n' \
            "$name" "$OUT/$name.diff.log"
        FAILED=$((FAILED + 1)); FAILED_NAMES="$FAILED_NAMES $name"
    fi
done

# ── Summary ──────────────────────────────────────────────────────────
echo ""
echo "rv32ui: $PASSED passed, $FAILED failed (excluded:$(
    for e in $EXCLUDE; do printf ' %s' "$e"; done))"
if [ $FAILED -ne 0 ]; then
    echo "failed:$FAILED_NAMES"
    exit 1
fi
exit 0
