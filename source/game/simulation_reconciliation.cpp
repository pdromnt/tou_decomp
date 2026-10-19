#include "tou.h"
#include "simulation_reconciliation.h"
#include <algorithm>

void SimulationReconciliation::stop()
{
    if (ready_) PresentationEvents_Stop();
    ready_ = failed_ = false;
    replay_end_ = queued_ = debt_ms_ = 0;
    inputs_ = InputTimeline();
    for (size_t i = 0; i < InputTimeline::MaximumPrediction + 1; ++i)
        history_[i] = Checkpoint();
}
bool SimulationReconciliation::start(uint32_t generation, uint8_t players,
    uint32_t window, size_t budget, bool deliver)
{
    stop();
    if (!window || window > InputTimeline::MaximumPrediction ||
        players != DAT_00489240 || SimulationState_Tick() != 0 ||
        Netplay_IsEnabled() || Replay_IsActive() || PresentationEvents_Enabled() ||
        !inputs_.reset(generation, players, window)) return false;
    window_ = window; players_ = players; budget_ = budget;
    generation_ = generation;
    corrections_ = truncated_ = 0; replayed_ = 0; peak_ = 0;
    PresentationEvents_Start(deliver);
    ready_ = true;
    if (!capture(false) || history_[0].state.capacity() > budget / (window + 1)) {
        stop(); return false;
    }
    return true;
}
bool SimulationReconciliation::aligned() const
{
    return healthy() && SimulationState_Tick() == inputs_.simulated_tick() &&
        PresentationEvents_Enabled() && PresentationEvents_Healthy() &&
        PresentationEvents_ConfirmedTick() <= inputs_.confirmed_tick();
}
SimulationReconciliation::Result SimulationReconciliation::fail()
{
    failed_ = true;
    LOG("[RECONCILE] failed sim=%u input=%u confirmed=%u dirty=%u\n",
        SimulationState_Tick(), inputs_.simulated_tick(), inputs_.confirmed_tick(), inputs_.dirty_from());
    return Failed;
}
const SimulationReconciliation::Checkpoint *SimulationReconciliation::checkpoint(uint32_t tick) const
{
    if (!healthy()) return NULL;
    const Checkpoint &slot = history_[tick % (window_ + 1)];
    return slot.valid && slot.tick == tick && tick <= inputs_.simulated_tick() ? &slot : NULL;
}
bool SimulationReconciliation::capture(bool continued)
{
    const uint32_t tick = SimulationState_Tick();
    Checkpoint &slot = history_[tick % (window_ + 1)];
    if (slot.valid && slot.tick != tick && slot.tick >= inputs_.confirmed_tick()) return false;
    slot.valid = false;
    if (!SimulationState_Capture(&slot.state)) return false;
    PresentationEvents_Capture(&slot.local);
    slot.events = tick ? PresentationEvents_Digest(tick) : 0;
    slot.tick = tick; slot.continued = continued;
    size_t bytes = 0;
    for (size_t i = 0; i <= window_; ++i) {
        const size_t size = history_[i].state.capacity() + history_[i].local.particle_tags.capacity();
        if (size > budget_ - bytes) return false;
        bytes += size;
    }
    peak_ = std::max(peak_, bytes);
    slot.valid = true;
    return true;
}
InputTimeline::Result SimulationReconciliation::submit(uint32_t generation,
    uint32_t tick, uint8_t player, uint8_t actions)
{
    if (!aligned()) { fail(); return InputTimeline::Invalid; }
    if (replaying()) {
        if (generation != generation_) return InputTimeline::WrongGeneration;
        if (!tick || player >= players_ || (actions & 0x80)) {
            fail(); return InputTimeline::Invalid;
        }
        if (tick > replay_end_ && tick - replay_end_ >= InputTimeline::Capacity) {
            fail(); return InputTimeline::TooFar;
        }
        for (uint32_t i = 0; i < queued_; ++i)
            if (pending_[i].tick == tick && pending_[i].player == player) {
                if (pending_[i].actions == actions) return InputTimeline::Duplicate;
                fail(); return InputTimeline::Conflict;
            }
        if (queued_ == MaximumQueuedInputs) {
            fail(); return InputTimeline::TooFar;
        }
        pending_[queued_++] = PendingInput{tick, player, actions};
        return InputTimeline::Deferred;
    }
    const InputTimeline::Result result = inputs_.submit(generation, tick, player, actions);
    if (result == InputTimeline::Invalid || result == InputTimeline::Conflict || result == InputTimeline::TooFar)
        fail();
    return result;
}
SimulationReconciliation::Result SimulationReconciliation::run_tick()
{
    uint8_t actions[InputTimeline::Players] = {};
    const uint32_t tick = inputs_.simulated_tick() + 1;
    if (!inputs_.prepare(tick, actions)) return Waiting;
    FUN_0045e1f0(); // Canonical LAN policy; never a wall-clock batch reset.
    const bool continued = Gameplay_AdvanceSimulation(actions, players_);
    if (SimulationState_Tick() != tick || !PresentationEvents_Healthy() ||
        !inputs_.simulated(tick) || !capture(continued)) return fail();
    return g_SubState == GAMEPLAY_ACTIVE ? Advanced : Terminal;
}
SimulationReconciliation::Result SimulationReconciliation::advance()
{
    if (!aligned()) return fail();
    if (inputs_.dirty_from() || replaying()) return Waiting; // Reconcile before advancing time.
    if (g_SubState != GAMEPLAY_ACTIVE) return Terminal;
    // Even known input cannot outrun uncommitted effects/checkpoint retention.
    // The caller must confirm before reusing the anchor's ring slot.
    if (inputs_.simulated_tick() - PresentationEvents_ConfirmedTick() >= window_)
        return Waiting;
    return run_tick();
}
bool SimulationReconciliation::drain_inputs()
{
    const uint32_t count = queued_;
    queued_ = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const PendingInput &input = pending_[i];
        const InputTimeline::Result result = inputs_.submit(generation_, input.tick, input.player, input.actions);
        if (result == InputTimeline::Invalid || result == InputTimeline::Conflict || result == InputTimeline::TooFar)
            return false;
    }
    return true;
}
SimulationReconciliation::Result SimulationReconciliation::reconcile(uint32_t max_ticks)
{
    if (!aligned()) return fail();
    if (!max_ticks) return Waiting;
    if (max_ticks > InputTimeline::MaximumPrediction) return fail();
    if (!replaying()) {
        const uint32_t dirty = inputs_.dirty_from();
        if (!dirty) return Unchanged;
        const uint32_t end = inputs_.simulated_tick();
        const Checkpoint *base = checkpoint(dirty - 1);
        if (!base || dirty <= PresentationEvents_ConfirmedTick() ||
            end - dirty >= window_ || !SimulationState_Restore(base->state.data(), base->state.size(), true))
            return fail();
        PresentationEvents_Restore(base->local);
        if (!PresentationEvents_Healthy() || !PresentationEvents_DiscardAfter(dirty - 1) ||
            !inputs_.begin_replay(dirty)) return fail();
        replay_end_ = end;
        ++corrections_;
    }
    for (uint32_t work = 0; work < max_ticks; ++work) {
        const uint32_t tick = inputs_.simulated_tick() + 1;
        const Result result = run_tick();
        if (result != Advanced && result != Terminal) return fail();
        ++replayed_;
        if (result == Terminal && tick < replay_end_) {
            if (!inputs_.truncate_replay(tick) || !PresentationEvents_DiscardAfter(tick)) return fail();
            for (size_t i = 0; i <= window_; ++i)
                if (history_[i].tick > tick) history_[i].valid = false;
            ++truncated_;
            replay_end_ = 0;
        } else if (tick == replay_end_) {
            replay_end_ = 0;
        }
        if (!replaying()) return drain_inputs() ? Replayed : fail();
    }
    return Replaying;
}
SimulationReconciliation::FrameWork SimulationReconciliation::pump(uint32_t elapsed_ms,
    uint32_t max_ticks, uint32_t target_tick)
{
    FrameWork work;
    if (!aligned() || !max_ticks || max_ticks > InputTimeline::MaximumPrediction) {
        work.result = fail(); return work;
    }
    const uint32_t cap = window_ * 16;
    const uint32_t elapsed = std::min(elapsed_ms, 250u);
    work.clamped = elapsed != elapsed_ms || elapsed > cap - debt_ms_;
    debt_ms_ = std::min(cap, debt_ms_ + elapsed);
    if (replaying() || inputs_.dirty_from()) {
        const uint64_t before = replayed_;
        work.result = reconcile(max_ticks);
        work.replay_ticks = static_cast<uint32_t>(replayed_ - before);
        if (work.result == Failed) return work;
        // A queued arrival can dirty an earlier tick at the end of a burst.
        // Never restore twice in one frame, even with spare tick budget.
        if (replaying() || inputs_.dirty_from()) {
            work.result = Replaying;
            work.pending_ticks = debt_ms_ / 16;
            return work;
        }
    }
    while (work.replay_ticks + work.forward_ticks < max_ticks && debt_ms_ >= 16 &&
           inputs_.simulated_tick() < target_tick) {
        const uint32_t before = inputs_.simulated_tick();
        work.result = advance();
        // advance() also returns Terminal without advancing an already-ended
        // world. Only actual forward ticks consume debt and frame work.
        if (inputs_.simulated_tick() == before + 1) {
            ++work.forward_ticks; debt_ms_ -= 16;
        }
        if (work.result != Advanced) break;
    }
    work.pending_ticks = debt_ms_ / 16;
    return work;
}
bool SimulationReconciliation::confirm()
{
    if (!aligned()) { fail(); return false; }
    if (inputs_.dirty_from() || replaying()) return false;
    const uint32_t tick = inputs_.confirmed_tick();
    if (!checkpoint(tick) || !PresentationEvents_CommitThrough(tick) || !inputs_.retain_from(tick)) {
        fail(); return false;
    }
    // Retain the confirmed checkpoint as the restore anchor, reuse older storage.
    for (size_t i = 0; i <= window_; ++i)
        if (history_[i].tick < tick) history_[i].valid = false;
    return true;
}
bool SimulationReconciliation::can_finalize() const
{
    return aligned() && !replaying() && !inputs_.dirty_from() && inputs_.can_finalize(SimulationState_Tick()) &&
        PresentationEvents_CanFinalize(SimulationState_Tick());
}
