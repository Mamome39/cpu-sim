#!/usr/bin/env bash
# Usage:
#   ./spike_test.sh <file.s>              # assemble + run Spike, print trace
#   ./spike_test.sh <file.s> --save       # save trace to <file>.trace
#   ./spike_test.sh <file.s> --compare    # compare Spike trace vs sim trace
#   ./spike_test.sh --all                 # run all .s files in this directory

set -euo pipefail

# ── config ────────────────────────────────────────────────────────────────────
SPIKE="${SPIKE:-spike}"
AS="${AS:-riscv64-unknown-elf-as}"
LD="${LD:-riscv64-unknown-elf-ld}"
SIM="${SIM:-}"          # path to your simulator binary once built, e.g. ../build/cpu-sim
ISA="rv32i"
MARCH="rv32i"
MABI="ilp32"
MELF="elf32lriscv"
LDSCRIPT="${LDSCRIPT:-$(cd "$(dirname "$0")" && pwd)/link.ld}"
# 1 MiB at 0x80000000 — keeps Spike's initial sp inside mapped memory for rv32i
SPIKE_MEM="0x80000000:0x100000"

# ── colors ────────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
pass()  { echo -e "${GREEN}[PASS]${NC} $1"; }
fail()  { echo -e "${RED}[FAIL]${NC} $1"; }
info()  { echo -e "${YELLOW}[INFO]${NC} $1"; }

# ── helpers ───────────────────────────────────────────────────────────────────
check_deps() {
    for cmd in "$SPIKE" "$AS" "$LD"; do
        if ! command -v "$cmd" &>/dev/null; then
            echo "Missing tool: $cmd" >&2; exit 1
        fi
    done
}

assemble() {
    local src="$1" elf="$2"
    "$AS" -march="$MARCH" -mabi="$MABI" -o "${elf%.elf}.o" "$src"
    "$LD" -m "$MELF" -T "$LDSCRIPT" -o "$elf" "${elf%.elf}.o"
}

spike_trace() {
    local elf="$1"
    # uniq collapses the spin loop at program end (consecutive identical lines)
    "$SPIKE" --isa="$ISA" -m"$SPIKE_MEM" --log-commits "$elf" 2>&1 \
        | grep -E "^core" \
        | uniq
}

normalize_trace() {
    # strip timing/cycle info Spike may emit, keep: PC  hex  reg  value lines
    grep -E "^core\s+[0-9]+: [0-9]" | \
    sed 's/^core\s\+[0-9]\+: [0-9]\+ //'
}

run_one() {
    local src="$1" mode="${2:-print}"
    local base; base="$(basename "$src" .s)"
    local dir;  dir="$(dirname "$src")"
    local elf="${dir}/${base}.elf"
    local trace_file="${dir}/${base}.trace"

    info "Testing: $src"

    assemble "$src" "$elf"

    case "$mode" in
        print)
            info "Spike trace:"
            spike_trace "$elf"
            ;;
        save)
            spike_trace "$elf" > "$trace_file"
            pass "Saved golden trace → $trace_file"
            ;;
        compare)
            if [[ -z "$SIM" ]]; then
                fail "SIM not set. Export SIM=/path/to/cpu-sim and retry."
                return 1
            fi
            if [[ ! -x "$SIM" ]]; then
                fail "Simulator binary not found or not executable: $SIM"
                return 1
            fi

            local spike_out sim_out
            spike_out="$(spike_trace "$elf")"
            sim_out="$("$SIM" "$elf" 2>&1)"

            if diff <(echo "$spike_out") <(echo "$sim_out") > /dev/null 2>&1; then
                pass "$base"
            else
                fail "$base — trace divergence:"
                diff <(echo "$spike_out") <(echo "$sim_out") || true
            fi
            ;;
    esac

    rm -f "${dir}/${base}.o" "$elf"
}

# ── main ──────────────────────────────────────────────────────────────────────
check_deps

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MODE="print"
FILES=()

for arg in "$@"; do
    case "$arg" in
        --save)    MODE="save"    ;;
        --compare) MODE="compare" ;;
        --all)
            while IFS= read -r -d '' f; do
                FILES+=("$f")
            done < <(find "$SCRIPT_DIR" -name "*.s" -print0)
            ;;
        *)
            [[ -f "$arg" ]] && FILES+=("$arg") || { echo "Not a file: $arg" >&2; exit 1; }
            ;;
    esac
done

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "Usage:"
    echo "  $0 <file.s>              print Spike trace"
    echo "  $0 <file.s> --save       save golden trace to <file>.trace"
    echo "  $0 <file.s> --compare    diff Spike vs your simulator (set SIM=)"
    echo "  $0 --all                 run all .s files in tests/"
    exit 0
fi

PASS=0; FAIL=0
for f in "${FILES[@]}"; do
    if run_one "$f" "$MODE"; then
        ((PASS++)) || true
    else
        ((FAIL++)) || true
    fi
done

if [[ $((PASS + FAIL)) -gt 1 ]]; then
    echo ""
    echo "Results: ${GREEN}${PASS} passed${NC}  ${RED}${FAIL} failed${NC}"
fi
