#include <gtest/gtest.h>
#include "cpusim/memory/flat_mem.h"
#include "cpusim/regfile.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/pipeline/fetch.h"
#include "cpusim/uarch/pipeline/decode.h"
#include "cpusim/uarch/pipeline/forward.h"
#include "cpusim/uarch/pipeline/execute.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"
#include "cpusim/uarch/hazard.h"
#include "cpusim/isa/rv32i/decoder.h"

using namespace cpusim;
using namespace cpusim::pipeline;
using namespace cpusim::rv32i;

static constexpr uint32_t PC = 0x80000000u;

static FlatMem& stub_imem() {
    static FlatMem m(PC, 0x1000);
    return m;
}

// ── ForwardUnit fixture ───────────────────────────────────────────────────────

struct FwdFixture {
    Latch<IfId>   if_id;
    Latch<IdEx>   id_ex;
    Latch<ExMem>  ex_mem;
    Latch<MemWb>  mem_wb;
    RegFile       rf;
    FetchStage    fetch{stub_imem(), if_id, PC};
    DecodeStage   decode{rf, if_id, id_ex};
    ForwardUnit   fwd{ex_mem, mem_wb};
    ExecuteStage  ex{id_ex, ex_mem, fwd, fetch, decode};

    // Commit a value into id_ex.
    void load_idex(Op op, uint8_t rd,
                   uint8_t rs1, uint32_t rs1_val,
                   uint8_t rs2, uint32_t rs2_val,
                   int32_t imm = 0) {
        IdEx d;
        d.op = op; d.rd = rd;
        d.rs1 = rs1; d.rs1_val = rs1_val;
        d.rs2 = rs2; d.rs2_val = rs2_val;
        d.imm = imm; d.valid = true;
        id_ex.write(d); id_ex.latch();
    }

    // Commit a value into ex_mem (simulates last cycle's EX output).
    void set_ex_mem(uint8_t rd, uint32_t alu_out) {
        ExMem e;
        e.rd = rd; e.alu_out = alu_out;
        e.op = Op::ADD; e.valid = true;
        ex_mem.write(e); ex_mem.latch();
    }

    // Commit a value into mem_wb (simulates last cycle's MEM output).
    void set_mem_wb(uint8_t rd, uint32_t wb_val) {
        MemWb w;
        w.rd = rd; w.wb_val = wb_val;
        w.op = Op::ADD; w.valid = true;
        mem_wb.write(w); mem_wb.latch();
    }

    // Run one evaluate+latch cycle without changing id_ex.
    void tick_idle() {
        fwd.evaluate();
        ex.evaluate();
        fwd.latch();
        ex.latch();
    }
};

// ── Forwarding: EX->EX (1-instruction gap) ────────────────────────────────────

// Producer result is in ex_mem (1 cycle old).
// Consumer ADD x3, x1, x2 reads x1 from ex_mem.
TEST(ForwardUnit, ExMemForwardsRs1) {
    FwdFixture f;
    f.set_ex_mem(1, 100);
    f.load_idex(Op::ADD, 3, 1, 0/*stale*/, 2, 5);
    f.fwd.evaluate();
    f.ex.evaluate(); f.fwd.latch(); f.ex.latch();
    // alu_out = forwarded_rs1 + rs2 = 100 + 5
    EXPECT_EQ(f.ex_mem.read().alu_out, 105u);
}

// Consumer uses x1 as rs2 (R-type), forwarded from ex_mem.
TEST(ForwardUnit, ExMemForwardsRs2) {
    FwdFixture f;
    f.set_ex_mem(1, 50);
    // ADD x3, x2, x1: rs1=x2=10, rs2=x1=forwarded 50
    f.load_idex(Op::ADD, 3, 2, 10, 1, 0/*stale*/);
    f.fwd.evaluate();
    f.ex.evaluate(); f.fwd.latch(); f.ex.latch();
    EXPECT_EQ(f.ex_mem.read().alu_out, 60u);
}

// ── Forwarding: MEM->EX (2-instruction gap) ───────────────────────────────────

// Producer result is in mem_wb; ex_mem has an unrelated register.
TEST(ForwardUnit, MemWbForwardsRs1) {
    FwdFixture f;
    f.set_mem_wb(1, 200);
    // ex_mem holds x5 — must NOT match x1
    f.set_ex_mem(5, 999);
    f.load_idex(Op::ADD, 3, 1, 0/*stale*/, 2, 10);
    f.fwd.evaluate();
    f.ex.evaluate(); f.fwd.latch(); f.ex.latch();
    // alu_out = forwarded_rs1 + rs2 = 200 + 10
    EXPECT_EQ(f.ex_mem.read().alu_out, 210u);
}

// ── Forwarding: WB->EX (3-instruction gap) ────────────────────────────────────

// Run an idle cycle so prev_mem_wb_ captures x1=300 from mem_wb.
// Then change mem_wb to an unrelated register and consume x1 in EX.
TEST(ForwardUnit, PrevMemWbForwardsRs1) {
    FwdFixture f;
    // Tick 1: mem_wb has x1=300; EX runs a bubble to capture snapshot.
    f.set_mem_wb(1, 300);
    f.tick_idle();
    // prev_mem_wb_ now holds {x1, 300}

    // Tick 2: mem_wb has x5=999, ex_mem has x6=111 — neither is x1.
    f.set_mem_wb(5, 999);
    f.set_ex_mem(6, 111);
    f.load_idex(Op::ADD, 3, 1, 0/*stale*/, 2, 7);
    f.fwd.evaluate();
    f.ex.evaluate(); f.fwd.latch(); f.ex.latch();
    // alu_out = prev_mem_wb(x1=300) + rs2(7)
    EXPECT_EQ(f.ex_mem.read().alu_out, 307u);
}

// ── Forwarding: priority — EX wins over MEM ───────────────────────────────────

TEST(ForwardUnit, ExMemTakesPriorityOverMemWb) {
    FwdFixture f;
    f.set_ex_mem(1, 77);   // EX path: x1=77
    f.set_mem_wb(1, 55);   // MEM path: x1=55 (lower priority)
    f.load_idex(Op::ADD, 3, 1, 0, 2, 0);
    f.fwd.evaluate();
    f.ex.evaluate(); f.fwd.latch(); f.ex.latch();
    EXPECT_EQ(f.ex_mem.read().alu_out, 77u);   // EX wins
}

// ── Forwarding: x0 always reads as zero ─────────────────────────────────────

TEST(ForwardUnit, X0AlwaysZero) {
    FwdFixture f;
    // ex_mem claims x0=999 — must be ignored
    f.set_ex_mem(0, 999);
    // ADD x3, x0, x2: rs1=x0 should yield 0, not 999
    f.load_idex(Op::ADD, 3, 0, 0, 2, 5);
    f.fwd.evaluate();
    f.ex.evaluate(); f.fwd.latch(); f.ex.latch();
    EXPECT_EQ(f.ex_mem.read().alu_out, 5u);   // 0 + 5
}

// ── Forwarding: store data (rs2) forwarded correctly ─────────────────────────

// SW x1, 0(x2): x1 was produced last cycle (in ex_mem).
// rs2 holds the store data; rs1 holds the base address.
TEST(ForwardUnit, StoreDataForwardedViaRs2) {
    FwdFixture f;
    f.set_ex_mem(1, 0xABCDu);  // x1=0xABCD is the store data
    // SW: rs1=x2 (base=0x80001000), rs2=x1 (data, stale=0)
    f.load_idex(Op::SW, 0, 2, 0x80001000u, 1, 0/*stale*/, 0);
    f.fwd.evaluate();
    f.ex.evaluate(); f.fwd.latch(); f.ex.latch();
    EXPECT_EQ(f.ex_mem.read().alu_out, 0x80001000u);  // address
    EXPECT_EQ(f.ex_mem.read().rs2_val, 0xABCDu);      // forwarded data
}

// ── HazardUnit fixture ────────────────────────────────────────────────────────

struct HazFixture {
    Latch<IfId>   if_id;
    Latch<IdEx>   id_ex;
    RegFile       rf;
    FetchStage    fetch{stub_imem(), if_id, PC};
    DecodeStage   decode{rf, if_id, id_ex};
    HazardUnit    hazard{if_id, id_ex, fetch, decode};

    void set_if_id(uint32_t raw) {
        IfId f; f.raw = raw; f.valid = true;
        if_id.write(f); if_id.latch();
    }

    void set_id_ex_load(uint8_t rd) {
        IdEx d; d.op = Op::LW; d.rd = rd; d.valid = true;
        id_ex.write(d); id_ex.latch();
    }

    void set_id_ex_nonload(uint8_t rd) {
        IdEx d; d.op = Op::ADD; d.rd = rd; d.valid = true;
        id_ex.write(d); id_ex.latch();
    }

    // Run hazard cycle then fetch+decode latch to consume signals.
    void run() {
        hazard.evaluate();
        hazard.latch();
    }
};

// ── HazardUnit tests ─────────────────────────────────────────────────────────

// LW x1 in EX; ADDI x3, x1, 5 in ID (rs1=x1 at bits[19:15]).
// addi x3, x1, 5 = 0x00508193
TEST(HazardUnit, LoadUseStallOnRs1) {
    HazFixture f;
    f.set_id_ex_load(1);
    f.set_if_id(0x00508193u);  // addi x3, x1, 5
    f.run();
    // Stall holds PC: after one evaluate+latch with stall, pc_ stays at PC.
    f.fetch.evaluate();
    f.fetch.latch();
    EXPECT_EQ(f.fetch.pc(), PC);
}

// LW x1 in EX; ADD x3, x2, x1 in ID (x1 as rs2 at bits[24:20]).
// add x3, x2, x1 = 0x001101B3
TEST(HazardUnit, LoadUseStallOnRs2) {
    HazFixture f;
    f.set_id_ex_load(1);
    f.set_if_id(0x001101B3u);  // add x3, x2, x1
    f.run();
    f.fetch.evaluate();
    f.fetch.latch();
    EXPECT_EQ(f.fetch.pc(), PC);
}

// Non-load in EX — no stall regardless of register match.
TEST(HazardUnit, NoStallForNonLoad) {
    HazFixture f;
    f.set_id_ex_nonload(1);
    f.set_if_id(0x00508193u);  // addi x3, x1, 5
    f.run();
    // No stall: one evaluate+latch advances PC by 4.
    f.fetch.evaluate();
    f.fetch.latch();
    EXPECT_EQ(f.fetch.pc(), PC + 4u);
}

// LW rd=x0 — writes to x0 are discarded; no stall needed.
TEST(HazardUnit, NoStallLoadRdIsX0) {
    HazFixture f;
    f.set_id_ex_load(0);
    f.set_if_id(0x00008013u);  // addi x0, x1, 0 — reads x1
    f.run();
    f.fetch.evaluate();
    f.fetch.latch();
    EXPECT_EQ(f.fetch.pc(), PC + 4u);
}

// No hazard when load.rd != consumer rs1 and rs2.
TEST(HazardUnit, NoStallNoDependency) {
    HazFixture f;
    f.set_id_ex_load(5);      // LW x5
    f.set_if_id(0x00208193u); // addi x3, x1, 2 — uses x1, not x5
    f.run();
    f.fetch.evaluate();
    f.fetch.latch();
    EXPECT_EQ(f.fetch.pc(), PC + 4u);
}

// Load-use stall: id_ex flush inserts a bubble.
// After stall, decode.latch() must produce invalid id_ex.
TEST(HazardUnit, StallFlushesDecode) {
    HazFixture f;
    f.set_id_ex_load(1);
    f.set_if_id(0x00508193u);  // addi x3, x1, 5
    f.hazard.evaluate();
    f.decode.evaluate();       // writes shadow from if_id
    f.hazard.latch();          // sets flush on decode
    f.decode.latch();          // flush wins — id_ex should be bubble
    EXPECT_FALSE(f.id_ex.read().valid);
}
