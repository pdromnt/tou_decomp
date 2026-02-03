#include "tou.h"

// Handle Menu State (004611d0)
// Stub for Load_Level_Resources (0041a010)
// int Load_Level_Resources(void);

// Handle Menu State (004611d0)
extern int DAT_004877a0; // Game State?
extern int DAT_00489238; // Width?
extern int DAT_0048923c; // Height?

// Handle Menu State (004611d0 / Logic at 0045d94c)
void Handle_Menu_State(void) {
  LOG("[INFO] Entering Main Menu State.\n");

  // 1. Load Menu Background (Index 0 -> menu3d.jpg)
  // Deduced from Intro using Index 1 (sanim2.jpg)
  Load_Background_To_Buffer(0);

  // 2. Init Sound/Graphics (FUN_0040e130)
  FUN_0040e130();

  // 3. Init Fonts (One time, safe to call multiple)
  Load_Fonts();

  // 3. Menu Loop
  // The original loop at 0045d94c likely re-initializes DD if focus lost,
  // but for our decomp, we assume Init_DirectDraw in Game_State_Manager held.

  bool inMenu = true;
  while (inMenu && g_GameState == 0x02) {
    // Reload Background to clear previous frame (if acting as canvas)
    // Or just Draw on top? Introduction loaded it once.
    // If we draw text into Buffer, we need to refresh background if text
    // moves/changes. Optimally: Load Background -> Buffer. Then Loop: Copy
    // Buffer -> Backbuffer. But Render_Frame copies Buffer -> Backbuffer. So we
    // must Draw Text INTO Buffer. If we do that, the text becomes permanent
    // unless we reload background. So: Reload BG -> Draw Text -> Render Frame.
    // This is slow for software rendering every frame?
    // Original Load_Background (0042d710) is fast?
    // Let's reload it every frame for now or just ONCE if text is static?
    // Menu text IS static usually.
    // BUT selection highlight changes.
    // So we probably need to redraw.

    // For now, load once, draw once.

    // REDRAW BG (Slow but correct for menu updates)
    Load_Background_To_Buffer(0);

    // DRAW TEXT
    // Draw_Text_To_Buffer("START GAME", 3, 0, DAT_004877c0 + 200*640 + 250,
    // 640, 0, 0, 0); 200*640 = Y offset.
    Draw_Text_To_Buffer("START GAME", 3, 0, DAT_004877c0 + (200 * 640) + 250,
                        640, 0, 0, 0);
    Draw_Text_To_Buffer("EXIT", 3, 0, DAT_004877c0 + (300 * 640) + 280, 640, 0,
                        0, 0);

    // A. Render
    Render_Frame();
    /*
    if (!Render_Frame()) {
      // Handle lost surface or error?
      // Logic from 0045d94c calls Init_DirectDraw recursively on failure?
      // For now, break to avoid hang.
      LOG("[ERROR] Render_Frame failed in Menu.\n");
      break;
    }
    */

    // B. Input Processing (Simple version)
    // DirectInput is updated in Game Loop?
    // We should probably call Input_Update() if we had it exposed.
    // For now, using GetAsyncKeyState for robust menu navigation.

    // Start Game -> 0x98
    if (GetAsyncKeyState(VK_RETURN) & 0x8000) {
      LOG("[INFO] Menu: Start Game Selected.\n");
      g_GameState = 0x98;
      inMenu = false;
    }

    // Exit -> 0xFE
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
      LOG("[INFO] Menu: Exit Selected.\n");
      g_GameState = 0xFE;
      inMenu = false;
    }

    // Process Windows Messages to prevent 'Not Responding'
    // This is technically handled in WinMain, but if Handle_Menu_State blocks,
    // we need to peek messages.
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        g_GameState = 0xFE;
        inMenu = false;
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    Sleep(16); // ~60 FPS Cap
  }
}
