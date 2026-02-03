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
  if (DAT_0048924c < 3) {
    if (CurrentTime > DAT_004892b8 + Durations[DAT_0048924c]) {
      LOG("[INFO] Intro Step %d Finished.\n", DAT_0048924c);
      DAT_0048924c++;
      LOG("[INFO] Intro: Loading Background %d\n", DAT_0048924c + 1);
      Load_Background_To_Buffer(DAT_0048924c + 1); // 1->2, 2->3

      if (DAT_0048924c == 1) {
        // Transition at 3.2s
        // Stop_All_Sounds(); // Maybe not stop music?
      }
    }
  } else {
    // Intro Done
    LOG("[INFO] Intro Finished.\n");
    g_GameState = 0x02; // To Menu
    DAT_0048924c = 0;
    DAT_004892b8 = 0;
  }
}
