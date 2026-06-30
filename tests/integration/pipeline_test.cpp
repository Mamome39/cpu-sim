#include <gtest/gtest.h>
#include "cpusim/memory/flat_mem.h"
#include "cpusim/regfile.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/pipeline/fetch.h"
#include "cpusim/uarch/pipeline/decode.h"
#include "cpusim/uarch/pipeline/forward.h"
#include "cpusim/uarch/pipeline/execute.h"
#include "cpusim/uarch/pipeline/mem_access.h"
#include "cpusim/uarch/pipeline/writeback.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"
#include "cpusim/uarch/hazard.h"

using namespace cpusim;
using namespace cpusim::pipeline;
using namespace cpusim::rv32i;

// ── Encodings ─────────────────────────────────────────────────────────────────
// addi x1, x0, 1    0x00100093
// addi x2, x0, 2    0x00200113
// addi x3, x0, 42   0x02A00193
// sw   x2, 0(x1)    0x0020A023  funct3=010
// lw   x4, 0(x1)    0x0000A203  funct3=010
// beq  x0, x0, +12  0x00000663
// nop (addi x0,x0,0) 0x00000013

static constexpr uint32_t BASE = 0x80000000;
static constexpr size_t   SZ   = 0x2000;

static constexpr uint32_t NOP        = 0x00000013u;
static constexpr uint32_t ADDI_X1_1  = 0x00100093u;
static constexpr uint32_t ADDI_X2_2  = 0x00200113u;
static constexpr uint32_t ADDI_X3_42 = 0x02A00193u;
static constexpr uint32_t SW_X2_0_X1 = 0x0020A023u;
static constexpr uint32_t LW_X4_0_X1 = 0x0000A203u;
static constexpr uint32_t BEQ_SKIP3  = 0x00000663u;  // beq x0,x0,+12

// ── Pipeline fixture ──────────────────────────────────────────────────────────

struct Pipeline {
    FlatMem  imem{BASE, SZ};
    FlatMem  dmem{BASE, SZ};
    RegFile  rf;

    Latch<IfId>  if_id;
    Latch<IdEx>  id_ex;
    Latch<ExMem> ex_mem;
    Latch<MemWb> mem_wb;

    FetchStage      fetch {imem, if_id, BASE};
    DecodeStage     decode{rf, if_id, id_ex};
    ForwardUnit     fwd   {ex_mem, mem_wb};
    ExecuteStage    ex    {id_ex, ex_mem, fwd, fetch, decode};
    MemAccessStage  mem   {dmem, ex_mem, mem_wb};
    WritebackStage  wb    {rf, mem_wb};
    HazardUnit      hazard{if_id, id_ex, fetch, decode};

    void tick() {
        fetch.evaluate();
        decode.evaluate();
        fwd.evaluate();    // snapshot mem_wb for 3-instr-gap forwarding
        ex.evaluate();
        mem.evaluate();
        wb.evaluate();
        hazard.evaluate(); // detect load-use

        // latch order: fwd before mem (snapshot committed first),
        // ex and hazard before decode/fetch (outbound signals applied).
        wb.latch();
        fwd.latch();
        ex.latch();
        mem.latch();
        hazard.latch();
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

// addi x3, x0, 42 takes 5 cycles to reach WB (pipeline fill + writeback).
TEST(Pipeline, AddiWritesRegFileAfterFiveCycles) {
    Pipeline p;
    p.load_program({ADDI_X3_42, NOP, NOP, NOP, NOP});

    for (int i = 0; i < 5; ++i) p.tick();

    EXPECT_EQ(p.rf.read(3), 42u);
}

// Two addi results committed to the register file on consecutive cycles.
TEST(Pipeline, TwoInstrsWriteRegFileInOrder) {
    Pipeline p;
    p.load_program({ADDI_X1_1, ADDI_X2_2, NOP, NOP, NOP, NOP});

    for (int i = 0; i < 5; ++i) p.tick();
    EXPECT_EQ(p.rf.read(1), 1u);

    p.tick();
    EXPECT_EQ(p.rf.read(2), 2u);
}

// BEQ x0,x0 is always taken. Wrong-path instructions must be flushed;
// PC redirects to target; ADDI_X3_42 at target writes x3.
TEST(Pipeline, BranchTakenFlushesWrongPath) {
    Pipeline p;
    // beq x0,x0,+12 at BASE → target = BASE+12
    p.load_program({BEQ_SKIP3,
                    ADDI_X1_1,   // wrong-path — must be flushed
                    ADDI_X2_2,   // wrong-path — must be flushed
                    ADDI_X3_42}); // at BASE+12: correct path

    // 3 cycles: BEQ reaches EX → redirect fires.
    for (int i = 0; i < 3; ++i) p.tick();
    EXPECT_FALSE(p.if_id.read().valid);
    EXPECT_FALSE(p.id_ex.read().valid);
    EXPECT_EQ(p.fetch.pc(), BASE + 12u);

    // Wrong-path addi's must not have written x1 or x2.
    EXPECT_EQ(p.rf.read(1), 0u);
    EXPECT_EQ(p.rf.read(2), 0u);

    // Run until ADDI_X3_42 completes WB (2 more cycles to fill + WB).
    for (int i = 0; i < 5; ++i) p.tick();
    EXPECT_EQ(p.rf.read(3), 42u);
}

// EX->EX forwarding: ADDI x1,1 immediately followed by ADDI x2,x1,1.
// Without forwarding x2 would get 0+1=1; with forwarding x2=1+1=2.
// addi x2, x1, 1 = 0x00108113
TEST(Pipeline, ForwardingExToEx) {
    Pipeline p;
    p.load_program({ADDI_X1_1,    // x1 = 1
                    0x00108113u,  // addi x2, x1, 1  (x2 = x1+1 = 2)
                    NOP, NOP, NOP, NOP});
    for (int i = 0; i < 6; ++i) p.tick();
    EXPECT_EQ(p.rf.read(1), 1u);
    EXPECT_EQ(p.rf.read(2), 2u);
}

// Load-use stall: LW x1 followed immediately by ADDI x2,x1,5.
// Hazard unit inserts one bubble; MEM->EX forwarding delivers the result.
// lw x1, 0(x3) = 0x0001A083  (x3 holds BASE)
// addi x2, x1, 5 = 0x00508113
TEST(Pipeline, LoadUseStallThenForward) {
    Pipeline p;
    p.dmem.store_word(BASE, 10u);  // mem[BASE] = 10
    p.rf.write(3, BASE);           // x3 = base address
    p.load_program({0x0001A083u,   // lw  x1, 0(x3)
                    0x00508113u,   // addi x2, x1, 5
                    NOP, NOP, NOP, NOP, NOP});
    // lw completes WB at cycle 5; addi (stalled one cycle) at cycle 7.
    for (int i = 0; i < 9; ++i) p.tick();
    EXPECT_EQ(p.rf.read(1), 10u);
    EXPECT_EQ(p.rf.read(2), 15u);
}

// SW followed by NOPs then LW — store visible to the load through dmem.
TEST(Pipeline, StoreWordThenLoadWordWritesRegFile) {
    Pipeline p;
    p.rf.write(1, BASE);      // base address
    p.rf.write(2, 0xABCDu);   // store data

    p.load_program({SW_X2_0_X1, NOP, NOP, NOP, LW_X4_0_X1, NOP, NOP, NOP, NOP});

    // SW reaches MEM at cycle 4 (store committed to dmem).
    // LW reaches WB at cycle 9 (x4 written).
    for (int i = 0; i < 9; ++i) p.tick();

    EXPECT_EQ(p.rf.read(4), 0xABCDu);
}
