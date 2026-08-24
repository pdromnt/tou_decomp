#ifndef TOU_H
#define TOU_H

#include <stdlib.h>

#include "compat.h"
#include "fixed_point.h"
#include "fmod.h"
#include "binary_compat.h"
#include "entity_callbacks.h"

#include "types.h"
#include "config.h"
#include "entity.h"
#include "gfx.h"
#include "input.h"
#include "sound.h"
#include "level.h"
#include "gamestate.h"

/* Use the original executable's embedded MSVC6 RNG instead of host CRT state. */
#define rand  TOU_Rand
#define srand TOU_Srand

#endif /* TOU_H */
