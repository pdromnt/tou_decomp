#include "tou.h"
#include <mmsystem.h>
#include <windows.h>

// Globals
int DAT_0048924c = 0;   // Splash Screen Index/Counter
DWORD DAT_004892b8 = 0; // Time tracker

// Intro Sequence (0045c720)
// Handles Splash Screens and Intro logic
void Intro_Sequence(void) {
  // Intro Durations (ms)
  static DWORD StartTime = 0;
  // 0xc80 = 3200, 0x2008 = 8200, 0x2990 = 10640
  static const DWORD Durations[] = {3200, 8200, 10640};

  if (StartTime == 0) {
    StartTime = timeGetTime();
  }

  DWORD CurrentTime = timeGetTime();

  // Check for Input Skip (Stub - Spacebar/Esc)
  if (GetAsyncKeyState(VK_ESCAPE) || GetAsyncKeyState(VK_SPACE) ||
      GetAsyncKeyState(VK_RETURN)) {
    g_GameState = 0x98; // Skip to Game/Menu (Should go to 0x02 actually? Menu)
    // Original code goes to 0x98 likely for "New Game" demo or similar?
    // Let's go to Menu (State 2) conceptually, but code uses 0x98 probably.
    // Wait, Menu calls Load_Level_Resources.

    StartTime = 0;
    DAT_0048924c = 0;
    return;
  }

  if (DAT_0048924c < 3) {
    if (CurrentTime > StartTime + Durations[DAT_0048924c]) {
      DAT_0048924c++;
      // Load Next Image?
      // Play Sound?
    } else {
      // Render Current Splash (Stub)
      // Draw_Splash(DAT_0048924c);
    }
  } else {
    // Intro Done
    g_GameState = 0x98; // Transition to Game/Menu Init
    StartTime = 0;
    DAT_0048924c = 0;
  }
}
