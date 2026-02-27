/*
 * stubs.cpp - Placeholder for miscellaneous undecompiled functions
 */
#include "tou.h"

/* ===== FUN_0045d7d0 - Intro particle/entity reset ===== */
void FUN_0045d7d0(void)
{
    g_FrameIndex = 2;              /* Intro starts showing frame 2 */
    DAT_00489248 = 0;              /* Entity count */
    DAT_00489250 = 0;              /* Particle count */
    DAT_0048925c = 0;              /* Misc counter */
    g_FrameTimer = timeGetTime();  /* Frame time reference */
    DAT_004892b8 = timeGetTime();  /* Intro start timestamp for duration checks */
}
