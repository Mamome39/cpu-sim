#include <gtest/gtest.h>
#include "cpusim/sim/core.h"

using namespace cpusim;

static constexpr uint32_t BASE = 0x80000000u;
static constexpr size_t   SZ   = 0x10000u;

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

// ── Basic arithmetic ─────────────────────────────────────────────────────────

// EBREAK halts the core; no prior instructions means all regs stay 0.
TEST(Core, EbreakHalts) {
    Core c(BASE, SZ);
    c.load_program({EBREAK, NOP, NOP, NOP, NOP});
    c.run();
    EXPECT_TRUE(c.halted());
}

// Single addi reaches WB; EBREAK trails behind to halt cleanly.
TEST(Core, AddiWritesReg) {
    Core c(BASE, SZ);
    c.load_program({ADDI_X3_42, NOP, NOP, NOP, EBREAK, NOP, NOP, NOP, NOP});
    c.run();
    EXPECT_EQ(c.read_reg(3), 42u);
}

// R-type ADD using two previously-set registers.
TEST(Core, AddTwoRegs) {
    Core c(BASE, SZ);
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
    Core c(BASE, SZ);
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
    Core c(BASE, SZ);
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
    Core c(BASE, SZ);
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

// ── Cycle budget ─────────────────────────────────────────────────────────────

// max_cycles caps the run even if no EBREAK is present.
TEST(Core, MaxCyclesCaps) {
    Core c(BASE, SZ);
    c.load_program({NOP, NOP, NOP, NOP, NOP});
    c.run(3);
    EXPECT_EQ(c.cycles(), 3u);
    EXPECT_FALSE(c.halted());
}
