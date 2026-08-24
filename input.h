#ifndef TOU_INPUT_H
#define TOU_INPUT_H

/* ===== Mouse Input (gameloop.cpp) ===== */
extern int                   g_MouseDeltaX;     /* 004877B4 */
extern int                   g_MouseDeltaY;     /* 004877B8 */
extern int                   g_SpectatorCameraX;/* zero-human free camera center */
extern int                   g_SpectatorCameraY;
extern char                  g_InputMode;       /* 004877E4 */
extern unsigned char         DAT_004877e6;      /* 004877E6 - input mode item index */
extern int                   DAT_004877e8;

/* SDL input adapter. It intentionally emits the legacy saved scan-code
 * layout so saved bindings and recovered gameplay code remain unchanged. */
void Input_UpdateKeyboardState(void);
int  Input_GetMouseState(int *x, int *y, unsigned char *buttons);

/* ===== Configurable Key Scan Codes (from config blob) ===== */

#endif /* TOU_INPUT_H */
