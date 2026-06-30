#include <gtest/gtest.h>
#include "cpusim/memory/flat_mem.h"
#include "cpusim/regfile.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/pipeline/fetch.h"
#include "cpusim/uarch/pipeline/decode.h"
#include "cpusim/uarch/pipeline/execute.h"
#include "cpusim/uarch/pipeline/mem_access.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"

using namespace cpusim;
using namespace cpusim::pipeline;
using namespace cpusim::rv32i;

// ── Encodings ─────────────────────────────────────────────────────────────────

// addi x1, x0, 1   0x00100093
// addi x2, x0, 2   0x00200113
// addi x3, x0, 42  0x02A00193
// sw x2, 0(x1)     0x00208023  (base=x1, data=x2, offset=0)
// lw x4, 0(x1)     0x00008203  (base=x1, offset=0)
// beq x0, x0, +12  0x00C00463
// nop (addi x0,x0,0) 0x00000013

static constexpr uint32_t BASE = 0x80000000;
static constexpr size_t   SZ   = 0x2000;

static constexpr uint32_t NOP         = 0x00000013u;
static constexpr uint32_t ADDI_X1_1   = 0x00100093u;
static constexpr uint32_t ADDI_X2_2   = 0x00200113u;
static constexpr uint32_t ADDI_X3_42  = 0x02A00193u;
static constexpr uint32_t SW_X2_0_X1  = 0x0020A023u;  // funct3=010
static constexpr uint32_t LW_X4_0_X1  = 0x0000A203u;  // funct3=010
// beq x0, x0, +12
static constexpr uint32_t BEQ_SKIP3   = 0x00000663u;

// ── Pipeline fixture ──────────────────────────────────────────────────────────

struct Pipeline {
    FlatMem  imem{BASE, SZ};
    FlatMem  dmem{BASE, SZ};
    RegFile  rf;

    Latch<IfId>  if_id;
    Latch<IdEx>  id_ex;
    Latch<ExMem> ex_mem;
    Latch<MemWb> mem_wb;

    FetchStage     fetch {imem, if_id, BASE};
    DecodeStage    decode{rf, if_id, id_ex};
    ExecuteStage   ex    {id_ex, ex_mem, fetch, decode};
    MemAccessStage mem   {dmem, ex_mem, mem_wb};

    void tick() {
        fetch.evaluate();
        decode.evaluate();
        ex.evaluate();
        mem.evaluate();
        // latch order: mem first (no signals), then ex (signals fetch+decode),
        // then decode and fetch consume those signals.
        mem.latch();
        ex.latch();
        decode.latch();
        fetch.latch();
    }

    void load_program(std::initializer_list<uint32_t> words) {
        uint32_t addr = BASE;
        for (uint32_t w : words) {
            imem.store_word(addr, w);
            addr += 4;
        }
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

// A single addi takes 4 cycles to reach MemWb (pipeline fill).
TEST(Pipeline, AddiReachesMemWbAfterFourCycles) {
    Pipeline p;
    p.load_program({ADDI_X3_42, NOP, NOP, NOP});

    for (int i = 0; i < 4; ++i) p.tick();

    EXPECT_TRUE(p.mem_wb.read().valid);
    EXPECT_EQ(p.mem_wb.read().op,     Op::ADDI);
    EXPECT_EQ(p.mem_wb.read().rd,     3u);
    EXPECT_EQ(p.mem_wb.read().wb_val, 42u);
}

// Two addi instructions should appear in MemWb on consecutive cycles.
TEST(Pipeline, TwoInstrsFlowInOrder) {
    Pipeline p;
    p.load_program({ADDI_X1_1, ADDI_X2_2, NOP, NOP, NOP});

    for (int i = 0; i < 4; ++i) p.tick();
    EXPECT_EQ(p.mem_wb.read().rd,     1u);
    EXPECT_EQ(p.mem_wb.read().wb_val, 1u);

    p.tick();
    EXPECT_EQ(p.mem_wb.read().rd,     2u);
    EXPECT_EQ(p.mem_wb.read().wb_val, 2u);
}

// BEQ x0,x0 is always taken. After it resolves in EX (cycle 3),
// the two wrong-path slots behind it must be bubbles, and PC must
// have redirected to the branch target.
TEST(Pipeline, BranchTakenFlushesWrongPath) {
    Pipeline p;
    // beq x0,x0,+12 at BASE → target = BASE+12
    // slots BASE+4 and BASE+8 are wrong-path addi's that must be flushed
    p.load_program({BEQ_SKIP3,
                    ADDI_X1_1,   // wrong-path — must become bubble
                    ADDI_X2_2,   // wrong-path — must become bubble
                    ADDI_X3_42}); // target (BASE+12): should execute

    // Run until BEQ reaches EX (3 cycles) and its latch() fires redirect.
    for (int i = 0; i < 3; ++i) p.tick();

    // After redirect: IfId and IdEx must be bubbles.
    EXPECT_FALSE(p.if_id.read().valid);
    EXPECT_FALSE(p.id_ex.read().valid);

    // PC must be at the branch target.
    EXPECT_EQ(p.fetch.pc(), BASE + 12u);
}

// SW followed by enough NOPs for the store to complete, then LW.
// Verifies memory is written and read back through the pipeline.
// (No hazard unit yet — NOPs separate the hazard manually.)
TEST(Pipeline, StoreWordThenLoadWord) {
    Pipeline p;
    // x1 = 1 (base address = BASE+1... use dmem base directly)
    // We set up x1 in the regfile manually to avoid hazard.
    p.rf.write(1, BASE);  // base address
    p.rf.write(2, 0xABCDu);  // store data

    // sw x2, 0(x1) then NOPs then lw x4, 0(x1)
    p.load_program({SW_X2_0_X1, NOP, NOP, NOP, LW_X4_0_X1, NOP, NOP, NOP});

    // Run enough cycles for SW to reach MEM (4 cycles) and
    // LW to reach MEM (4 more cycles after SW).
    for (int i = 0; i < 8; ++i) p.tick();

    EXPECT_EQ(p.mem_wb.read().op,     Op::LW);
    EXPECT_EQ(p.mem_wb.read().wb_val, 0xABCDu);
}
