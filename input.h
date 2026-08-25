#ifndef TOU_INPUT_H
#define TOU_INPUT_H

#include <stdint.h>

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

/* Stable simulation-facing actions. Their bit values intentionally match the
 * original PlayerData::buttons field; physical keys remain an SDL/config
 * concern and future replay/network frames only carry these action bits. */
enum GameAction : uint8_t {
    GAME_ACTION_TURN_LEFT      = 0x01,
    GAME_ACTION_TURN_RIGHT     = 0x02,
    GAME_ACTION_THRUST         = 0x04,
    GAME_ACTION_FIRE_PRIMARY   = 0x08,
    GAME_ACTION_FIRE_SECONDARY = 0x10,
    GAME_ACTION_DETONATE       = 0x20,
    GAME_ACTION_BRAKE          = 0x40
};

enum { GAME_ACTION_BINDING_COUNT = 7 };

struct PlayerCommandFrame {
    uint32_t tick;
    uint16_t sequence;
    uint8_t player_slot;
    uint8_t actions;
};

/* Pure translation used by local input, replay, and network validation.
 * keyboard_state uses the legacy 256-entry scan-code namespace and bindings
 * are ordered left/right/thrust/primary/secondary/detonate/brake. */
uint8_t Input_EncodeGameActions(const unsigned char keyboard_state[256],
                                const unsigned char bindings[GAME_ACTION_BINDING_COUNT]);
bool Input_IsValidGameActions(uint8_t actions);

/* ===== Configurable Key Scan Codes (from config blob) ===== */

#endif /* TOU_INPUT_H */
