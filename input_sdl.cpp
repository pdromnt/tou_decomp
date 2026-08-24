/* SDL input adapter preserving TOU's legacy saved scan-code namespace. */
#include "tou.h"
#include "input.h"
#include "platform.h"
#include <SDL3/SDL.h>
#include <string.h>

static SDL_Scancode LegacyScanCodeToSdl(unsigned int code)
{
    switch (code) {
    case 0x01: return SDL_SCANCODE_ESCAPE;
    case 0x02: return SDL_SCANCODE_1;
    case 0x03: return SDL_SCANCODE_2;
    case 0x04: return SDL_SCANCODE_3;
    case 0x05: return SDL_SCANCODE_4;
    case 0x06: return SDL_SCANCODE_5;
    case 0x07: return SDL_SCANCODE_6;
    case 0x08: return SDL_SCANCODE_7;
    case 0x09: return SDL_SCANCODE_8;
    case 0x0A: return SDL_SCANCODE_9;
    case 0x0B: return SDL_SCANCODE_0;
    case 0x0C: return SDL_SCANCODE_MINUS;
    case 0x0D: return SDL_SCANCODE_EQUALS;
    case 0x0E: return SDL_SCANCODE_BACKSPACE;
    case 0x0F: return SDL_SCANCODE_TAB;
    case 0x10: return SDL_SCANCODE_Q;
    case 0x11: return SDL_SCANCODE_W;
    case 0x12: return SDL_SCANCODE_E;
    case 0x13: return SDL_SCANCODE_R;
    case 0x14: return SDL_SCANCODE_T;
    case 0x15: return SDL_SCANCODE_Y;
    case 0x16: return SDL_SCANCODE_U;
    case 0x17: return SDL_SCANCODE_I;
    case 0x18: return SDL_SCANCODE_O;
    case 0x19: return SDL_SCANCODE_P;
    case 0x1A: return SDL_SCANCODE_LEFTBRACKET;
    case 0x1B: return SDL_SCANCODE_RIGHTBRACKET;
    case 0x1C: return SDL_SCANCODE_RETURN;
    case 0x1D: return SDL_SCANCODE_LCTRL;
    case 0x1E: return SDL_SCANCODE_A;
    case 0x1F: return SDL_SCANCODE_S;
    case 0x20: return SDL_SCANCODE_D;
    case 0x21: return SDL_SCANCODE_F;
    case 0x22: return SDL_SCANCODE_G;
    case 0x23: return SDL_SCANCODE_H;
    case 0x24: return SDL_SCANCODE_J;
    case 0x25: return SDL_SCANCODE_K;
    case 0x26: return SDL_SCANCODE_L;
    case 0x27: return SDL_SCANCODE_SEMICOLON;
    case 0x28: return SDL_SCANCODE_APOSTROPHE;
    case 0x29: return SDL_SCANCODE_GRAVE;
    case 0x2A: return SDL_SCANCODE_LSHIFT;
    case 0x2B: return SDL_SCANCODE_BACKSLASH;
    case 0x2C: return SDL_SCANCODE_Z;
    case 0x2D: return SDL_SCANCODE_X;
    case 0x2E: return SDL_SCANCODE_C;
    case 0x2F: return SDL_SCANCODE_V;
    case 0x30: return SDL_SCANCODE_B;
    case 0x31: return SDL_SCANCODE_N;
    case 0x32: return SDL_SCANCODE_M;
    case 0x33: return SDL_SCANCODE_COMMA;
    case 0x34: return SDL_SCANCODE_PERIOD;
    case 0x35: return SDL_SCANCODE_SLASH;
    case 0x36: return SDL_SCANCODE_RSHIFT;
    case 0x37: return SDL_SCANCODE_KP_MULTIPLY;
    case 0x38: return SDL_SCANCODE_LALT;
    case 0x39: return SDL_SCANCODE_SPACE;
    case 0x3A: return SDL_SCANCODE_CAPSLOCK;
    case 0x3B: return SDL_SCANCODE_F1;
    case 0x3C: return SDL_SCANCODE_F2;
    case 0x3D: return SDL_SCANCODE_F3;
    case 0x3E: return SDL_SCANCODE_F4;
    case 0x3F: return SDL_SCANCODE_F5;
    case 0x40: return SDL_SCANCODE_F6;
    case 0x41: return SDL_SCANCODE_F7;
    case 0x42: return SDL_SCANCODE_F8;
    case 0x43: return SDL_SCANCODE_F9;
    case 0x44: return SDL_SCANCODE_F10;
    case 0x45: return SDL_SCANCODE_NUMLOCKCLEAR;
    case 0x46: return SDL_SCANCODE_SCROLLLOCK;
    case 0x47: return SDL_SCANCODE_KP_7;
    case 0x48: return SDL_SCANCODE_KP_8;
    case 0x49: return SDL_SCANCODE_KP_9;
    case 0x4A: return SDL_SCANCODE_KP_MINUS;
    case 0x4B: return SDL_SCANCODE_KP_4;
    case 0x4C: return SDL_SCANCODE_KP_5;
    case 0x4D: return SDL_SCANCODE_KP_6;
    case 0x4E: return SDL_SCANCODE_KP_PLUS;
    case 0x4F: return SDL_SCANCODE_KP_1;
    case 0x50: return SDL_SCANCODE_KP_2;
    case 0x51: return SDL_SCANCODE_KP_3;
    case 0x52: return SDL_SCANCODE_KP_0;
    case 0x53: return SDL_SCANCODE_KP_PERIOD;
    case 0x56: return SDL_SCANCODE_NONUSBACKSLASH;
    case 0x57: return SDL_SCANCODE_F11;
    case 0x58: return SDL_SCANCODE_F12;
    case 0x9C: return SDL_SCANCODE_KP_ENTER;
    case 0x9D: return SDL_SCANCODE_RCTRL;
    case 0xB3: return SDL_SCANCODE_KP_PERIOD;
    case 0xB5: return SDL_SCANCODE_KP_DIVIDE;
    case 0xB7: return SDL_SCANCODE_PRINTSCREEN;
    case 0xB8: return SDL_SCANCODE_RALT;
    case 0xC5: return SDL_SCANCODE_PAUSE;
    case 0xC7: return SDL_SCANCODE_HOME;
    case 0xC8: return SDL_SCANCODE_UP;
    case 0xC9: return SDL_SCANCODE_PAGEUP;
    case 0xCB: return SDL_SCANCODE_LEFT;
    case 0xCD: return SDL_SCANCODE_RIGHT;
    case 0xCF: return SDL_SCANCODE_END;
    case 0xD0: return SDL_SCANCODE_DOWN;
    case 0xD1: return SDL_SCANCODE_PAGEDOWN;
    case 0xD2: return SDL_SCANCODE_INSERT;
    case 0xD3: return SDL_SCANCODE_DELETE;
    case 0xDB: return SDL_SCANCODE_LGUI;
    case 0xDC: return SDL_SCANCODE_RGUI;
    case 0xDD: return SDL_SCANCODE_APPLICATION;
    default:   return SDL_SCANCODE_UNKNOWN;
    }
}

void Input_UpdateKeyboardState(void)
{
    memset(g_KeyboardState, 0, sizeof(g_KeyboardState));
    if (!g_bIsActive)
        return;

    int key_count = 0;
    const bool *keys = SDL_GetKeyboardState(&key_count);
    if (keys == NULL)
        return;
    for (unsigned int legacy = 0; legacy < 256; legacy++) {
        SDL_Scancode scancode = LegacyScanCodeToSdl(legacy);
        if (scancode != SDL_SCANCODE_UNKNOWN && (int)scancode < key_count && keys[scancode])
            g_KeyboardState[legacy] = 0x80;
    }
}

int Input_GetMouseState(int *x, int *y, unsigned char *buttons)
{
    if (Platform_GetSdlWindow() == NULL)
        return 0;
    float mouse_x;
    float mouse_y;
    SDL_MouseButtonFlags state = SDL_GetMouseState(&mouse_x, &mouse_y);
    if (x != NULL) *x = (int)mouse_x;
    if (y != NULL) *y = (int)mouse_y;
    if (buttons != NULL) {
        *buttons = 0;
        if (state & SDL_BUTTON_LMASK) *buttons |= 1;
        if (state & SDL_BUTTON_RMASK) *buttons |= 2;
    }
    return SDL_GetMouseFocus() == (SDL_Window *)Platform_GetSdlWindow();
}
