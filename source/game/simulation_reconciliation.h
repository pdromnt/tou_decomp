#ifndef TOU_SIMULATION_RECONCILIATION_H
#define TOU_SIMULATION_RECONCILIATION_H
#include "input_timeline.h"
#include "presentation_events.h"

// Experimental game-thread owner; no sockets, pacing, rendering, or level I/O.
// Prediction mode owns full-copy world/local checkpoints and the event journal.
// Live lockstep may borrow a control-only anchor; prediction remains isolated.
class SimulationReconciliation {
public:
    SimulationReconciliation() = default;
    enum InputMode { DirectInputs, HostRevisions };
    enum Control { OpenPauseMenu = 1, ResumeMatch, SkipLevel, EndMatch };
    enum Result { Advanced, Waiting, Terminal, Replayed, Unchanged, Failed, Replaying };
    struct FrameWork {
        Result result = Unchanged;
        uint32_t replay_ticks = 0, forward_ticks = 0, pending_ticks = 0;
        bool clamped = false;
    };
    struct Checkpoint {
        uint32_t tick = 0;
        bool valid = false, continued = false;
        std::vector<uint8_t> state;
        PresentationCheckpoint local;
        uint64_t events = 0;
    };
    ~SimulationReconciliation() { stop(); }
    bool start(uint32_t generation, uint8_t players, uint32_t window,
               size_t budget, bool deliver = false, InputMode mode = DirectInputs);
    // Borrow an already-confirmed live lockstep anchor for controls only.
    // Never owns a journal, advances physics, or changes local presentation.
    bool start_live_control(uint32_t generation, uint8_t players, uint32_t epoch, size_t budget);
    void stop();
    InputTimeline::Result submit(uint32_t generation, uint32_t tick,
                                 uint8_t player, uint8_t actions, uint32_t epoch = 1);
    InputTimeline::Result revise(const AuthoritativeInput &input);
    bool canonical_input(uint32_t tick, uint8_t *actions, bool *sealed) const;
    bool export_confirmed(uint32_t new_epoch, ConfirmedState *state) const;
    // Prepare a controlled anchor without changing the running world. Requires
    // quiescing at a fully confirmed current tick; applied only by the barrier.
    bool export_controlled(uint32_t new_epoch, Control control, ConfirmedState *state) const;
    bool apply_confirmed(const ConfirmedState &state);
    uint32_t epoch() const { return epoch_; }
    Result advance();
    Result reconcile(uint32_t max_ticks = InputTimeline::MaximumPrediction);
    // Shared replay/forward tick budget, at most one restore per call. Tick time
    // matches the existing 16 ms loop. No hard wall-time bound for one tick.
    FrameWork pump(uint32_t elapsed_ms, uint32_t max_ticks, uint32_t target_tick = UINT32_MAX);
    bool confirm();
    bool can_finalize() const;
    const Checkpoint *checkpoint(uint32_t tick) const;
    uint32_t simulated_tick() const { return inputs_.simulated_tick(); }
    uint32_t confirmed_tick() const { return inputs_.confirmed_tick(); }
    uint32_t committed_tick() const { return control_only_ ? inputs_.confirmed_tick() : PresentationEvents_ConfirmedTick(); }
    uint32_t dirty_from() const { return inputs_.dirty_from(); }
    uint64_t replayed_ticks() const { return replayed_; }
    uint32_t corrections() const { return corrections_; }
    uint32_t truncated_tails() const { return truncated_; }
    size_t peak_bytes() const { return peak_; }
    bool healthy() const { return ready_ && !failed_; }
    bool replaying() const { return replay_end_ != 0; }
    uint32_t queued_inputs() const { return queued_; }
private:
    SimulationReconciliation(const SimulationReconciliation &) = delete;
    SimulationReconciliation &operator=(const SimulationReconciliation &) = delete;
    InputTimeline inputs_;
    Checkpoint history_[InputTimeline::MaximumPrediction + 1];
    bool capture(bool continued);
    bool aligned() const;
    bool world_matches_checkpoint() const;
    Result fail();
    Result run_tick();
    bool drain_inputs();
    struct PendingInput {
        uint32_t tick = 0;
        uint8_t player = 0, actions = 0;
        bool revision = false;
        AuthoritativeInput authority;
    };
    enum { MaximumQueuedInputs = 128 };
    PendingInput pending_[MaximumQueuedInputs];
    bool ready_ = false, failed_ = false;
    uint32_t window_ = 0, players_ = 0, corrections_ = 0, truncated_ = 0;
    uint64_t replayed_ = 0;
    size_t budget_ = 0, peak_ = 0;
    uint32_t generation_ = 0, replay_end_ = 0, queued_ = 0, debt_ms_ = 0;
    uint32_t epoch_ = 1, base_tick_ = 0;
    uint8_t base_actions_[4] = {};
    InputMode mode_ = DirectInputs;
    bool deliver_ = false;
    bool control_only_ = false;
};
#endif
