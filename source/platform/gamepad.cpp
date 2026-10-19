#include <algorithm>
#include <climits>
#include <cstdlib>
#include "tou.h"
#include "gamepad.h"
#include "settings.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_hidapi.h>

namespace {
// Reserve a fixed button namespace so adding SDL buttons won't shift saved axes.
constexpr int kButtonCount = 32;
constexpr int kBindingCount = 1 + kButtonCount + 12;
constexpr uint8_t kActions[7] = {1, 2, 4, 8, 16, 32, 64};
struct Pad {
    SDL_Gamepad *handle = nullptr;
    SDL_JoystickID id = 0;
    bool armed = false;
    std::array<bool, kBindingCount> down{};
};
std::array<Pad, 4> s_Pads;
GamepadSettings s_Settings = Gamepad_DefaultSettings();
bool s_Initialized = false;
bool s_InitAttempted = false;
bool s_Dirty = true;
bool s_MenuOwned = false;
int s_Selected = -1;
int s_Page = -1;
int s_MouseX = -1, s_MouseY = -1;
int s_CursorX = 320 << 18, s_CursorY = 240 << 18;
uint32_t s_MenuTick = 0;
uint32_t s_RepeatAt = 0;
int s_Direction = 0;
std::array<bool, kBindingCount> s_Previous{};
bool s_Gameplay = false;

int AxisBinding(SDL_GamepadAxis axis, bool positive)
{
    return 1 + kButtonCount + static_cast<int>(axis) * 2 + (positive ? 1 : 0);
}

int Clamp(int value, int low, int high) { return std::max(low, std::min(value, high)); }

void RefreshDevices()
{
    if (g_LogEnabled) {
        SDL_hid_device_info *hid = SDL_hid_enumerate(0x045e, 0x0b13);
        for (const SDL_hid_device_info *info = hid; info; info = info->next)
            LOG("[GAMEPAD] Xbox BLE HID endpoint: bus=%d usage=%04x:%04x path-prefix=%.8s\n",
                static_cast<int>(info->bus_type), info->usage_page, info->usage, info->path);
        SDL_hid_free_enumeration(hid);
    }
    for (Pad &pad : s_Pads) {
        if (pad.handle && !SDL_GamepadConnected(pad.handle)) {
            SDL_CloseGamepad(pad.handle);
            pad = Pad{};
        }
    }
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    int joystickCount = 0;
    SDL_JoystickID *joysticks = SDL_GetJoysticks(&joystickCount);
    LOG("[GAMEPAD] SDL discovery: %d joystick(s), %d mapped gamepad(s)\n",
        joystickCount, count);
    for (int i = 0; joysticks && i < joystickCount; ++i) {
        char guid[33];
        SDL_GUIDToString(SDL_GetJoystickGUIDForID(joysticks[i]), guid, sizeof(guid));
        LOG("[GAMEPAD] Device %u: %s (mapped=%d)\n",
            static_cast<unsigned int>(joysticks[i]), SDL_GetJoystickNameForID(joysticks[i]),
            SDL_IsGamepad(joysticks[i]) ? 1 : 0);
        LOG("[GAMEPAD] GUID: %s\n", guid);
        if (!SDL_IsGamepad(joysticks[i])) {
            SDL_Joystick *joystick = SDL_OpenJoystick(joysticks[i]);
            if (joystick) {
                LOG("[GAMEPAD] Layout: %d axes, %d buttons, %d hats\n",
                    SDL_GetNumJoystickAxes(joystick), SDL_GetNumJoystickButtons(joystick),
                    SDL_GetNumJoystickHats(joystick));
                SDL_CloseJoystick(joystick);
            }
        }
    }
    SDL_free(joysticks);
    for (int i = 0; ids && i < count; ++i) {
        bool opened = false;
        for (const Pad &pad : s_Pads) opened |= pad.id == ids[i];
        if (opened) continue;
        for (Pad &pad : s_Pads) {
            if (pad.handle) continue;
            SDL_Gamepad *handle = SDL_OpenGamepad(ids[i]);
            if (handle) {
                pad.handle = handle;
                pad.id = ids[i];
                LOG("[GAMEPAD] Connected: %s\n", SDL_GetGamepadName(handle));
            } else {
                LOG("[GAMEPAD] Open failed: %s\n", SDL_GetError());
            }
            break;
        }
    }
    SDL_free(ids);
    s_Dirty = false;
    if (DAT_004877a4 == 0xE1) DAT_004877b1 = 1;
}

std::array<bool, kBindingCount> MenuButtons()
{
    std::array<bool, kBindingCount> result{};
    for (const Pad &pad : s_Pads) {
        if (!pad.armed) continue;
        for (int i = 0; i < kBindingCount; ++i)
            result[i] = result[i] || pad.down[i];
    }
    return result;
}
} // namespace

GamepadSettings Gamepad_DefaultSettings(void)
{
    GamepadSettings result{};
    result.devices = {1, 2, 3, 4};
    result.deadzone = 20;
    result.sensitivity = 100;
    for (auto &bindings : result.bindings) {
        bindings = {AxisBinding(SDL_GAMEPAD_AXIS_LEFTX, false),
                    AxisBinding(SDL_GAMEPAD_AXIS_LEFTX, true),
                    AxisBinding(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, true),
                    1 + SDL_GAMEPAD_BUTTON_SOUTH, 1 + SDL_GAMEPAD_BUTTON_WEST,
                    1 + SDL_GAMEPAD_BUTTON_NORTH, 0};
    }
    return result;
}

const GamepadSettings &Gamepad_GetSettings(void) { return s_Settings; }
void Gamepad_ApplySettings(const GamepadSettings &settings)
{
    s_Settings = settings;
    s_Settings.deadzone = Clamp(settings.deadzone, 5, 80);
    s_Settings.sensitivity = Clamp(settings.sensitivity, 50, 200);
    for (int &device : s_Settings.devices) device = Clamp(device, 0, 4);
    for (auto &bindings : s_Settings.bindings)
        for (int &binding : bindings) binding = Clamp(binding, 0, kBindingCount - 1);
    // Re-arm input, but retain menu selection so repeated binding changes don't
    // send controller users back to the first row.
    for (Pad &pad : s_Pads) { pad.armed = false; pad.down.fill(false); }
    s_Previous.fill(false);
}
void Gamepad_DevicesChanged(void) { s_Dirty = true; }
void Gamepad_FocusLost(void)
{
    for (Pad &pad : s_Pads) { pad.armed = false; pad.down.fill(false); }
    s_Previous.fill(false);
    s_MenuOwned = false;
    s_Selected = -1;
}

void Gamepad_Update(void)
{
    if (!s_Initialized) {
        if (s_InitAttempted) return;
        s_InitAttempted = true;
#if defined(_WIN32)
        // SDL leaves these desktop Windows backends opt-in. Xbox BLE can be
        // absent from XInput even while Windows receives its joystick reports.
        SDL_SetHint(SDL_HINT_JOYSTICK_GAMEINPUT, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "1");
#endif
        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
            LOG("[GAMEPAD] SDL initialization failed: %s\n", SDL_GetError());
            return;
        }
        // Optional community mappings; absence is normal, built-in mappings remain.
        SDL_PathInfo mappingInfo{};
        if (SDL_GetPathInfo("gamecontrollerdb.txt", &mappingInfo))
            SDL_AddGamepadMappingsFromFile("gamecontrollerdb.txt");
        s_Initialized = true;
    }
    if (s_Dirty) RefreshDevices();
    const bool gameplay = g_GameState == GAME_STATE_GAMEPLAY && g_SubState == GAMEPLAY_ACTIVE;
    if (gameplay != s_Gameplay) {
        Gamepad_FocusLost();
        s_Gameplay = gameplay;
    }
    for (Pad &pad : s_Pads) {
        pad.down.fill(false);
        if (!pad.handle || !g_bIsActive) { pad.armed = false; continue; }
        bool neutral = true;
        for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT && button < kButtonCount; ++button) {
            pad.down[1 + button] = SDL_GetGamepadButton(pad.handle, static_cast<SDL_GamepadButton>(button));
            neutral &= !pad.down[1 + button];
        }
        // Digital simulation actions: sensitivity changes activation threshold,
        // never the original rotation speed, thrust, or network command format.
        const int threshold = Clamp(32767 * s_Settings.deadzone /
                                        s_Settings.sensitivity, 1638, 29490);
        for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; ++axis) {
            const int value = SDL_GetGamepadAxis(pad.handle, static_cast<SDL_GamepadAxis>(axis));
            pad.down[AxisBinding(static_cast<SDL_GamepadAxis>(axis), false)] = value < -threshold;
            pad.down[AxisBinding(static_cast<SDL_GamepadAxis>(axis), true)] = value > threshold;
            neutral &= std::abs(value) <= threshold;
        }
        if (!pad.armed && neutral) pad.armed = true;
        if (!pad.armed) pad.down.fill(false);
    }
    const auto down = MenuButtons();
    const auto pressed = [&](SDL_GamepadButton button) {
        return down[1 + button] && !s_Previous[1 + button];
    };
    if (g_bIsActive && g_GameState == GAME_STATE_GAMEPLAY) {
        if (pressed(SDL_GAMEPAD_BUTTON_START)) g_KeyboardState[0x01] |= 0x80;
        if (g_SubState != GAMEPLAY_ACTIVE) {
            if (pressed(SDL_GAMEPAD_BUTTON_SOUTH)) g_KeyboardState[0x1c] |= 0x80;
            if (pressed(SDL_GAMEPAD_BUTTON_EAST)) g_KeyboardState[0x01] |= 0x80;
            if (pressed(SDL_GAMEPAD_BUTTON_BACK)) g_KeyboardState[0x44] |= 0x80;
        }
    }
    // MenuButtons is compared again by MenuInput before committing previous state.
    if (g_GameState == GAME_STATE_GAMEPLAY) s_Previous = down;
}

uint8_t Gamepad_PlayerActions(int player)
{
    if (!g_bIsActive || player < 0 || player >= 4) return 0;
    const int device = s_Settings.devices[player];
    if (device == 0) return 0;
    const Pad &pad = s_Pads[device - 1];
    if (!pad.armed) return 0;
    // The first player owns a shared device; never steer two ships accidentally.
    for (int i = 0; i < player; ++i)
        if (s_Settings.devices[i] == device) return 0;
    uint8_t result = 0;
    for (int i = 0; i < 7; ++i)
        if (pad.down[s_Settings.bindings[player][i]]) result |= kActions[i];
    // D-pad is a convenient digital alternative, including base weapon selection.
    if (pad.down[1 + SDL_GAMEPAD_BUTTON_DPAD_LEFT]) result |= GAME_ACTION_TURN_LEFT;
    if (pad.down[1 + SDL_GAMEPAD_BUTTON_DPAD_RIGHT]) result |= GAME_ACTION_TURN_RIGHT;
    if (pad.down[1 + SDL_GAMEPAD_BUTTON_DPAD_UP]) result |= GAME_ACTION_THRUST;
    if (pad.down[1 + SDL_GAMEPAD_BUTTON_DPAD_DOWN]) result |= GAME_ACTION_BRAKE;
    if (pad.down[AxisBinding(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, true)]) result |= GAME_ACTION_BRAKE;
    return result;
}

void Gamepad_MenuInput(void)
{
    const auto down = MenuButtons();
    const auto pressed = [&](SDL_GamepadButton button) {
        return down[1 + button] && !s_Previous[1 + button];
    };
    float mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    if (static_cast<int>(mx) != s_MouseX || static_cast<int>(my) != s_MouseY) s_MenuOwned = false;
    s_MouseX = static_cast<int>(mx); s_MouseY = static_cast<int>(my);
    if (s_Page != DAT_004877a4) { s_Page = DAT_004877a4; s_Selected = -1; }
    int direction = down[1 + SDL_GAMEPAD_BUTTON_DPAD_DOWN] ? 1 :
                    down[1 + SDL_GAMEPAD_BUTTON_DPAD_UP] ? -1 : 0;
    MenuItem *items = static_cast<MenuItem *>(g_GameViewData);
    const uint32_t now = static_cast<uint32_t>(SDL_GetTicks());
    const float elapsed = s_MenuTick ? static_cast<float>(std::min(now - s_MenuTick, 32u)) / 1000.0f : 0.0f;
    s_MenuTick = now;
    if (!s_MenuOwned) { s_CursorX = g_MouseDeltaX; s_CursorY = g_MouseDeltaY; }
    float stickX = 0.0f, stickY = 0.0f;
    const int threshold = Clamp(32767 * s_Settings.deadzone / s_Settings.sensitivity, 1638, 29490);
    for (const Pad &pad : s_Pads) {
        if (!pad.handle || !pad.armed || !g_bIsActive) continue;
        const auto filtered = [&](SDL_GamepadAxis axis) {
            const int value = SDL_GetGamepadAxis(pad.handle, axis);
            if (std::abs(value) <= threshold) return 0.0f;
            const float amount = static_cast<float>(std::abs(value) - threshold) /
                                 static_cast<float>(32768 - threshold);
            return value < 0 ? -amount : amount;
        };
        const float x = filtered(SDL_GAMEPAD_AXIS_LEFTX);
        const float y = filtered(SDL_GAMEPAD_AXIS_LEFTY);
        // One pad owns the analog cursor at a time; don't sum opposing sticks.
        if (x != 0.0f || y != 0.0f) { stickX = x; stickY = y; break; }
    }
    if (g_InputMode == 0 && (stickX != 0.0f || stickY != 0.0f)) {
        s_MenuOwned = true;
        s_Selected = -1;
        s_CursorX += static_cast<int>(stickX * 360.0f * elapsed * 262144.0f);
        s_CursorY += static_cast<int>(stickY * 360.0f * elapsed * 262144.0f);
        s_CursorX = Clamp(s_CursorX, 15 << 18, 625 << 18);
        s_CursorY = Clamp(s_CursorY, 15 << 18, 465 << 18);
    } else if (g_InputMode == 1) {
        // Hold confirm while moving the stick, just like dragging with a mouse.
        DAT_004877e8 += static_cast<int>(stickX * 360.0f * elapsed * 128.0f);
    }
    if (g_bIsActive && items && g_InputMode == 0 && direction &&
        (direction != s_Direction || static_cast<int32_t>(now - s_RepeatAt) >= 0)) {
        const int current = s_Selected >= 0 && s_Selected < DAT_004877a8 ? s_Selected : -1;
        int best = -1, bestScore = INT_MAX;
        for (int i = 0; i < DAT_004877a8; ++i) {
            if (!items[i].clickable || i == current || items[i].height <= 8) continue;
            const int y = items[i].y;
            const int x = items[i].x;
            int score = y * 1000 + x;
            if (direction < 0) score = -score;
            if (current >= 0) {
                const int delta = (y - items[current].y) * 1000 + x - items[current].x;
                score = direction * delta;
                if (score <= 0) score += 1000000; // wrap
            }
            if (score < bestScore) { best = i; bestScore = score; }
        }
        if (best >= 0) { s_Selected = best; s_MenuOwned = true; }
        s_RepeatAt = now + (direction != s_Direction ? 350 : 140);
    }
    s_Direction = direction;
    if (s_MenuOwned && items && s_Selected >= 0 && s_Selected < DAT_004877a8) {
        s_CursorX = (items[s_Selected].x + items[s_Selected].width / 2) << 18;
        s_CursorY = (items[s_Selected].y + items[s_Selected].height / 2) << 18;
    }
    if (s_MenuOwned) { g_MouseDeltaX = s_CursorX; g_MouseDeltaY = s_CursorY; }
    if (g_bIsActive && g_InputMode != 2) {
        if (down[1 + SDL_GAMEPAD_BUTTON_SOUTH]) g_KeyboardState[0x9d] |= 0x80;
        if (pressed(SDL_GAMEPAD_BUTTON_EAST)) g_KeyboardState[0x01] |= 0x80;
        if (pressed(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) g_KeyboardState[0x9d] |= 0x80;
        if (pressed(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) g_KeyboardState[0x36] |= 0x80;
        if (g_InputMode == 1) {
            if (down[1 + SDL_GAMEPAD_BUTTON_DPAD_LEFT]) DAT_004877e8 -= 0x400;
            if (down[1 + SDL_GAMEPAD_BUTTON_DPAD_RIGHT]) DAT_004877e8 += 0x400;
        }
    }
    s_Previous = down;
}

int Gamepad_BindingCount(void) { return kBindingCount; }
int Gamepad_CycleBinding(int binding, int direction)
{
    do {
        binding = (binding + direction + kBindingCount) % kBindingCount;
        // Reserved future buttons and negative trigger axes aren't useful choices.
    } while ((binding > SDL_GAMEPAD_BUTTON_COUNT && binding <= kButtonCount) ||
             binding == AxisBinding(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, false) ||
             binding == AxisBinding(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, false));
    return binding;
}
const char *Gamepad_BindingName(int binding, int device)
{
    if (binding <= 0 || binding >= kBindingCount) return Text_Get("gamepad.unbound");
    if (binding <= kButtonCount) {
        if (binding > SDL_GAMEPAD_BUTTON_COUNT) return Text_Get("gamepad.unbound");
        static const char *keys[] = {
            "gamepad.input.bottom", "gamepad.input.right", "gamepad.input.left", "gamepad.input.top",
            "gamepad.input.back", "gamepad.input.guide", "gamepad.input.start",
            "gamepad.input.left_click", "gamepad.input.right_click",
            "gamepad.input.left_shoulder", "gamepad.input.right_shoulder",
            "gamepad.input.dpad_up", "gamepad.input.dpad_down", "gamepad.input.dpad_left", "gamepad.input.dpad_right"
        };
        const int button = binding - 1;
        const char *name = button < 15 ? Text_Get(keys[button]) : nullptr;
        static char text[128];
        if (!name) {
            snprintf(text, sizeof(text), Text_Get("gamepad.input.extra_format"), button - 14);
            return text;
        }
        if (button < 4) {
            const Pad *pad = device >= 1 && device <= 4 ? &s_Pads[device - 1] : nullptr;
            SDL_GamepadButtonLabel label = pad && pad->handle
                ? SDL_GetGamepadButtonLabel(pad->handle, static_cast<SDL_GamepadButton>(button))
                : SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
            const char *legend = nullptr;
            switch (label) {
            case SDL_GAMEPAD_BUTTON_LABEL_A: legend = "A"; break;
            case SDL_GAMEPAD_BUTTON_LABEL_B: legend = "B"; break;
            case SDL_GAMEPAD_BUTTON_LABEL_X: legend = "X"; break;
            case SDL_GAMEPAD_BUTTON_LABEL_Y: legend = "Y"; break;
            case SDL_GAMEPAD_BUTTON_LABEL_CROSS: legend = Text_Get("gamepad.input.cross"); break;
            case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE: legend = Text_Get("gamepad.input.circle"); break;
            case SDL_GAMEPAD_BUTTON_LABEL_SQUARE: legend = Text_Get("gamepad.input.square"); break;
            case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: legend = Text_Get("gamepad.input.triangle"); break;
            default: break;
            }
            if (legend) { snprintf(text, sizeof(text), "%s (%s)", name, legend); return text; }
        }
        return name;
    }
    static const char *keys[12] = {
        "gamepad.input.lstick_left", "gamepad.input.lstick_right", "gamepad.input.lstick_up", "gamepad.input.lstick_down",
        "gamepad.input.rstick_left", "gamepad.input.rstick_right", "gamepad.input.rstick_up", "gamepad.input.rstick_down",
        "gamepad.input.left_trigger", "gamepad.input.left_trigger", "gamepad.input.right_trigger", "gamepad.input.right_trigger"
    };
    return Text_Get(keys[binding - 1 - kButtonCount]);
}
const char *Gamepad_DeviceName(int device)
{
    if (device < 1 || device > 4) return Text_Get("gamepad.disabled");
    const Pad &pad = s_Pads[device - 1];
    const char *name = pad.handle ? SDL_GetGamepadName(pad.handle) : nullptr;
    return name ? name : Text_Get("gamepad.disconnected");
}
void Gamepad_Shutdown(void)
{
    for (Pad &pad : s_Pads) { if (pad.handle) SDL_CloseGamepad(pad.handle); pad = Pad{}; }
    if (s_Initialized) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    s_Initialized = false; s_InitAttempted = false; s_Dirty = true;
}
