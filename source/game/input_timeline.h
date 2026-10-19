#ifndef TOU_INPUT_TIMELINE_H
#define TOU_INPUT_TIMELINE_H
#include <stdint.h>
#include <stddef.h>

// Pure game-thread input ledger. No sockets, world mutation or snapshot ownership.
class InputTimeline {
public:
    enum { Capacity = 512, Players = 4, MaximumPrediction = 12 };
    // Deferred is returned only by the reconciliation owner during sliced replay.
    enum Result { Accepted, Duplicate, Conflict, TooOld, WrongGeneration, TooFar, Invalid, Deferred };
    bool reset(uint32_t generation, uint8_t players, uint32_t prediction,
               uint32_t base = 0, const uint8_t *previous = NULL);
    Result submit(uint32_t generation, uint32_t tick, uint8_t player, uint8_t actions);
    bool prepare(uint32_t tick, uint8_t *actions);
    bool simulated(uint32_t tick);
    // Caller must restore world/local presentation at dirty_from()-1 first.
    bool begin_replay(uint32_t from);
    // A corrected terminal tick may invalidate the remaining speculative tail.
    bool truncate_replay(uint32_t tick);
    bool retain_from(uint32_t state_tick);
    uint32_t simulated_tick() const { return simulated_; }
    uint32_t confirmed_tick() const { return confirmed_; }
    uint32_t oldest_state() const { return oldest_; }
    uint32_t dirty_from() const { return dirty_; }
    bool can_finalize(uint32_t tick) const { return ready_ && tick <= confirmed_; }
private:
    struct Frame {
        uint32_t tick = 0;
        uint8_t actual[Players] = {}, used[Players] = {}, before[Players] = {}, mask = 0;
        bool ran = false, prepared = false;
    };
    Frame frames_[Capacity];
    Frame *frame(uint32_t tick);
    void confirm();
    uint32_t generation_ = 0, simulated_ = 0, confirmed_ = 0, oldest_ = 0;
    uint32_t prediction_ = 0, dirty_ = 0, replay_end_ = 0;
    uint8_t players_ = 0, last_[Players] = {};
    bool ready_ = false;
};
#endif
