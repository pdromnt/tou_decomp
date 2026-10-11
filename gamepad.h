#ifndef TOU_GAMEPAD_H
#define TOU_GAMEPAD_H

#include <array>
#include <stdint.h>

// Device numbers are stable connection slots, not SDL instance IDs.
// 0 disables a player's pad; 1..4 select a connection slot.
struct GamepadSettings {
    std::array<int, 4> devices;
    int deadzone;
    int sensitivity;
    std::array<std::array<int, 7>, 4> bindings;
};

GamepadSettings Gamepad_DefaultSettings(void);
const GamepadSettings &Gamepad_GetSettings(void);
void Gamepad_ApplySettings(const GamepadSettings &settings);
void Gamepad_Update(void);
void Gamepad_DevicesChanged(void);
void Gamepad_FocusLost(void);
void Gamepad_Shutdown(void);
uint8_t Gamepad_PlayerActions(int player);
void Gamepad_MenuInput(void);
int Gamepad_BindingCount(void);
int Gamepad_CycleBinding(int binding, int direction);
const char *Gamepad_BindingName(int binding, int device);
const char *Gamepad_DeviceName(int device);

#endif
