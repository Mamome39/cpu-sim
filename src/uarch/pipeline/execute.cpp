#include "cpusim/uarch/pipeline/execute.h"
#include "cpusim/uarch/alu.h"
#include "cpusim/isa/rv32i/decoder.h"

namespace cpusim {

ExecuteStage::ExecuteStage(const Latch<pipeline::IdEx>& in,
                           Latch<pipeline::ExMem>&      out,
                           ForwardUnit&                 fwd,
                           FetchStage&                  fetch,
                           DecodeStage&                 decode,
                           BranchPredictor*             bp,
                           SimStats*                    stats)
    : in_(in), out_(out), fwd_(fwd), fetch_(fetch), decode_(decode),
      bp_(bp), stats_(stats) {}

static bool is_cond_branch(rv32i::Op op) {
    switch (op) {
        case rv32i::Op::BEQ:  case rv32i::Op::BNE:
        case rv32i::Op::BLT:  case rv32i::Op::BGE:
        case rv32i::Op::BLTU: case rv32i::Op::BGEU:
            return true;
        default:
            return false;
    }
}

void ExecuteStage::evaluate() {
    const pipeline::IdEx& id = in_.read();

    if (!id.valid) {
        out_.write(pipeline::ExMem{});
        redirect_ = false;
        train_    = false;
        return;
    }

    uint32_t rs1 = fwd_.resolve(id.rs1, id.rs1_val);
    uint32_t rs2 = fwd_.resolve(id.rs2, id.rs2_val);

    // ALU operand B: immediate for I/S/B/U/J, rs2 for R-format.
    bool uses_imm = (id.op != rv32i::Op::ADD  &&
                     id.op != rv32i::Op::SUB  &&
                     id.op != rv32i::Op::AND  &&
                     id.op != rv32i::Op::OR   &&
                     id.op != rv32i::Op::XOR  &&
                     id.op != rv32i::Op::SLL  &&
                     id.op != rv32i::Op::SRL  &&
                     id.op != rv32i::Op::SRA  &&
                     id.op != rv32i::Op::SLT  &&
                     id.op != rv32i::Op::SLTU);

    uint32_t a   = rs1;
    uint32_t b   = uses_imm ? static_cast<uint32_t>(id.imm) : rs2;
    AluOp    aop = alu_op_for(id.op);

    // AUIPC: ALU computes pc + imm, so override operand A.
    if (id.op == rv32i::Op::AUIPC) a = id.pc;

    uint32_t alu_out = alu_exec(aop, a, b);

    bool     cond        = is_cond_branch(id.op);
    bool     actual_taken = branch_taken(id.op, rs1, rs2);
    uint32_t target       = id.pc + static_cast<uint32_t>(id.imm);

    // JAL/JALR write pc+4 as the return address and always jump.
    bool is_jal  = (id.op == rv32i::Op::JAL);
    bool is_jalr = (id.op == rv32i::Op::JALR);
    if (is_jal || is_jalr) {
        alu_out      = id.pc + 4;
        actual_taken = true;
        if (is_jalr)   // target from forwarded rs1, not deterministic
            target = (rs1 + static_cast<uint32_t>(id.imm)) & ~1u;
    }

    // Misprediction: the path IF fetched (predicted) differs from the
    // real outcome. Predicted-taken instructions have deterministic
    // targets (cond branch / JAL), so a matching direction means a
    // matching target — comparing direction is enough. Redirect to the
    // real next PC on a mismatch.
    redirect_        = (actual_taken != id.predicted_taken);
    redirect_target_ = actual_taken ? target : (id.pc + 4);

    // Train the predictor only on deterministic-target control ops.
    train_        = bp_ && (cond || is_jal);
    train_pc_     = id.pc;
    train_cond_   = cond;
    train_taken_  = actual_taken;
    train_target_ = target;

    pipeline::ExMem ex;
    ex.pc            = id.pc;
    ex.raw           = id.raw;
    ex.op            = id.op;
    ex.rd            = id.rd;
    ex.alu_out       = alu_out;
    ex.rs2_val       = rs2;   // forwarded store data
    ex.branch_taken  = actual_taken;
    ex.branch_target = target;
    ex.valid         = true;

    out_.write(ex);
}

void ExecuteStage::latch() {
    if (redirect_) {
        fetch_.set_redirect(true, redirect_target_);
        decode_.set_flush(true);
        if (stats_) ++stats_->branch_mispredicts;   // probe at resolve
    }
    if (train_)
        bp_->update(train_pc_, train_cond_, train_taken_, train_target_);
    out_.latch();
    redirect_ = false;
    train_    = false;
}

}  // namespace cpusim
