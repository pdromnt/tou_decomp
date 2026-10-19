#ifndef TOU_INPUT_AUTHORITY_H
#define TOU_INPUT_AUTHORITY_H
#include <stdint.h>
#include <stddef.h>
#include <vector>

// Experimental records, not yet part of the live LAN protocol.
struct AuthoritativeInput {
    uint32_t generation = 0, epoch = 0, tick = 0, revision = 0;
    uint8_t players = 0;
    bool sealed = false;
    uint8_t actions[4] = {};
};
bool AuthorityInput_Valid(const AuthoritativeInput &input);
bool AuthorityInput_Equal(const AuthoritativeInput &a, const AuthoritativeInput &b);
bool AuthorityInput_Encode(const AuthoritativeInput &input, std::vector<uint8_t> *bytes);
bool AuthorityInput_Decode(const uint8_t *bytes, size_t size, AuthoritativeInput *input);

class InputAuthorityPublisher {
public:
    enum Result { Changed, Unchanged, Conflict, TooOld, TooFar, Invalid };
    bool reset(uint32_t generation, uint32_t epoch, uint8_t players, uint32_t base = 0);
    Result publish(uint32_t tick, const uint8_t *actions, bool sealed, AuthoritativeInput *output);
private:
    AuthoritativeInput frames_[512];
    uint32_t generation_ = 0, epoch_ = 0, base_ = 0, confirmed_ = 0;
    uint8_t players_ = 0;
};

struct ConfirmedState {
    uint32_t generation = 0, epoch = 0, tick = 0;
    uint8_t players = 0, actions[4] = {};
    std::vector<uint8_t> state;
    uint64_t hash = 0;
};
uint64_t AuthorityState_Hash(const uint8_t *bytes, size_t size);
bool AuthorityState_Valid(const ConfirmedState &state);
bool AuthorityState_Encode(const ConfirmedState &state, std::vector<uint8_t> *bytes);
bool AuthorityState_Decode(const uint8_t *bytes, size_t size, ConfirmedState *state);
#endif
