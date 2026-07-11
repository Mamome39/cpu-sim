#include "cpusim/uarch/branch_predictor.h"
#include <cassert>

namespace cpusim {

static unsigned log2_exact(unsigned v) {
    assert(v && (v & (v - 1)) == 0 && "must be a power of two");
    unsigned n = 0;
    while (v > 1) { v >>= 1; ++n; }
    return n;
}

BranchPredictor::BranchPredictor(unsigned bht_entries, unsigned btb_entries)
    : bht_(bht_entries, 1)            // init weakly not-taken
    , bht_mask_(bht_entries - 1)
    , btb_(btb_entries)
    , btb_mask_(btb_entries - 1)
    , btb_index_bits_(log2_exact(btb_entries)) {}

// Instructions are word-aligned, so drop the low 2 PC bits.
unsigned BranchPredictor::bht_index(uint32_t pc) const {
    return (pc >> 2) & bht_mask_;
}

unsigned BranchPredictor::btb_index(uint32_t pc) const {
    return (pc >> 2) & btb_mask_;
}

uint32_t BranchPredictor::btb_tag(uint32_t pc) const {
    return (pc >> 2) >> btb_index_bits_;
}

BranchPredictor::Prediction BranchPredictor::predict(uint32_t pc) const {
    const BtbEntry& e = btb_[btb_index(pc)];
    if (!e.valid || e.tag != btb_tag(pc))
        return {};                       // BTB miss -> predict not-taken

    // Known branch: conditional direction from the counter; an
    // unconditional jump (JAL) is always taken.
    bool taken = e.cond ? (bht_[bht_index(pc)] >= 2) : true;
    return {taken, e.target};
}

void BranchPredictor::update(uint32_t pc, bool is_cond,
                             bool taken, uint32_t target) {
    if (is_cond) {
        uint8_t& c = bht_[bht_index(pc)];
        if (taken) { if (c < 3) ++c; }
        else       { if (c > 0) --c; }
    }

    BtbEntry& e = btb_[btb_index(pc)];
    e.valid  = true;
    e.tag    = btb_tag(pc);
    e.target = target;
    e.cond   = is_cond;
}

}  // namespace cpusim
