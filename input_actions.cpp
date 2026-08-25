#include "input.h"

#include <stddef.h>

namespace {

const uint8_t kActionBits[GAME_ACTION_BINDING_COUNT] = {
    GAME_ACTION_TURN_LEFT,
    GAME_ACTION_TURN_RIGHT,
    GAME_ACTION_THRUST,
    GAME_ACTION_FIRE_PRIMARY,
    GAME_ACTION_FIRE_SECONDARY,
    GAME_ACTION_DETONATE,
    GAME_ACTION_BRAKE
};

const uint8_t kAllGameActions =
    GAME_ACTION_TURN_LEFT |
    GAME_ACTION_TURN_RIGHT |
    GAME_ACTION_THRUST |
    GAME_ACTION_FIRE_PRIMARY |
    GAME_ACTION_FIRE_SECONDARY |
    GAME_ACTION_DETONATE |
    GAME_ACTION_BRAKE;

} // namespace

uint8_t Input_EncodeGameActions(const unsigned char keyboard_state[256],
                                const unsigned char bindings[GAME_ACTION_BINDING_COUNT])
{
    uint8_t actions = 0;
    if (keyboard_state == NULL || bindings == NULL)
        return actions;

    for (size_t i = 0; i < GAME_ACTION_BINDING_COUNT; ++i) {
        if ((keyboard_state[bindings[i]] & 0x80u) != 0)
            actions = static_cast<uint8_t>(actions | kActionBits[i]);
    }
    return actions;
}

bool Input_IsValidGameActions(uint8_t actions)
{
    return (actions & static_cast<uint8_t>(~kAllGameActions)) == 0;
}
