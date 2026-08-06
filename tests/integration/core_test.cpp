#include <gtest/gtest.h>
#include <utility>
#include "cpusim/sim/core.h"

using namespace cpusim;

static constexpr uint32_t BASE = 0x80000000u;
static constexpr size_t   SZ   = 0x10000u;

// Default single-cycle-memory config at the standard base/size.
static SimConfig cfg() {
    SimConfig c;
    c.ram_base_addr  = BASE;
    c.ram_size_bytes = SZ;
    return c;
}

// addi x1, x0, 1    0x00100093
// addi x2, x0, 2    0x00200113
// addi x3, x0, 42   0x02A00193
// add  x4, x1, x2   0x002081B3
// sw   x3, 0(x1)    0x0030A023  (base=x1, data=x3, offset=0)
// lw   x5, 0(x1)    0x0000A283  (base=x1, offset=0)
// ebreak             0x00100073

static constexpr uint32_t NOP       = 0x00000013u;
static constexpr uint32_t EBREAK    = 0x00100073u;
static constexpr uint32_t ADDI_X1_1  = 0x00100093u;
static constexpr uint32_t ADDI_X2_2  = 0x00200113u;
static constexpr uint32_t ADDI_X3_42 = 0x02A00193u;
static constexpr uint32_t ADD_X4_X1_X2 = 0x00208233u;
// sw x3, 0(x1): funct3=010, rs1=x1, rs2=x3, imm=0
static constexpr uint32_t SW_X3_0_X1 = 0x0030A023u;
// lw x5, 0(x1): funct3=010, rs1=x1, rd=x5, imm=0
static constexpr uint32_t LW_X5_0_X1 = 0x0000A283u;
// lw x1, 0(x2): funct3=010, rs1=x2, rd=x1, imm=0
static constexpr uint32_t LW_X1_0_X2 = 0x00012083u;

// ── Basic arithmetic ─────────────────────────────────────────────────────────

// EBREAK halts the core; no prior instructions means all regs stay 0.
TEST(Core, EbreakHalts) {
    Core c(cfg());
    c.load_program({EBREAK, NOP, NOP, NOP, NOP});
    c.run();
    EXPECT_TRUE(c.halted());
}

// Single addi reaches WB; EBREAK trails behind to halt cleanly.
TEST(Core, AddiWritesReg) {
    Core c(cfg());
    c.load_program({ADDI_X3_42, NOP, NOP, NOP, EBREAK, NOP, NOP, NOP, NOP});
    c.run();
    EXPECT_EQ(c.read_reg(3), 42u);
}

// R-type ADD using two previously-set registers.
TEST(Core, AddTwoRegs) {
    Core c(cfg());
    c.load_program({ADDI_X1_1, ADDI_X2_2,
                    NOP, NOP, NOP,         // avoid hazard (no forwarding test)
                    ADD_X4_X1_X2,
                    NOP, NOP, NOP, NOP, EBREAK, NOP, NOP, NOP, NOP});
    c.run();
    EXPECT_EQ(c.read_reg(4), 3u);
}

// ── Forwarding ───────────────────────────────────────────────────────────────

// EX->EX: addi x1,1 immediately followed by addi x2,x1,1.
// addi x2, x1, 1 = 0x00108113
TEST(Core, ForwardExToEx) {
    Core c(cfg());
    c.load_program({ADDI_X1_1, 0x00108113u,
                    NOP, NOP, NOP, NOP, EBREAK, NOP, NOP, NOP, NOP});
    c.run();
    EXPECT_EQ(c.read_reg(1), 1u);
    EXPECT_EQ(c.read_reg(2), 2u);
}

// ── Load-use stall ───────────────────────────────────────────────────────────

// lw x1, 0(x3) followed immediately by addi x2, x1, 5.
// x3 = BASE (set before run via pre-write)
// lw x1, 0(x3) = 0x0001A083
// addi x2, x1, 5 = 0x00508113
TEST(Core, LoadUseStallThenForward) {
    Core c(cfg());
    c.store_word(BASE, 10u);
    c.write_reg(3, BASE);
    c.load_program({0x0001A083u, 0x00508113u,
                    NOP, NOP, NOP, NOP, NOP, EBREAK, NOP, NOP, NOP, NOP});
    c.run();
    EXPECT_EQ(c.read_reg(1), 10u);
    EXPECT_EQ(c.read_reg(2), 15u);
}

// ── Memory ───────────────────────────────────────────────────────────────────

// SW then LW round-trips a value through dmem.
// x1 = BASE (store base), x3 = 42 (value)
// sw x3, 0(x1)  then  lw x5, 0(x1)
TEST(Core, StoreLoadRoundTrip) {
    Core c(cfg());
    c.write_reg(1, BASE);
    c.load_program({ADDI_X3_42,
                    NOP, NOP, NOP,
                    SW_X3_0_X1,
                    NOP, NOP, NOP,
                    LW_X5_0_X1,
                    NOP, NOP, NOP, NOP, EBREAK, NOP, NOP, NOP, NOP});
    c.run();
    EXPECT_EQ(c.read_reg(5), 42u);
}

// ── Front-end (I-cache) stall ───────────────────────────────────────────────

// icache with a 4-byte line forces every fetch to be a cold miss (each
// instruction address is its own line), so we can pin down exactly when
// each fetch resolves.
static SimConfig icache_forced_miss_cfg() {
    SimConfig c = cfg();
    c.icache_enabled            = true;
    c.icache_line_bytes         = 4;
    c.icache_sets               = 4;
    c.icache_ways               = 1;
    c.icache_hit_latency_cycles = 1;
    c.imem_latency_cycles       = 5;   // miss cost = 5 + 1 = 6 cycles
    return c;
}

// idx0 (ADDI) is well past IF by the time idx1's (EBREAK) own fetch is
// still mid-I-miss. A front-end stall must hold only IF/PC — decode
// through writeback keep draining — so idx0 should retire while idx1 is
// still waiting on its fetch, not after. If an I-miss instead froze the
// whole pipe (like a MEM stall), idx0 could not reach WB until idx1's
// fetch resolved.
TEST(Core, FetchStallOnlyFreezesFrontEnd) {
    Core c(icache_forced_miss_cfg());
    c.load_program({ADDI_X1_1, EBREAK, NOP, NOP, NOP});

    for (int i = 0; i < 10; ++i) c.tick();

    EXPECT_EQ(c.read_reg(1), 1u);   // idx0 retired
    EXPECT_FALSE(c.halted());       // idx1 (EBREAK) has not reached WB yet
}

// mem_stall must dominate a concurrent fetch-stall: while MEM is waiting
// on a not-yet-served access, fetch is frozen entirely (not even its
// timer ticks) — the documented v1 pessimism. Use an I-miss (imem
// latency 20) long enough to still be in flight throughout either D-stall
// below, so if fetch kept ticking during the D-stall, some of that I-miss
// would resolve "for free" and the two penalties would partially overlap.
// Because mem_stall dominates, they are exactly additive: the total-cycle
// delta between two D-stall lengths equals the D-stall delta itself.
TEST(Core, MemStallDominatesFetchStall) {
    auto cycles_to_halt = [](unsigned dmem_latency) {
        SimConfig c = icache_forced_miss_cfg();
        c.imem_latency_cycles = 20;
        c.dmem_latency_cycles = dmem_latency;
        Core core(c);
        core.write_reg(2, BASE);
        core.load_program({LW_X1_0_X2, EBREAK, NOP, NOP, NOP});
        core.run(100000);
        return core.cycles();
    };

    uint64_t low  = cycles_to_halt(2);
    uint64_t high = cycles_to_halt(10);

    EXPECT_EQ(high - low, 10u - 2u);
}

// ── Retired instruction count ────────────────────────────────────────────────

// Straight-line code retires exactly one instruction per program word,
// up to and including the EBREAK that halts. The trailing NOPs are
// still in the pipeline at halt and never reach WB.
TEST(Core, CountsRetiredInstructions) {
    Core c(cfg());
    c.load_program({ADDI_X1_1, ADDI_X2_2, ADDI_X3_42, EBREAK,
                    NOP, NOP, NOP, NOP});
    c.run();
    ASSERT_TRUE(c.halted());
    EXPECT_EQ(c.instructions(), 4u);
    EXPECT_EQ(c.stats().instructions, c.instructions());
}

// Stalls cost cycles, never instructions: the same program under slow
// memory (load-use interlock + MEM stall + I-miss front-end stalls)
// retires the same count while taking strictly longer. Bubbles are
// invalid at WB, so they are not miscounted as retirements.
TEST(Core, StallsDoNotInflateInstructionCount) {
    // Core is not copyable, so return just the two counters.
    auto run = [](const SimConfig& sc) {
        Core c(sc);
        c.write_reg(2, BASE);
        c.load_program({LW_X1_0_X2, ADD_X4_X1_X2, EBREAK,
                        NOP, NOP, NOP, NOP});
        c.run(100000);
        EXPECT_TRUE(c.halted());
        return std::pair<uint64_t, uint64_t>{c.instructions(), c.cycles()};
    };

    SimConfig slow = icache_forced_miss_cfg();
    slow.dmem_latency_cycles = 10;

    auto [fast_insts, fast_cycles] = run(cfg());
    auto [slow_insts, slow_cycles] = run(slow);

    EXPECT_EQ(fast_insts, 3u);
    EXPECT_EQ(slow_insts, fast_insts);
    EXPECT_GT(slow_cycles, fast_cycles);
}

// ── Cycle budget ─────────────────────────────────────────────────────────────

// max_cycles caps the run even if no EBREAK is present.
TEST(Core, MaxCyclesCaps) {
    Core c(cfg());
    c.load_program({NOP, NOP, NOP, NOP, NOP});
    c.run(3);
    EXPECT_EQ(c.cycles(), 3u);
    EXPECT_FALSE(c.halted());
}
