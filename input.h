#ifndef TOU_INPUT_H
#define TOU_INPUT_H

#include "compat.h"

/* ===== DirectInput Globals (init.cpp) ===== */
extern LPDIRECTINPUT         lpDI;              /* 00489ED4 */
extern LPDIRECTINPUTDEVICE   lpDI_Keyboard;     /* 00489EE4 */
extern LPDIRECTINPUTDEVICE   lpDI_Mouse;        /* 00489EC0 */
extern HANDLE                hMouseEvent;       /* 00489EE0 */

/* ===== Mouse Input (gameloop.cpp) ===== */
extern int                   g_MouseDeltaX;     /* 004877B4 */
extern int                   g_MouseDeltaY;     /* 004877B8 */
extern int                   g_SpectatorCameraX;/* zero-human free camera center */
extern int                   g_SpectatorCameraY;
extern char                  g_InputMode;       /* 004877E4 */
extern unsigned char         DAT_004877e6;      /* 004877E6 - input mode item index */
extern int                   DAT_004877e8;
extern char                  g_DirectInputMouseXSeen;

/* SDL input adapter. It intentionally emits the legacy DirectInput scan-code
 * layout so saved bindings and recovered gameplay code remain unchanged. */
#ifdef TOU_HAS_SDL
void Input_UpdateKeyboardState(void);
int  Input_GetMouseState(int *x, int *y, unsigned char *buttons);
#endif

/* ===== Configurable Key Scan Codes (from config blob) ===== */

#endif /* TOU_INPUT_H */
