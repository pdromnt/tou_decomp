# Gamepads

## Setup and default controls

Open Options > Controls > Gamepads. Select Player 1..4, then controller slot
1..4 (or Disabled). Slots are allocated in connection order and remain stable
while connected. A disconnected slot stays empty; a newly connected controller
fills the first empty slot. Assignment is a slot preference, not a permanent
hardware identity. Recheck assignments if reconnecting controllers in a new order.
If two players choose one controller, only the first receives its gameplay input.

Xbox defaults:

| Control | Action |
| --- | --- |
| Left stick horizontal | Turn left/right |
| RT | Thrust |
| A | Primary fire |
| X | Secondary fire |
| Y | Detonate owned weapons |
| LT or D-pad Down | Brake/reverse |
| B | Unassigned in gameplay; available for rebinding |
| D-pad | Original left/right/thrust/brake actions; weapon selection in bases |
| Start/Menu | Open/close ESC menu |

Weapon selection keeps the original base behavior, not global weapon cycling.
Steering, thrust, both fire actions and detonation can be rebound to buttons or axis directions.
Stick bindings name the stick and direction, and triggers use their full names.
Face buttons list their physical position and SDL's detected hardware label:
Xbox letters, PlayStation symbol names, or Nintendo's letter arrangement.
Unknown/disconnected layouts show physical position only.
Use left/right or mouse clicks to cycle a binding. Menu shortcuts remain fixed.
Brake/reverse is listed directly below thrust. Open **D-pad and shortcuts**
for the four separate D-pad directions and menu actions.
Each shortcut acts independently; no button combination is needed.
Left Trigger and D-pad Down are fixed brake/reverse shortcuts.
B still returns from menus, but no longer brakes by default during gameplay.
The initial prototype's unchanged default layout is upgraded automatically;
custom layouts remain intact.

Deadzone and sensitivity are global activation settings. Gameplay still uses
digital action bits: higher sensitivity lowers the stick/trigger threshold; it
does not increase ship rotation speed or acceleration. Defaults are a 20-percent
deadzone and 100-percent sensitivity. This preserves simulation/network behavior.

## Menus and round panels

The left stick moves the game cursor freely in both directions, with a deadzone
and gradual speed outside it. Hold A and move the stick horizontally to drag
a slider. D-pad up/down also selects clickable rows directly.
A activates; B returns. D-pad right/left supplies primary/secondary clicks
to increase/decrease values. Hold A plus D-pad left/right to adjust sliders too.
Moving the mouse takes cursor ownership back. Keyboard key capture and LAN
address entry still require a keyboard; no on-screen keyboard is implemented.

Start opens the original ESC panel, which pauses gameplay under existing session
rules. On that panel A means Next level, B or Start means Back to game, and
Back/View means Exit to menu. On between-level panels A continues and Back/View
ends the match. These shortcuts retain the host-only restrictions in LAN.

## Compatibility and safety

The implementation uses SDL3's mapped Gamepad API and SDL's platform drivers,
including HIDAPI, rather than requiring XInput on every platform. A raw HID
device is not automatically a mapped gamepad: the OS must expose it and SDL
must recognize its layout. An optional user-supplied `gamecontrollerdb.txt`
beside the game assets is loaded at startup for additional SDL mappings.
No external controller database or extra driver code is bundled.

On Windows, prefer the MSVC/Windows SDK build used by CI. The older local
TDM-GCC toolchain lacks the headers for SDL's Windows Gaming Input and GameInput
backends. Testing an Xbox Series X Bluetooth LE controller found that XInput
reported no connected device, while DirectInput exposed an unmapped joystick
when XInput was disabled. Merely compiling HIDAPI is not sufficient for every
Xbox Bluetooth path. SDL's Windows HID filter is patched only for Xbox Series
BLE VID/PID `045e:0b13` endpoints whose bus is explicitly Bluetooth. This lets
SDL's existing Xbox HIDAPI parser handle the actual reports without disabling
XInput globally. Wired proxy endpoints retain their original filtering.
The rebuilt local version detected the controller through this HID path, and
Xbox Bluetooth menus and gameplay passed hands-on testing. The earlier
DirectInput experiment was removed because its axes mirrored stick movement
into trigger values. Reconnect and mixed wired/BLE sessions still need
human acceptance.

Keyboard and pad actions are ORed together before the unchanged replay/LAN
command path. LAN always samples this machine's Player 1 controller, regardless
of its assigned network player number. Remote players do not receive local pads.
Controller configuration is not part of the simulation snapshot or LAN settings.

Controllers must return to neutral after a focus loss, reconnect, settings
change, or transition into/out of active gameplay. Disconnect clears input;
it does not automatically pause a LAN match. Full settings reset also restores
controller defaults. Old JSON settings without a gamepad section retain defaults.

## Remaining human acceptance

Xbox Bluetooth gameplay and menu navigation have passed. Remaining checks:

- Analog cursor, D-pad navigation, hold-confirm dragging, keyboard/mouse handoff,
  all languages, and detected Xbox/PlayStation/Nintendo face-button labels.
- Both fire actions, Y detonation, thrust, steering, brake, base weapon selection.
- ESC/round-panel shortcuts, including host-only LAN actions.
- Unplug/replug during play; release before steering resumes; no stuck actions.
- Focus loss/return and paused menus; held buttons do not skip or resume a screen.
- Two/four local controllers, duplicate assignments, disabled slots, controller order.
- LAN client in a nonzero player slot uses its own Player 1 controller.
- Save/relaunch, old settings, and factory reset restore the intended bindings.
- Other controller families and Linux/macOS require separate hardware.
