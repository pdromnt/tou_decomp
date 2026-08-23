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
extern char                  g_InputMode;       /* 004877E4 */
extern unsigned char         DAT_004877e6;      /* 004877E6 - input mode item index */
extern int                   DAT_004877e8;
extern char                  g_DirectInputMouseXSeen;

/* ===== Configurable Key Scan Codes (from config blob) ===== */
extern unsigned char         DAT_004837ba;       /* Pause key scan code */
extern unsigned char         DAT_004837bb;       /* Camera cycle key scan code */

#endif /* TOU_INPUT_H */
