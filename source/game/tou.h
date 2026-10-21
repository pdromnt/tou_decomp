#ifndef TOU_H
#define TOU_H

#include <stdlib.h>
// libstdc++'s algorithm header declares random_shuffle using std::rand.
// Parse it before the gameplay rand/srand macros below, including when a
// translation unit includes <algorithm> after this umbrella header.
#include <algorithm>

#include "fixed_point.h"
#include "terrain_properties.h"
#include "sprite_atlas.h"
#include "platform.h"
#include "audio_backend.h"
#include "original_semantics.h"
#include "entity_callbacks.h"

#include "types.h"
#include "config.h"
#include "entity.h"
#include "gfx.h"
#include "input.h"
#include "gamepad.h"
#include "sound.h"
#include "level.h"
#include "gamestate.h"
#include "localization.h"
#include "simulation_state.h"
#include "simulation_tick.h"
#include "netplay.h"
#include "replay.h"

/* Use the original executable's embedded MSVC6 RNG instead of host CRT state. */
#define rand  TOU_Rand
#define srand TOU_Srand

#endif /* TOU_H */
