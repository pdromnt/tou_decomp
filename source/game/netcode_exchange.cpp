#include "tou.h"
#include "netcode_exchange.h"
#include <string.h>
#include <utility>

namespace {
const size_t kMaximumQueue = 64u * 1024u * 1024u + 65536u;
void Put32(std::vector<uint8_t> *out, uint32_t n) {
    for (int i = 0; i < 4; ++i) out->push_back(static_cast<uint8_t>(n >> (i * 8)));
}
void Put64(std::vector<uint8_t> *out, uint64_t n) { Put32(out, uint32_t(n)); Put32(out, uint32_t(n >> 32)); }
uint32_t Get32(const uint8_t *in) {
    return uint32_t(in[0]) | uint32_t(in[1]) << 8 | uint32_t(in[2]) << 16 | uint32_t(in[3]) << 24;
}
uint64_t Get64(const uint8_t *in) { return uint64_t(Get32(in)) | uint64_t(Get32(in + 4)) << 32; }
std::vector<uint8_t> Scope(uint32_t generation, uint32_t epoch) {
    std::vector<uint8_t> out; Put32(&out, generation); Put32(&out, epoch); return out;
}
}
bool NetcodeExchange::valid_size(Type type, size_t size) {
    switch (type) {
    case Receipt: case ReceiptAck: return size == 14;
    case Revision: return size == 22;
    case RevisionAck: return size == 16;
    case Progress: case SyncReady: case SyncCommit: case SyncApplied: case SyncResume:
    case End: case EndAck: return size == 20;
    case SyncOffer: return size >= 38 && size <= 64u * 1024u * 1024u + 37u;
    default: return false;
    }
}
void NetcodeExchange::reset_history(uint32_t base) {
    for (size_t i = 0; i < 64; ++i) { receipts_[i] = ReceiptSlot(); revisions_[i] = RevisionSlot(); }
    peer_confirmed_ = last_progress_ = base;
}
bool NetcodeExchange::start(Role role, uint32_t gen, uint32_t epoch, uint8_t players, uint8_t slot) {
    disconnect();
    if ((role != Host && role != Client) || !gen || !epoch || players < 2 || players > 4 || !slot || slot >= players)
        return false;
    role_ = role; generation_ = gen; epoch_ = epoch; players_ = players; slot_ = slot;
    reset_history(0); peer_hash_ = 0; pending_ = ConfirmedState(); phase_ = Running; return true;
}
void NetcodeExchange::disconnect() {
    phase_ = Failed; outgoing_.clear(); queued_bytes_ = 0; pending_ = ConfirmedState();
}
bool NetcodeExchange::fail() { disconnect(); return false; }
bool NetcodeExchange::queue(Type type, std::vector<uint8_t> bytes) {
    if (!valid_size(type, bytes.size()) || outgoing_.size() >= 128 || bytes.size() > kMaximumQueue - queued_bytes_)
        return false;
    Message message; message.type = type; message.bytes = std::move(bytes);
    queued_bytes_ += message.bytes.size(); outgoing_.push_back(std::move(message)); return true;
}
bool NetcodeExchange::take(Message *message) {
    if (!message || outgoing_.empty() || phase_ == Failed) return false;
    queued_bytes_ -= outgoing_.front().bytes.size(); *message = std::move(outgoing_.front()); outgoing_.pop_front(); return true;
}
bool NetcodeExchange::receipt(uint32_t tick, uint8_t actions) {
    if (role_ != Client || phase_ != Running || !tick || (actions & 0x80)) return false;
    if (tick <= last_progress_ || tick - last_progress_ >= 64) return false;
    ReceiptSlot &slot = receipts_[tick % 64];
    if (slot.valid && slot.tick == tick) return slot.actions == actions;
    if (slot.valid && !slot.acked) return false;
    std::vector<uint8_t> out = Scope(generation_, epoch_); Put32(&out, tick);
    out.push_back(slot_); out.push_back(actions);
    if (!queue(Receipt, std::move(out))) return false;
    slot.tick = tick; slot.actions = actions; slot.valid = true; slot.acked = false; return true;
}
bool NetcodeExchange::revision(const AuthoritativeInput &in, const SimulationReconciliation &world) {
    if (role_ != Host || phase_ != Running || !AuthorityInput_Valid(in) || in.generation != generation_ ||
        in.epoch != epoch_ || in.players != players_ || world.epoch() != epoch_) return false;
    uint8_t used[4] = {}; bool sealed = false;
    if (!world.canonical_input(in.tick, used, &sealed) || sealed != in.sealed || memcmp(used, in.actions, 4)) return false;
    if (in.tick <= peer_confirmed_ || in.tick - peer_confirmed_ >= 64) return false;
    RevisionSlot &slot = revisions_[in.tick % 64];
    if (slot.input.tick == in.tick) {
        if (in.revision < slot.input.revision) return true;
        if (in.revision == slot.input.revision) return AuthorityInput_Equal(slot.input, in);
        if (slot.input.sealed && (!in.sealed || memcmp(slot.input.actions, in.actions, 4))) return false;
    } else if (slot.input.revision && (slot.input.tick > peer_confirmed_ || !slot.acked)) return false;
    uint64_t hash = 0;
    if (in.sealed) {
        const SimulationReconciliation::Checkpoint *checkpoint = world.checkpoint(in.tick);
        if (!checkpoint || in.tick > world.confirmed_tick()) return false;
        hash = AuthorityState_Hash(checkpoint->state.data(), checkpoint->state.size());
    }
    std::vector<uint8_t> out;
    if (!AuthorityInput_Encode(in, &out) || !queue(Revision, std::move(out))) return false;
    slot.input = in; slot.hash = hash; slot.acked = false; return true;
}
bool NetcodeExchange::progress(const SimulationReconciliation &world) {
    if (role_ != Client || phase_ != Running || !world.healthy() || world.replaying() || world.dirty_from() || world.epoch() != epoch_)
        return false;
    const uint32_t tick = PresentationEvents_ConfirmedTick();
    if (tick == last_progress_) return true;
    const SimulationReconciliation::Checkpoint *checkpoint = world.checkpoint(tick);
    if (!checkpoint || tick < last_progress_) return false;
    std::vector<uint8_t> out = Scope(generation_, epoch_); Put32(&out, tick);
    Put64(&out, AuthorityState_Hash(checkpoint->state.data(), checkpoint->state.size()));
    if (!queue(Progress, std::move(out))) return false;
    last_progress_ = tick; return true;
}
bool NetcodeExchange::stamp(Type type) {
    std::vector<uint8_t> out = Scope(pending_.generation, pending_.epoch);
    Put32(&out, pending_.tick); Put64(&out, pending_.hash); return queue(type, std::move(out));
}
bool NetcodeExchange::same_stamp(const std::vector<uint8_t> &in) const {
    return in.size() == 20 && Get32(in.data()) == pending_.generation && Get32(in.data() + 4) == pending_.epoch &&
        Get32(in.data() + 8) == pending_.tick && Get64(in.data() + 12) == pending_.hash;
}
bool NetcodeExchange::begin_sync(const SimulationReconciliation &world, uint32_t epoch, uint32_t now) {
    if (role_ != Host || (phase_ != Running && phase_ != NeedsSync) || world.epoch() != epoch_) return false;
    ConfirmedState state;
    std::vector<uint8_t> bytes;
    if (!world.export_confirmed(epoch, &state) || state.tick < peer_confirmed_ ||
        !AuthorityState_Encode(state, &bytes) || !queue(SyncOffer, std::move(bytes))) return false;
    pending_ = std::move(state); phase_ = AwaitReady; deadline_start_ = now; return true;
}
bool NetcodeExchange::finish(uint32_t tick, uint32_t now) {
    if (role_ != Host || phase_ != Running || tick != peer_confirmed_) return false;
    std::vector<uint8_t> out = Scope(generation_, epoch_); Put32(&out, tick); Put64(&out, peer_hash_);
    if (!queue(End, std::move(out))) return false;
    phase_ = Closing; deadline_start_ = now; return true;
}
bool NetcodeExchange::poll(uint32_t now) {
    if (phase_ == Failed) return false;
    if (phase_ != Running && phase_ != Closed && now - deadline_start_ >= 5000u) return fail();
    return true;
}
bool NetcodeExchange::receive(const Message &message, SimulationReconciliation *world, uint32_t now) {
    if (!world || phase_ == Failed || !valid_size(message.type, message.bytes.size())) return fail();
    const std::vector<uint8_t> &in = message.bytes;
    const uint8_t *data = in.data();
    if (message.type == SyncOffer) {
        if (role_ != Client) return fail();
        ConfirmedState state;
        if (!AuthorityState_Decode(data, in.size(), &state)) return fail();
        if (state.generation != generation_) return true;
        if (pending_.epoch == state.epoch && pending_.hash == state.hash && pending_.tick == state.tick) {
            if (phase_ == Prepared) return stamp(SyncReady) || fail();
            if (phase_ == AwaitResume) return stamp(SyncApplied) || fail();
        }
        if (state.epoch <= epoch_) return true;
        if (phase_ != Running || state.players != players_ || state.tick < PresentationEvents_ConfirmedTick() ||
            !SimulationState_Validate(state.state.data(), state.state.size(), state.tick)) return fail();
        pending_ = std::move(state); phase_ = Prepared; deadline_start_ = now;
        return stamp(SyncReady) || fail();
    }
    const uint32_t gen = Get32(data), epoch = Get32(data + 4);
    if (gen != generation_) return true;
    if (message.type >= SyncReady && message.type <= SyncResume) {
        if (epoch < epoch_ || (pending_.epoch && epoch < pending_.epoch)) return true;
        if (!same_stamp(in)) return fail();
        switch (message.type) {
        case SyncReady:
            if (role_ != Host) return fail();
            if (phase_ == AwaitApplied) return stamp(SyncCommit) || fail();
            if (phase_ == Running && epoch == epoch_) return true;
            if (phase_ != AwaitReady || !world->apply_confirmed(pending_)) return fail();
            epoch_ = epoch; phase_ = AwaitApplied; return stamp(SyncCommit) || fail();
        case SyncCommit:
            if (role_ != Client) return fail();
            if (phase_ == AwaitResume || (phase_ == Running && epoch == epoch_)) return stamp(SyncApplied) || fail();
            if (phase_ != Prepared || !world->apply_confirmed(pending_)) return fail();
            epoch_ = epoch; phase_ = AwaitResume; return stamp(SyncApplied) || fail();
        case SyncApplied:
            if (role_ != Host) return fail();
            if (phase_ == Running && epoch == epoch_) return stamp(SyncResume) || fail();
            if (phase_ != AwaitApplied) return fail();
            if (!stamp(SyncResume)) return fail();
            reset_history(pending_.tick); peer_hash_ = pending_.hash; phase_ = Running; return true;
        case SyncResume:
            if (role_ != Client) return fail();
            if (phase_ == Running && epoch == epoch_) return true;
            if (phase_ != AwaitResume) return fail();
            reset_history(pending_.tick); peer_hash_ = pending_.hash; phase_ = Running; return true;
        default: return fail();
        }
    }
    if (epoch != epoch_) return true;
    if (message.type == End || message.type == EndAck) {
        const uint32_t tick = Get32(data + 8); const uint64_t hash = Get64(data + 12);
        if (message.type == End) {
            if (role_ != Client || (phase_ != Running && phase_ != Closed) || tick != PresentationEvents_ConfirmedTick()) return fail();
            const SimulationReconciliation::Checkpoint *checkpoint = world->checkpoint(tick);
            if (!checkpoint || hash != AuthorityState_Hash(checkpoint->state.data(), checkpoint->state.size())) return fail();
            if (!queue(EndAck, in)) return fail();
            phase_ = Closed; return true;
        }
        if (role_ != Host || (phase_ != Closing && phase_ != Closed) || tick != peer_confirmed_ || hash != peer_hash_) return fail();
        phase_ = Closed; return true;
    }
    if (phase_ != Running) return true; // Old receipts/revisions cannot cross a sync barrier.
    const uint32_t tick = Get32(data + 8);
    switch (message.type) {
    case Receipt: {
        if (role_ != Host || data[12] != slot_ || (data[13] & 0x80)) return fail();
        const InputTimeline::Result result = world->submit(gen, tick, slot_, data[13], epoch);
        if (result == InputTimeline::TooOld) return true;
        if (result != InputTimeline::Accepted && result != InputTimeline::Duplicate && result != InputTimeline::Deferred) return fail();
        return queue(ReceiptAck, in) || fail();
    }
    case ReceiptAck: {
        if (role_ != Client || data[12] != slot_) return fail();
        ReceiptSlot &slot = receipts_[tick % 64];
        if (tick <= last_progress_ && (!slot.valid || slot.tick != tick)) return true;
        if (!slot.valid || slot.tick != tick || slot.actions != data[13]) return fail();
        slot.acked = true; return true;
    }
    case Revision: {
        AuthoritativeInput value;
        if (role_ != Client || !AuthorityInput_Decode(data, in.size(), &value)) return fail();
        const InputTimeline::Result result = world->revise(value);
        if (result == InputTimeline::TooOld) return true;
        if (result != InputTimeline::Accepted && result != InputTimeline::Duplicate &&
            result != InputTimeline::Deferred && result != InputTimeline::StaleRevision) return fail();
        std::vector<uint8_t> out = Scope(gen, epoch); Put32(&out, tick); Put32(&out, value.revision);
        return queue(RevisionAck, std::move(out)) || fail();
    }
    case RevisionAck: {
        if (role_ != Host) return fail();
        RevisionSlot &slot = revisions_[tick % 64];
        if (tick <= peer_confirmed_ && slot.input.tick != tick) return true;
        const uint32_t revision = Get32(data + 12);
        if (!tick || !revision) return fail();
        if (slot.input.tick != tick || revision > slot.input.revision) return fail();
        if (revision == slot.input.revision) slot.acked = true;
        return true;
    }
    case Progress: {
        if (role_ != Host) return fail();
        const uint64_t hash = Get64(data + 12);
        if (tick < peer_confirmed_) return true;
        if (tick == peer_confirmed_) return hash == peer_hash_ || fail();
        const RevisionSlot &slot = revisions_[tick % 64];
        if (slot.input.tick != tick || !slot.input.sealed || tick > world->confirmed_tick()) return fail();
        if (hash != slot.hash) { phase_ = NeedsSync; deadline_start_ = now; return true; }
        peer_confirmed_ = tick; peer_hash_ = hash; return true;
    }
    default: return fail();
    }
}
