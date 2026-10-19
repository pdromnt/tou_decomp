#include "input_timeline.h"
#include <string.h>
#include <limits.h>

bool InputTimeline::reset(uint32_t generation, uint8_t players, uint32_t prediction,
                          uint32_t base, const uint8_t *previous)
{
    ready_ = false;
    if (!generation || !players || players > Players || prediction > MaximumPrediction ||
        base > UINT32_MAX - Capacity) return false;
    for (size_t p = 0; previous && p < players; ++p)
        if (previous[p] & 0x80) return false;
    generation_ = generation; players_ = players; prediction_ = prediction;
    simulated_ = confirmed_ = oldest_ = base;
    dirty_ = replay_end_ = 0;
    for (size_t i = 0; i < Capacity; ++i) frames_[i] = Frame();
    for (size_t p = 0; p < Players; ++p) last_[p] = previous && p < players ? previous[p] : 0;
    ready_ = true;
    return true;
}
InputTimeline::Frame *InputTimeline::frame(uint32_t tick)
{
    Frame &slot = frames_[tick % Capacity];
    if (slot.tick != tick) {
        if (slot.tick > oldest_ && slot.tick > confirmed_) return NULL;
        slot = Frame(); slot.tick = tick;
    }
    return &slot;
}
InputTimeline::Result InputTimeline::submit(uint32_t generation, uint32_t tick,
                                           uint8_t player, uint8_t actions)
{
    if (!ready_ || replay_end_ || player >= players_ || (actions & 0x80) || !tick) return Invalid;
    if (generation != generation_) return WrongGeneration;
    if (tick > simulated_ && tick - simulated_ >= Capacity) return TooFar;
    Frame &existing = frames_[tick % Capacity];
    const uint8_t bit = static_cast<uint8_t>(1u << player);
    if (existing.tick == tick && (existing.mask & bit))
        return existing.actual[player] == actions ? Duplicate : Conflict;
    if (tick <= confirmed_ || tick <= oldest_) return TooOld;
    Frame *slot = frame(tick);
    if (!slot) return TooFar;
    slot->actual[player] = actions; slot->mask |= bit;
    if (slot->ran && slot->used[player] != actions) {
        if (!dirty_ || tick < dirty_) dirty_ = tick;
    }
    confirm();
    return Accepted;
}
bool InputTimeline::prepare(uint32_t tick, uint8_t *actions)
{
    if (!ready_ || !actions || tick == 0 || simulated_ == UINT32_MAX ||
        tick != simulated_ + 1 || (dirty_ && !replay_end_)) return false;
    Frame *slot = frame(tick);
    if (!slot) return false;
    const uint8_t all = static_cast<uint8_t>((1u << players_) - 1u);
    if (tick - confirmed_ > 1 && tick - confirmed_ > prediction_) return false;
    if (slot->mask != all && tick - confirmed_ > prediction_) return false;
    memcpy(slot->before, last_, players_);
    for (size_t p = 0; p < players_; ++p) {
        // Holding last input preserves held-button edges; never invent a new press.
        slot->used[p] = (slot->mask & (1u << p)) ? slot->actual[p] :
            slot->authority.revision ? slot->authority.actions[p] : last_[p];
        actions[p] = slot->used[p];
    }
    slot->prepared = true;
    return true;
}
InputTimeline::Result InputTimeline::revise(const AuthoritativeInput &in)
{
    if (!ready_ || replay_end_ || !AuthorityInput_Valid(in) || in.players != players_) return Invalid;
    if (in.generation != generation_) return WrongGeneration;
    if (in.tick > simulated_ && in.tick - simulated_ >= Capacity) return TooFar;
    Frame &existing = frames_[in.tick % Capacity];
    if (existing.tick == in.tick && existing.authority.revision) {
        const AuthoritativeInput &old = existing.authority;
        if (old.epoch != in.epoch) return WrongGeneration;
        if (in.revision < old.revision) return StaleRevision;
        if (in.revision == old.revision) return AuthorityInput_Equal(old, in) ? Duplicate : Conflict;
        if (old.sealed && (!in.sealed || memcmp(old.actions, in.actions, players_) != 0)) return Conflict;
    }
    if (in.tick <= confirmed_ || in.tick <= oldest_) return TooOld;
    Frame *slot = frame(in.tick);
    if (!slot) return TooFar;
    // Direct receipts and authority are separate owner modes. Never reinterpret
    // a received actual action as a replaceable host prediction.
    if (slot->mask && !slot->authority.revision) return Conflict;
    slot->authority = in;
    if (in.sealed) {
        memcpy(slot->actual, in.actions, players_);
        slot->mask = static_cast<uint8_t>((1u << players_) - 1u);
    }
    if (slot->ran && memcmp(slot->used, in.actions, players_) != 0)
        if (!dirty_ || in.tick < dirty_) dirty_ = in.tick;
    confirm();
    return Accepted;
}
bool InputTimeline::used(uint32_t tick, uint8_t *actions, bool *sealed) const
{
    if (!ready_ || !actions || !sealed) return false;
    const Frame &slot = frames_[tick % Capacity];
    if (slot.tick != tick || !slot.ran || !slot.prepared) return false;
    memcpy(actions, slot.used, players_);
    *sealed = tick <= confirmed_;
    return true;
}
bool InputTimeline::simulated(uint32_t tick)
{
    if (!ready_ || simulated_ == UINT32_MAX || tick != simulated_ + 1) return false;
    Frame &slot = frames_[tick % Capacity];
    if (slot.tick != tick || !slot.prepared) return false;
    slot.ran = true; simulated_ = tick;
    memcpy(last_, slot.used, players_);
    if (replay_end_ && tick == replay_end_) { dirty_ = replay_end_ = 0; }
    confirm();
    return true;
}
void InputTimeline::confirm()
{
    const uint8_t all = static_cast<uint8_t>((1u << players_) - 1u);
    while (confirmed_ < simulated_) {
        const uint32_t next = confirmed_ + 1;
        const Frame &slot = frames_[next % Capacity];
        if ((dirty_ && next >= dirty_) || slot.tick != next || !slot.ran || slot.mask != all)
            break;
        bool equal = true;
        for (size_t p = 0; p < players_; ++p)
            if (slot.actual[p] != slot.used[p]) equal = false;
        if (!equal) break;
        confirmed_ = next;
    }
}
bool InputTimeline::begin_replay(uint32_t from)
{
    if (!ready_ || replay_end_ || !dirty_ || from != dirty_ ||
        from <= confirmed_ || from <= oldest_ || from > simulated_) return false;
    const Frame &first = frames_[from % Capacity];
    if (first.tick != from) return false;
    memcpy(last_, first.before, players_);
    replay_end_ = simulated_;
    simulated_ = from - 1;
    for (uint32_t tick = from; tick <= replay_end_; ++tick) {
        frames_[tick % Capacity].ran = frames_[tick % Capacity].prepared = false;
    }
    return true;
}
bool InputTimeline::retain_from(uint32_t state_tick)
{
    if (!ready_ || dirty_ || replay_end_ || state_tick < oldest_ || state_tick > confirmed_)
        return false;
    oldest_ = state_tick;
    return true;
}
bool InputTimeline::truncate_replay(uint32_t tick)
{
    if (!ready_ || !replay_end_ || tick != simulated_ || tick < dirty_ ||
        tick >= replay_end_) return false;
    for (uint32_t n = tick + 1; n <= replay_end_; ++n)
        frames_[n % Capacity].ran = frames_[n % Capacity].prepared = false;
    dirty_ = replay_end_ = 0;
    confirm();
    return true;
}
