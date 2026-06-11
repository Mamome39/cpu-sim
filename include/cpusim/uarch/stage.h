#pragma once
#include "cpusim/uarch/latch.h"

// Stage — base interface for all pipeline stages.
//
// Each concrete stage owns the latch on its OUTPUT side and
// reads from the latch owned by the stage upstream of it.
//
//   IF  →[IF/ID latch]→  ID  →[ID/EX latch]→  EX  → ...
//        ^^^owned by IF       ^^^owned by ID
//
// Clock cycle protocol (driven by the top-level sim loop):
//   1. Call evaluate() on every stage (combinational, no commits).
//   2. Call latch() on every stage (sequential, all commit at once).
//
// Stall: hazard unit sets the stall flag before step 2;
//        the affected stage skips its latch() call.
// Flush: hazard unit sets the flush flag before step 2;
//        the affected latch is cleared to a bubble.

namespace cpusim {

class Stage {
public:
    virtual ~Stage() = default;

    // Combinational phase: read inputs, compute outputs into shadow.
    // Must NOT modify the visible register or any upstream latch.
    virtual void evaluate() = 0;

    // Sequential phase: commit shadow → register (clock edge).
    // Stall logic: implementations should check their stall flag
    // and skip the latch() call on their output latch if set.
    virtual void latch() = 0;
};

}  // namespace cpusim
