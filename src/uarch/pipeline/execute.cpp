#include "cpusim/uarch/pipeline/execute.h"
#include "cpusim/uarch/alu.h"
#include "cpusim/isa/rv32i/decoder.h"

namespace cpusim {

ExecuteStage::ExecuteStage(const Latch<pipeline::IdEx>&  in,
                           Latch<pipeline::ExMem>&       out,
                           FetchStage&                   fetch,
                           DecodeStage&                  decode)
    : in_(in), out_(out), fetch_(fetch), decode_(decode) {}

void ExecuteStage::evaluate() {
    const pipeline::IdEx& id = in_.read();

    if (!id.valid) {
        out_.write(pipeline::ExMem{});  // bubble
        branch_taken_  = false;
        branch_target_ = 0;
        return;
    }

    // ALU operand B: immediate for I/S/B/U/J formats,
    // rs2_val for R-format.
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

    uint32_t a   = id.rs1_val;
    uint32_t b   = uses_imm ? static_cast<uint32_t>(id.imm) : id.rs2_val;
    AluOp    aop = alu_op_for(id.op);

    // AUIPC: ALU computes pc + imm, so override operand A.
    if (id.op == rv32i::Op::AUIPC) a = id.pc;

    uint32_t alu_out = alu_exec(aop, a, b);

    branch_taken_  = branch_taken(id.op, id.rs1_val, id.rs2_val);
    branch_target_ = id.pc + static_cast<uint32_t>(id.imm);

    // JAL/JALR write pc+4 as the return address.
    bool is_jump = (id.op == rv32i::Op::JAL ||
                    id.op == rv32i::Op::JALR);
    if (is_jump) {
        alu_out       = id.pc + 4;
        branch_taken_ = true;
        // JALR target: (rs1 + imm) & ~1
        if (id.op == rv32i::Op::JALR)
            branch_target_ = (id.rs1_val +
                               static_cast<uint32_t>(id.imm)) & ~1u;
    }

    pipeline::ExMem ex;
    ex.pc            = id.pc;
    ex.op            = id.op;
    ex.rd            = id.rd;
    ex.alu_out       = alu_out;
    ex.rs2_val       = id.rs2_val;
    ex.branch_taken  = branch_taken_;
    ex.branch_target = branch_target_;
    ex.valid         = true;

    out_.write(ex);
}

void ExecuteStage::latch() {
    if (branch_taken_) {
        fetch_.set_redirect(true, branch_target_);
        decode_.set_flush(true);
    }
    out_.latch();
    branch_taken_  = false;
    branch_target_ = 0;
}

}  // namespace cpusim
