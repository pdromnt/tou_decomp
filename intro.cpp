#include "tou.h"
#include <stdio.h>
#include <windows.h>

// Globals
int DAT_00489248 = 0;
int DAT_00489250 = 0;
int DAT_0048925c = 0;
extern DWORD _DAT_004877f4; // Timer
DWORD DAT_004892b8 = 0;     // Start time

// Intro_Sequence (0045c720)
void Intro_Sequence(void) {
  // Durations from 0045c72b: 3200, 8200, 10640 (ms)
  static const DWORD Durations[] = {3200, 8200, 10640};

  if (DAT_004892b8 == 0) {
    DAT_004892b8 = timeGetTime();
  }

  DWORD CurrentTime = timeGetTime();

  // 1. Check for Skip (DirectInput handled in Game_State_Manager/Input_Update)
  // For now, simple GetAsyncKeyState for Esc/Space/Enter
  if (GetAsyncKeyState(VK_ESCAPE) || GetAsyncKeyState(VK_SPACE) ||
      GetAsyncKeyState(VK_RETURN)) {
    LOG("[INFO] Intro Skipped by User.\n");
    Stop_All_Sounds();
    g_GameState = 0x02; // Transition to Menu
    DAT_0048924c = 0;
    DAT_004892b8 = 0;
    return;
  }

  // 2. Sequence Handling
  // 2. Sequence Handling based on Timings
  DWORD Elapsed = CurrentTime - DAT_004892b8;

  // Default to Frame 0
  DAT_004877c8 = 0;

  if (Elapsed < Durations[0]) {
    // 0 - 3200ms: Frame 0 (Background)
    DAT_004877c8 = 0;
    DAT_0048924c = 0;
  } else if (Elapsed < Durations[1]) {
    // 3200ms - 8200ms: Frame 1 (Studio Title)
    DAT_004877c8 = 1;
    DAT_0048924c = 1;

    // Play sound effect at start of Frame 1?
    // Decompilation hinted at logic here, but for now we follow visuals.
  } else if (Elapsed < Durations[2]) {
    // 8200ms - 10640ms: Frame 2 (Explosion)
    DAT_004877c8 = 2;
    DAT_0048924c = 2;
  } else {
    // Intro Done
    LOG("[INFO] Intro Finished (Time: %d).\n", Elapsed);
    DAT_0048924c = 3; // Signal done

    Stop_All_Sounds();
    g_GameState = 0x02; // Transition to Menu
    DAT_004892b8 = 0;   // Reset Timer
    return;
  }

  // Render the current frame
  Render_Frame();
}
