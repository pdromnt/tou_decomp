#ifndef TOU_SIMULATION_RECONCILIATION_H
#define TOU_SIMULATION_RECONCILIATION_H
#include "input_timeline.h"
#include "presentation_events.h"

// Experimental game-thread owner; no sockets, pacing, rendering, or level I/O.
// Owns full-copy world/local checkpoints and the presentation journal lifetime.
// Never created by ordinary gameplay or live LAN yet.
class SimulationReconciliation {
public:
    SimulationReconciliation() = default;
    enum Result { Advanced, Waiting, Terminal, Replayed, Unchanged, Failed };
    struct Checkpoint {
        uint32_t tick = 0;
        bool valid = false, continued = false;
        std::vector<uint8_t> state;
        PresentationCheckpoint local;
        uint64_t events = 0;
    };
    ~SimulationReconciliation() { stop(); }
    bool start(uint32_t generation, uint8_t players, uint32_t window,
               size_t budget, bool deliver = false);
    void stop();
    InputTimeline::Result submit(uint32_t generation, uint32_t tick,
                                 uint8_t player, uint8_t actions);
    Result advance();
    Result reconcile();
    bool confirm();
    bool can_finalize() const;
    const Checkpoint *checkpoint(uint32_t tick) const;
    uint32_t simulated_tick() const { return inputs_.simulated_tick(); }
    uint32_t confirmed_tick() const { return inputs_.confirmed_tick(); }
    uint32_t dirty_from() const { return inputs_.dirty_from(); }
    uint64_t replayed_ticks() const { return replayed_; }
    uint32_t corrections() const { return corrections_; }
    uint32_t truncated_tails() const { return truncated_; }
    size_t peak_bytes() const { return peak_; }
    bool healthy() const { return ready_ && !failed_; }
private:
    SimulationReconciliation(const SimulationReconciliation &) = delete;
    SimulationReconciliation &operator=(const SimulationReconciliation &) = delete;
    InputTimeline inputs_;
    Checkpoint history_[InputTimeline::MaximumPrediction + 1];
    bool capture(bool continued);
    bool aligned() const;
    Result fail();
    Result run_tick();
    bool ready_ = false, failed_ = false;
    uint32_t window_ = 0, players_ = 0, corrections_ = 0, truncated_ = 0;
    uint64_t replayed_ = 0;
    size_t budget_ = 0, peak_ = 0;
};
#endif
