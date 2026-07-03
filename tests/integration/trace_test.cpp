#include <gtest/gtest.h>
#include <sstream>
#include "cpusim/sim/core.h"
#include "cpusim/sim/tracer.h"

using namespace cpusim;

static constexpr uint32_t BASE = 0x80000000u;
static constexpr size_t   SZ   = 0x10000u;

static constexpr uint32_t NOP    = 0x00000013u;  // addi x0, x0, 0
static constexpr uint32_t EBREAK = 0x00100073u;

// Helper: build a Core, attach a Tracer, run, return the trace string.
static std::string run_trace(Core& c) {
    std::ostringstream ss;
    Tracer t(ss);
    c.set_tracer(&t);
    c.run();
    return ss.str();
}

// ── BasicArith ───────────────────────────────────────────────────────────────
// addi x1,x0,1 / addi x2,x0,2 / addi x3,x0,42 / 3×nop / add x4,x1,x2 /
// 4×nop / ebreak
TEST(Trace, BasicArith) {
    Core c(BASE, SZ);
    c.load_program({
        0x00100093u,             // addi x1, x0, 1
        0x00200113u,             // addi x2, x0, 2
        0x02a00193u,             // addi x3, x0, 42
        NOP, NOP, NOP,
        0x00208233u,             // add  x4, x1, x2
        NOP, NOP, NOP, NOP,
        EBREAK,
    });

    static const char* kExpected =
        "core   0: 3 0x80000000 (0x00100093) ra   0x00000001\n"
        "core   0: 3 0x80000004 (0x00200113) sp   0x00000002\n"
        "core   0: 3 0x80000008 (0x02a00193) gp   0x0000002a\n"
        "core   0: 3 0x8000000c (0x00000013)\n"
        "core   0: 3 0x80000010 (0x00000013)\n"
        "core   0: 3 0x80000014 (0x00000013)\n"
        "core   0: 3 0x80000018 (0x00208233) tp   0x00000003\n"
        "core   0: 3 0x8000001c (0x00000013)\n"
        "core   0: 3 0x80000020 (0x00000013)\n"
        "core   0: 3 0x80000024 (0x00000013)\n"
        "core   0: 3 0x80000028 (0x00000013)\n"
        "core   0: 3 0x8000002c (0x00100073)\n";

    EXPECT_EQ(run_trace(c), kExpected);
}

// ── ForwardExToEx ────────────────────────────────────────────────────────────
// addi x1,x0,1 immediately followed by addi x2,x1,1 (EX→EX forwarding).
TEST(Trace, ForwardExToEx) {
    Core c(BASE, SZ);
    c.load_program({
        0x00100093u,             // addi x1, x0, 1
        0x00108113u,             // addi x2, x1, 1  (uses forwarded x1)
        NOP, NOP, NOP, NOP,
        EBREAK,
    });

    static const char* kExpected =
        "core   0: 3 0x80000000 (0x00100093) ra   0x00000001\n"
        "core   0: 3 0x80000004 (0x00108113) sp   0x00000002\n"
        "core   0: 3 0x80000008 (0x00000013)\n"
        "core   0: 3 0x8000000c (0x00000013)\n"
        "core   0: 3 0x80000010 (0x00000013)\n"
        "core   0: 3 0x80000014 (0x00000013)\n"
        "core   0: 3 0x80000018 (0x00100073)\n";

    EXPECT_EQ(run_trace(c), kExpected);
}

// ── LoadUseStall ─────────────────────────────────────────────────────────────
// lw x1,0(x3) immediately followed by addi x2,x1,5.
// A load-use stall is inserted; the program-order trace is unchanged.
// x3 = BASE (pre-written), mem[BASE] = 10.
TEST(Trace, LoadUseStall) {
    Core c(BASE, SZ);
    c.store_word(BASE, 10u);
    c.write_reg(3, BASE);
    c.load_program({
        0x0001a083u,             // lw   x1, 0(x3)
        0x00508113u,             // addi x2, x1, 5  (stall + MEM→EX fwd)
        NOP, NOP, NOP, NOP, NOP,
        EBREAK,
    });

    static const char* kExpected =
        "core   0: 3 0x80000000 (0x0001a083) ra   0x0000000a\n"
        "core   0: 3 0x80000004 (0x00508113) sp   0x0000000f\n"
        "core   0: 3 0x80000008 (0x00000013)\n"
        "core   0: 3 0x8000000c (0x00000013)\n"
        "core   0: 3 0x80000010 (0x00000013)\n"
        "core   0: 3 0x80000014 (0x00000013)\n"
        "core   0: 3 0x80000018 (0x00000013)\n"
        "core   0: 3 0x8000001c (0x00100073)\n";

    EXPECT_EQ(run_trace(c), kExpected);
}

// ── StoreLoad ────────────────────────────────────────────────────────────────
// addi x3,x0,42 / sw x3,0(x1) / lw x5,0(x1) / ebreak
// Validates mem lines for stores and reg lines for loads.
// x1 = BASE (pre-written).
TEST(Trace, StoreLoad) {
    Core c(BASE, SZ);
    c.write_reg(1, BASE);
    c.load_program({
        0x02a00193u,             // addi x3, x0, 42
        NOP, NOP, NOP,
        0x0030a023u,             // sw   x3, 0(x1)
        NOP, NOP, NOP,
        0x0000a283u,             // lw   x5, 0(x1)
        NOP, NOP, NOP, NOP,
        EBREAK,
    });

    static const char* kExpected =
        "core   0: 3 0x80000000 (0x02a00193) gp   0x0000002a\n"
        "core   0: 3 0x80000004 (0x00000013)\n"
        "core   0: 3 0x80000008 (0x00000013)\n"
        "core   0: 3 0x8000000c (0x00000013)\n"
        "core   0: 3 0x80000010 (0x0030a023) mem 0x80000000 0x0000002a\n"
        "core   0: 3 0x80000014 (0x00000013)\n"
        "core   0: 3 0x80000018 (0x00000013)\n"
        "core   0: 3 0x8000001c (0x00000013)\n"
        "core   0: 3 0x80000020 (0x0000a283) t0   0x0000002a\n"
        "core   0: 3 0x80000024 (0x00000013)\n"
        "core   0: 3 0x80000028 (0x00000013)\n"
        "core   0: 3 0x8000002c (0x00000013)\n"
        "core   0: 3 0x80000030 (0x00000013)\n"
        "core   0: 3 0x80000034 (0x00100073)\n";

    EXPECT_EQ(run_trace(c), kExpected);
}
