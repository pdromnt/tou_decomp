#include "tou.h"
#include "simulation_reconciliation.h"
#include <algorithm>
#include <string.h>

void SimulationReconciliation::stop()
{
    if (ready_ && !control_only_) PresentationEvents_Stop();
    ready_ = failed_ = false;
    control_only_ = false;
    replay_end_ = queued_ = debt_ms_ = 0;
    inputs_ = InputTimeline();
    for (size_t i = 0; i < InputTimeline::MaximumPrediction + 1; ++i)
        history_[i] = Checkpoint();
}
bool SimulationReconciliation::start(uint32_t generation, uint8_t players,
    uint32_t window, size_t budget, bool deliver, InputMode mode)
{
    return start_internal(generation, players, window, budget, deliver, mode, false);
}
bool SimulationReconciliation::start_live_prediction(uint32_t generation, uint8_t players,
    uint32_t window, size_t budget, InputMode mode)
{
    if (!Netplay_PredictionCanStart()) return false;
    return start_internal(generation, players, window, budget, true, mode, true);
}
bool SimulationReconciliation::start_internal(uint32_t generation, uint8_t players,
    uint32_t window, size_t budget, bool deliver, InputMode mode, bool live)
{
    stop();
    if (!window || window > InputTimeline::MaximumPrediction ||
        players != DAT_00489240 || SimulationState_Tick() != 0 ||
        (!live && Netplay_IsEnabled()) || Replay_IsActive() || PresentationEvents_Enabled() ||
        (mode != DirectInputs && mode != HostRevisions) || !inputs_.reset(generation, players, window)) return false;
    window_ = window; players_ = players; budget_ = budget;
    generation_ = generation;
    epoch_ = 1; base_tick_ = 0; memset(base_actions_, 0, sizeof(base_actions_));
    mode_ = mode; deliver_ = deliver;
    corrections_ = truncated_ = 0; replayed_ = 0; peak_ = 0;
    PresentationEvents_Start(deliver);
    ready_ = true;
    if (!capture(false) || history_[0].state.capacity() > budget / (window + 1)) {
        stop(); return false;
    }
    return true;
}
bool SimulationReconciliation::start_live_control(uint32_t generation, uint8_t players, uint32_t epoch, size_t budget)
{
    stop();
    const uint32_t tick = SimulationState_Tick();
    if (!Netplay_ControlAnchorReady(tick) ||
        Replay_IsActive() || PresentationEvents_Enabled() || !epoch || players != DAT_00489240 ||
        !inputs_.reset(generation, players, 0, tick)) return false;
    generation_ = generation; players_ = players; epoch_ = epoch; base_tick_ = tick;
    window_ = 1; budget_ = budget; mode_ = DirectInputs; control_only_ = true;
    memset(base_actions_, 0, sizeof(base_actions_));
    corrections_ = truncated_ = 0; replayed_ = 0; peak_ = 0;
    ready_ = true;
    if (!capture(false)) { stop(); return false; }
    return true;
}
bool SimulationReconciliation::aligned() const
{
    return healthy() && SimulationState_Tick() == inputs_.simulated_tick() &&
        (control_only_ ? !PresentationEvents_Enabled() :
         (PresentationEvents_Enabled() && PresentationEvents_Healthy() &&
          PresentationEvents_ConfirmedTick() <= inputs_.confirmed_tick()));
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
    if (!control_only_) PresentationEvents_Capture(&slot.local);
    slot.events = tick && !control_only_ ? PresentationEvents_Digest(tick) : 0;
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
    uint32_t tick, uint8_t player, uint8_t actions, uint32_t epoch)
{
    if (control_only_ || !aligned() || mode_ != DirectInputs) { fail(); return InputTimeline::Invalid; }
    if (generation != generation_ || epoch != epoch_) return InputTimeline::WrongGeneration;
    if (replaying()) {
        if (generation != generation_) return InputTimeline::WrongGeneration;
        if (!tick || player >= players_ || (actions & 0x80)) {
            fail(); return InputTimeline::Invalid;
        }
        if (tick > replay_end_ && tick - replay_end_ >= InputTimeline::Capacity) {
            fail(); return InputTimeline::TooFar;
        }
        for (uint32_t i = 0; i < queued_; ++i)
            if (!pending_[i].revision && pending_[i].tick == tick && pending_[i].player == player) {
                if (pending_[i].actions == actions) return InputTimeline::Duplicate;
                fail(); return InputTimeline::Conflict;
            }
        if (queued_ == MaximumQueuedInputs) {
            fail(); return InputTimeline::TooFar;
        }
        PendingInput input; input.tick = tick; input.player = player; input.actions = actions;
        pending_[queued_++] = input;
        return InputTimeline::Deferred;
    }
    const InputTimeline::Result result = inputs_.submit(generation, tick, player, actions);
    if (result == InputTimeline::Invalid || result == InputTimeline::Conflict || result == InputTimeline::TooFar)
        fail();
    return result;
}
InputTimeline::Result SimulationReconciliation::revise(const AuthoritativeInput &in)
{
    if (!aligned() || mode_ != HostRevisions) { fail(); return InputTimeline::Invalid; }
    if (in.generation != generation_ || in.epoch != epoch_) return InputTimeline::WrongGeneration;
    if (!AuthorityInput_Valid(in) || in.players != players_) { fail(); return InputTimeline::Invalid; }
    if (replaying()) {
        if (in.tick > replay_end_ && in.tick - replay_end_ >= InputTimeline::Capacity) {
            fail(); return InputTimeline::TooFar;
        }
        for (uint32_t i = 0; i < queued_; ++i) {
            PendingInput &queued = pending_[i];
            if (!queued.revision || queued.tick != in.tick) continue;
            const AuthoritativeInput &old = queued.authority;
            if (in.revision < old.revision) return InputTimeline::StaleRevision;
            if (in.revision == old.revision) {
                if (AuthorityInput_Equal(old, in)) return InputTimeline::Duplicate;
                fail(); return InputTimeline::Conflict;
            }
            if (old.sealed && (!in.sealed || memcmp(old.actions, in.actions, players_) != 0)) {
                fail(); return InputTimeline::Conflict;
            }
            queued.authority = in; return InputTimeline::Deferred;
        }
        if (queued_ == MaximumQueuedInputs) { fail(); return InputTimeline::TooFar; }
        PendingInput queued; queued.tick = in.tick; queued.revision = true; queued.authority = in;
        pending_[queued_++] = queued;
        return InputTimeline::Deferred;
    }
    const InputTimeline::Result result = inputs_.revise(in);
    if (result == InputTimeline::Invalid || result == InputTimeline::Conflict || result == InputTimeline::TooFar)
        fail();
    return result;
}
bool SimulationReconciliation::canonical_input(uint32_t tick, uint8_t *actions, bool *sealed) const
{
    return aligned() && mode_ == DirectInputs && !replaying() && !inputs_.dirty_from() &&
        inputs_.used(tick, actions, sealed);
}
bool SimulationReconciliation::world_matches_checkpoint() const
{
    const Checkpoint *current = checkpoint(inputs_.simulated_tick());
    std::vector<uint8_t> state;
    if (!current || !SimulationState_Capture(&state)) return false;
    if (state != current->state) {
        unsigned count = 0;
        for (size_t i = 0; i < std::min(state.size(), current->state.size()) && count < 8; ++i)
            if (state[i] != current->state[i]) {
                LOG("[RECONCILE] external mutation tick=%u offset=%llu expected=%u actual=%u\n", inputs_.simulated_tick(),
                    static_cast<unsigned long long>(i), unsigned(current->state[i]), unsigned(state[i])); ++count;
            }
        return false;
    }
    return true;
}
bool SimulationReconciliation::export_confirmed(uint32_t new_epoch, ConfirmedState *out) const
{
    if (!out || !aligned() || mode_ != DirectInputs || replaying() || inputs_.dirty_from() ||
        new_epoch <= epoch_ || !world_matches_checkpoint()) return false;
    const uint32_t tick = control_only_ ? inputs_.confirmed_tick() : PresentationEvents_ConfirmedTick();
    const Checkpoint *base = checkpoint(tick);
    if (!base || tick != inputs_.confirmed_tick()) return false;
    ConfirmedState value;
    value.generation = generation_; value.epoch = new_epoch; value.tick = tick;
    value.players = static_cast<uint8_t>(players_);
    bool sealed = false;
    if (tick == base_tick_) memcpy(value.actions, base_actions_, players_);
    else if (!inputs_.used(tick, value.actions, &sealed) || !sealed) return false;
    value.state = base->state;
    value.hash = AuthorityState_Hash(value.state.data(), value.state.size());
    if (!AuthorityState_Valid(value)) return false;
    *out = std::move(value); return true;
}
bool SimulationReconciliation::export_controlled(uint32_t new_epoch, Control control, ConfirmedState *out) const
{
    if (!out || !can_finalize()) return false;
    if ((control == OpenPauseMenu && g_SubState != GAMEPLAY_ACTIVE) ||
        (control == ResumeMatch && g_SubState != GAMEPLAY_PAUSED && g_SubState != GAMEPLAY_EXIT_MENU &&
         g_SubState != GAMEPLAY_LEVEL_PREVIEW) ||
        ((control == SkipLevel || control == EndMatch) && g_SubState != GAMEPLAY_EXIT_MENU &&
         g_SubState != GAMEPLAY_LEVEL_PREVIEW) || control < OpenPauseMenu || control > EndMatch) return false;
    ConfirmedState candidate;
    if (!export_confirmed(new_epoch, &candidate)) return false;
    // Snapshot capture is game-thread-only and has no presentation side effects.
    // Restore just these fields even if capture/allocation fails; do not restore
    // an entire world (which would disturb local audio/particle ownership).
    struct ControlScope {
        GameplaySubState substate;
        decltype(DAT_0048693c) level;
        decltype(DAT_004892a5) countdown;
        ControlScope() : substate(g_SubState), level(DAT_0048693c), countdown(DAT_004892a5) {}
        ~ControlScope() { g_SubState = substate; DAT_0048693c = level; DAT_004892a5 = countdown; }
    } restore;
    switch (control) {
    case OpenPauseMenu: g_SubState = GAMEPLAY_EXIT_MENU; break;
    case ResumeMatch: g_SubState = GAMEPLAY_ACTIVE; break;
    case EndMatch:
        *reinterpret_cast<unsigned char *>(&DAT_0048693c) = g_GameConfig.values.active_level_count;
        // Fall through: exit and skip use the original level-advance path.
    case SkipLevel: g_SubState = GAMEPLAY_LEVEL_ADVANCE; DAT_004892a5 = 0; break;
    default: return false;
    }
    if (!SimulationState_Capture(&candidate.state)) return false;
    candidate.hash = AuthorityState_Hash(candidate.state.data(), candidate.state.size());
    if (!AuthorityState_Valid(candidate)) return false;
    *out = std::move(candidate); return true;
}
bool SimulationReconciliation::apply_confirmed(const ConfirmedState &state)
{
    if (!aligned() || state.generation != generation_ || state.epoch <= epoch_ || state.players != players_ ||
        state.tick < (control_only_ ? inputs_.confirmed_tick() : PresentationEvents_ConfirmedTick()) ||
        (control_only_ && state.tick != inputs_.simulated_tick()) ||
        (state.tick > std::max(replay_end_, inputs_.simulated_tick()) &&
         state.tick - std::max(replay_end_, inputs_.simulated_tick()) >= InputTimeline::Capacity) ||
        !AuthorityState_Valid(state) ||
        !SimulationState_Validate(state.state.data(), state.state.size(), state.tick) ||
        !world_matches_checkpoint()) {
        LOG("[RECONCILE] resync rejected epoch=%u incoming=%u tick=%u base=%u committed=%u dirty=%u replay=%u aligned=%d\n",
            epoch_, state.epoch, inputs_.simulated_tick(), state.tick, committed_tick(), inputs_.dirty_from(), replay_end_, int(aligned()));
        return false;
    }
    // Allocate/validate everything possible before touching the running world.
    Checkpoint candidate;
    candidate.tick = state.tick; candidate.state = state.state;
    if (!control_only_) PresentationEvents_Capture(&candidate.local); // Allocate local tag capacity now.
    if (candidate.state.capacity() + candidate.local.particle_tags.capacity() > budget_ / (window_ + 1)) return false;
    InputTimeline replacement;
    if (!replacement.reset(generation_, static_cast<uint8_t>(players_), control_only_ ? 0 : window_, state.tick, state.actions)) return false;
    if (!SimulationState_Restore(state.state.data(), state.state.size(), control_only_)) { fail(); return false; }
    if (!control_only_) {
        PresentationEvents_Start(deliver_, state.tick); // Stop owned loops; never import remote mixer handles.
        PresentationEvents_Capture(&candidate.local);
    }
    candidate.valid = true;
    for (size_t i = 0; i <= window_; ++i) history_[i] = Checkpoint();
    history_[state.tick % (window_ + 1)] = std::move(candidate);
    inputs_ = replacement;
    epoch_ = state.epoch; base_tick_ = state.tick;
    memcpy(base_actions_, state.actions, sizeof(base_actions_));
    queued_ = replay_end_ = debt_ms_ = 0;
    return true;
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
    if (control_only_ || !aligned()) return fail();
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
        const InputTimeline::Result result = input.revision ? inputs_.revise(input.authority) :
            inputs_.submit(generation_, input.tick, input.player, input.actions);
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
    if (control_only_) return can_finalize();
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
