#pragma once
#include <cstdint>

// Latch<T> — a single pipeline register.
//
// Models the flip-flop bank that separates two pipeline stages.
// In real hardware, all flip-flops capture on the same clock edge.
// Here that is replicated by a two-phase protocol:
//
//   Phase 1 — evaluate():
//     Upstream stage writes its output into the shadow register.
//     The current register is still readable by the downstream
//     stage, so both stages see a consistent snapshot.
//
//   Phase 2 — latch():
//     Shadow is committed to the register (rising clock edge).
//     All stages advance simultaneously.
//
// Stall  : skip latch() — register holds its value next cycle.
// Flush  : call flush() — register is replaced with a bubble.
//
// T must be default-constructible; the default value is the bubble
// (NOP state) inserted on flush or at reset.

namespace cpusim {

template <typename T>
class Latch {
public:
    // Read the current register (downstream stage calls this).
    const T& read() const { return reg_; }

    // Write the shadow register (upstream stage calls this).
    void write(const T& val) { shadow_ = val; }

    // Commit shadow → register (rising clock edge).
    // Skip this call to stall; the register keeps its value.
    void latch() { reg_ = shadow_; }

    // Replace the register with a bubble next cycle.
    // Does NOT touch the shadow — the upstream stage is unaffected.
    void flush() { reg_ = T{}; shadow_ = T{}; }

    // Reset both register and shadow to the bubble state.
    void reset() { reg_ = T{}; shadow_ = T{}; }

private:
    T reg_{};     // visible to downstream stage
    T shadow_{};  // written by upstream stage this cycle
};

// Stall flag pair passed through the pipeline each cycle.
// Each stage checks whether it should skip latch().
struct StallSignals {
    bool if_stall  = false;
    bool id_stall  = false;
    bool ex_stall  = false;
    bool mem_stall = false;
};

// Flush flag pair: which latch boundaries to clear.
struct FlushSignals {
    bool if_id_flush  = false; // flush IF/ID latch (branch mispredict)
    bool id_ex_flush  = false; // flush ID/EX latch
};

}  // namespace cpusim
