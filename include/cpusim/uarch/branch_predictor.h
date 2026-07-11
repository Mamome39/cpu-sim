#pragma once
#include <cstdint>
#include <vector>

namespace cpusim {

// BranchPredictor — a BTB + bimodal (2-bit saturating) direction
// predictor, consulted at IF by program counter alone (no decode).
//
//   BTB: direct-mapped, tagged, PC -> {target, is_conditional}.
//        Filled at EX when a branch/jump resolves. A miss means "not a
//        known branch" -> predict not-taken (fetch falls through).
//   BHT: 2-bit saturating counters, PC-indexed, giving the direction of
//        a conditional branch on a BTB hit (>= 2 = taken).
//
// Only deterministic-target instructions are stored (conditional
// branches and JAL, whose target is PC+imm), so a correct direction
// prediction is automatically a correct target — the pipeline need only
// carry the predicted direction. JALR is never stored (register target
// is not deterministic); it stays a BTB miss and resolves in EX.
//
// bht_entries and btb_entries must be powers of two.
class BranchPredictor {
public:
    BranchPredictor(unsigned bht_entries, unsigned btb_entries);

    struct Prediction {
        bool     taken  = false;
        uint32_t target = 0;   // valid only when taken
    };

    // IF: predict from PC alone. Not-taken on a BTB miss.
    Prediction predict(uint32_t pc) const;

    // EX: train on a resolved branch/jump. Call only for instructions
    // with a deterministic target (conditional branches and JAL).
    void update(uint32_t pc, bool is_cond, bool taken, uint32_t target);

private:
    unsigned bht_index(uint32_t pc) const;
    unsigned btb_index(uint32_t pc) const;
    uint32_t btb_tag(uint32_t pc)   const;

    std::vector<uint8_t> bht_;   // 2-bit counters (0..3)
    uint32_t bht_mask_;

    struct BtbEntry {
        bool     valid  = false;
        uint32_t tag    = 0;
        uint32_t target = 0;
        bool     cond   = false;
    };
    std::vector<BtbEntry> btb_;
    uint32_t btb_mask_;
    unsigned btb_index_bits_;
};

}  // namespace cpusim
