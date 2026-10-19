#include "input_authority.h"
#include <string.h>
#include <utility>

namespace {
const size_t kMaximumState = 64u * 1024u * 1024u;
void Put32(std::vector<uint8_t> *out, uint32_t n) {
    for (int i = 0; i < 4; ++i) out->push_back(static_cast<uint8_t>(n >> (i * 8)));
}
uint32_t Get32(const uint8_t *in) {
    return uint32_t(in[0]) | (uint32_t(in[1]) << 8) | (uint32_t(in[2]) << 16) | (uint32_t(in[3]) << 24);
}
bool ActionsValid(uint8_t players, const uint8_t *actions) {
    if (!players || players > 4) return false;
    for (uint8_t p = 0; p < 4; ++p)
        if ((actions[p] & 0x80) || (p >= players && actions[p])) return false;
    return true;
}
}
bool AuthorityInput_Valid(const AuthoritativeInput &in) {
    return in.generation && in.epoch && in.tick && in.revision && ActionsValid(in.players, in.actions);
}
bool AuthorityInput_Equal(const AuthoritativeInput &a, const AuthoritativeInput &b) {
    return a.generation == b.generation && a.epoch == b.epoch && a.tick == b.tick &&
        a.revision == b.revision && a.players == b.players && a.sealed == b.sealed &&
        memcmp(a.actions, b.actions, 4) == 0;
}
bool AuthorityInput_Encode(const AuthoritativeInput &in, std::vector<uint8_t> *out) {
    if (!out || !AuthorityInput_Valid(in)) return false;
    out->clear(); Put32(out, in.generation); Put32(out, in.epoch); Put32(out, in.tick); Put32(out, in.revision);
    out->push_back(in.players); out->push_back(in.sealed ? 1 : 0);
    out->insert(out->end(), in.actions, in.actions + 4); return true;
}
bool AuthorityInput_Decode(const uint8_t *in, size_t size, AuthoritativeInput *out) {
    if (!in || !out || size != 22 || in[17] > 1) return false;
    AuthoritativeInput value;
    value.generation = Get32(in); value.epoch = Get32(in + 4); value.tick = Get32(in + 8);
    value.revision = Get32(in + 12); value.players = in[16]; value.sealed = in[17] != 0;
    memcpy(value.actions, in + 18, 4);
    if (!AuthorityInput_Valid(value)) return false;
    *out = value; return true;
}
bool InputAuthorityPublisher::reset(uint32_t gen, uint32_t epoch, uint8_t players, uint32_t base) {
    players_ = 0;
    if (!gen || !epoch || !players || players > 4 || base > UINT32_MAX - 512) return false;
    generation_ = gen; epoch_ = epoch; players_ = players; base_ = confirmed_ = base;
    for (size_t i = 0; i < 512; ++i) frames_[i] = AuthoritativeInput();
    return true;
}
InputAuthorityPublisher::Result InputAuthorityPublisher::publish(uint32_t tick,
    const uint8_t *actions, bool sealed, AuthoritativeInput *out) {
    if (!players_ || !actions || !out || !tick) return Invalid;
    for (uint8_t p = 0; p < players_; ++p) if (actions[p] & 0x80) return Invalid;
    if (tick <= base_) return TooOld;
    if (tick > confirmed_ && tick - confirmed_ >= 512) return TooFar;
    AuthoritativeInput &slot = frames_[tick % 512];
    if (slot.tick != tick) {
        if (tick <= confirmed_) return TooOld;
        if (slot.tick > confirmed_) return TooFar;
        slot = AuthoritativeInput(); slot.generation = generation_; slot.epoch = epoch_;
        slot.tick = tick; slot.players = players_;
    } else {
        const bool same = memcmp(slot.actions, actions, players_) == 0;
        if (slot.sealed && (!sealed || !same)) return Conflict;
        if (slot.sealed == sealed && same) { *out = slot; return Unchanged; }
        if (slot.revision == UINT32_MAX) return Invalid;
    }
    ++slot.revision; slot.sealed = sealed; memcpy(slot.actions, actions, players_);
    while (confirmed_ != UINT32_MAX) {
        const AuthoritativeInput &next = frames_[(confirmed_ + 1) % 512];
        if (next.tick != confirmed_ + 1 || !next.sealed) break;
        ++confirmed_;
    }
    *out = slot; return Changed;
}
uint64_t AuthorityState_Hash(const uint8_t *in, size_t size) {
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < size; ++i) { hash ^= in[i]; hash *= 1099511628211ull; }
    return hash;
}
bool AuthorityState_Valid(const ConfirmedState &in) {
    return in.generation && in.epoch && in.tick <= UINT32_MAX - 512 &&
        ActionsValid(in.players, in.actions) && !in.state.empty() && in.state.size() <= kMaximumState &&
        in.hash == AuthorityState_Hash(in.state.data(), in.state.size());
}
bool AuthorityState_Encode(const ConfirmedState &in, std::vector<uint8_t> *out) {
    if (!out || out == &in.state || !AuthorityState_Valid(in)) return false;
    out->clear(); Put32(out, 0x53554f54); Put32(out, 1);
    Put32(out, in.generation); Put32(out, in.epoch); Put32(out, in.tick);
    out->push_back(in.players); out->insert(out->end(), in.actions, in.actions + 4);
    Put32(out, static_cast<uint32_t>(in.state.size()));
    Put32(out, static_cast<uint32_t>(in.hash)); Put32(out, static_cast<uint32_t>(in.hash >> 32));
    out->insert(out->end(), in.state.begin(), in.state.end()); return true;
}
bool AuthorityState_Decode(const uint8_t *in, size_t size, ConfirmedState *out) {
    if (!in || !out || size < 37 || Get32(in) != 0x53554f54 || Get32(in + 4) != 1) return false;
    const uint32_t bytes = Get32(in + 25);
    if (!bytes || bytes > kMaximumState || size - 37 != bytes) return false;
    ConfirmedState value;
    value.generation = Get32(in + 8); value.epoch = Get32(in + 12); value.tick = Get32(in + 16);
    value.players = in[20]; memcpy(value.actions, in + 21, 4);
    value.hash = uint64_t(Get32(in + 29)) | (uint64_t(Get32(in + 33)) << 32);
    if (!value.generation || !value.epoch || value.tick > UINT32_MAX - 512 || !ActionsValid(value.players, value.actions) ||
        value.hash != AuthorityState_Hash(in + 37, bytes)) return false;
    value.state.assign(in + 37, in + size);
    if (!AuthorityState_Valid(value)) return false;
    *out = std::move(value); return true;
}
