#include "tou.h"
#define INITGUID
#include <dinput.h>
#include <stdio.h>
#include <string.h>

// Globals used in Init
// Many of these are simple flags or config values
// DAT_0048769c extern in tou.h
int DAT_00487649 = 0;
struct {
  char _3_1_;         // Output Type
} GlobalConfig = {3}; // DAT_00483720, Default 3?

// Globals
// DAT_00487880 defined in memory.cpp
// Game State extern in tou.h

// Pointers at DAT_00489ebc
int *DAT_00489ebc = NULL;

// In memory.cpp: void* DAT_00487ab0;
// Here we used it as param to Init_Math_Tables(int* buffer).
// So let's declare it matching the usage or sync.
// We will resolve this by declaring extern void* and casting.
int DAT_00483c04, DAT_00483c08, DAT_00483c14, DAT_00483c18;
int DAT_00483c44, DAT_00483c48, DAT_00483c0c, DAT_00483c4c;
int DAT_00483c10, DAT_00483c50, DAT_00483c54, DAT_00483c58;
int DAT_00483c1c, DAT_00483c5c, DAT_00483c20, DAT_00483c60;
int DAT_00483c24, DAT_00483c64, DAT_00483c28, DAT_00483c68;
int DAT_00483c00;
struct {
  char _0_1_;
} DAT_00489d7c;
struct {
  char _1_1_;
  char _2_1_;
} DAT_00487640;
// Authorization Globals
int DAT_0048764b = 0;
int DAT_0048764a = 0;

// Initialization Stubs (to be filled later)
// Init_Memory_Pools (004201a0) declared in tou.h
void FUN_00425780(int p1, int p2) {}
void FUN_0041eae0() {}
void FUN_0045a060() {}
void FUN_0045b2a0() {}
void FUN_0041fc10() {}
void FUN_0041fe70() {}
void FUN_0041f900() {}
void FUN_00422a10() {}
void FUN_0042d8b0() {}
int FUN_00422740() { return 1; }
void FUN_00420be0() {}
void FUN_0041e580() {}
int FUN_00414060() { return 1; }
void FUN_00413f70() {}
void FUN_0041e4a0() {}

// Early Init (WinMain start)
void Early_Init_Vars(void) {
  DAT_004892a0 = 0; // Memory Tracker Reset
}

// System Check (WinMain after window creation)
int System_Init_Check(void) {
  DWORD dwTime;
  int iVar2;

  dwTime = timeGetTime();
  // FUN_00464409(dwTime); // Random seed?

  LOG("[DEBUG] System_Init_Check: Calling Init_Memory_Pools...\n");
  Init_Memory_Pools();
  LOG("[DEBUG] System_Init_Check: Memory Pools Init Success.\n");
  // Casting void* to int* for math table init
  LOG("[DEBUG] System_Init_Check: Calling Init_Math_Tables...\n");
  Init_Math_Tables((int *)DAT_00487ab0, 0x800);
  LOG("[DEBUG] System_Init_Check: Math Tables Init Success.\n");
  Init_Game_Config(); // Was FUN_004207c0

  DAT_0048769c = 0xff;
  DAT_00487649 = 1;

  // Check Sound Config
  if (GlobalConfig._3_1_ != 3) { // 3 = NOSOUND?
    if (Init_Sound_System() == 0) {
      // Failed sound init
      // goto LAB_0041d4d2; => Just proceed to main init, probably disabling
      // sound? In original code it jumps to LAB_0041d4d2 which skips
      // DAT_00487649=0
      goto MainInit;
    }
  }
  DAT_00487649 = 0;

MainInit:
  FUN_0041eae0();
  FUN_0045a060();
  FUN_0045b2a0();

  // Initialize DAT_00487880 (Some large structure/array)
  // Using manual indices based on decompilation (byte offsets / 4)
  // +8 -> [2], +4 -> [1], +0 -> [0], +0xc -> [3] ...
  DAT_00487880[2] = 0x19000;
  DAT_00487880[1] = 0xc0000;
  DAT_00487880[0] = 0;
  DAT_00487880[3] = 0x2710000;
  // ... Filling logical defaults ...

  // ... Pointers at DAT_00489ebc ...

  // Calls to other sub-systems
  FUN_0041fc10();
  FUN_0041fe70();
  FUN_0041f900();

  // Loop to clear DAT_00487ac0 (likely large array)
  // memset(&DAT_00487ac0, 0, ...);

  LOG("[DEBUG] System_Init_Check: Stage 1\n");
  FUN_00422a10();
  LOG("[DEBUG] System_Init_Check: Stage 2\n");
  FUN_0042d8b0();

  if (FUN_00422740() != 1) {
    LOG("[DEBUG] System_Init_Check: FUN_00422740 Failed.\n");
    return 2; // Fixed return value
  }

  FUN_00420be0();
  FUN_0041e580();

  if (FUN_00414060() == 0) {
    LOG("[ERROR] System_Init_Check: FUN_00414060 Failed.\n");
    return 3;
  }

  FUN_00413f70();

  // Resolution / Gameplay Constants?
  DAT_00483c04 = 320; // 0x140
  DAT_00483c08 = 320;
  DAT_00489296 = 0;
  DAT_00483c14 = 640; // 0x280
  DAT_00483c18 = 640;
  DAT_00489297 = 0;

  // ... More constants (looks like screen mode variants) ...
  // 640x480, 800x600, etc.

  DAT_00483c00 = 10;
  DAT_00489d7c._0_1_ = 0;
  DAT_00487640._1_1_ = 0xff;
  DAT_00487640._2_1_ = 5;

  FUN_0041e4a0();

  DAT_0048764b = 0;
  DAT_0048764a = 0;

  return 0;
}

// Globals for DInput
LPDIRECTINPUT A_DirectInput_Interface = NULL;    // DAT_00489ed4
LPDIRECTINPUTDEVICE A_DI_Device_Keyboard = NULL; // DAT_00489ee4
// lpDI_Device is usually Mouse or Joystick?
// Based on decomp, 00489ec0 seems to be the primary input device used in loop
// (Mouse?) Or Keyboard? Let's assume Keyboard (ee4) and Mouse (ec0).

int Init_DirectInput(void) {
  HRESULT hr;

  LOG("[INFO] Init_DirectInput: Start\n");

  // 1. Create DirectInput Interface
  hr = DirectInputCreateA(GetModuleHandle(NULL), DIRECTINPUT_VERSION,
                          &A_DirectInput_Interface, NULL);
  if (FAILED(hr)) {
    LOG("[ERROR] DirectInputCreateA failed: 0x%08X\n", (unsigned int)hr);
    return 0;
  }
  LOG("[INFO] DirectInput Interface Created.\n");

  // 2. Keyboard Init
  hr = A_DirectInput_Interface->CreateDevice(GUID_SysKeyboard,
                                             &A_DI_Device_Keyboard, NULL);
  if (FAILED(hr)) {
    LOG("[ERROR] CreateDevice Keyboard failed: 0x%08X\n", (unsigned int)hr);
    return 0;
  }

  hr = A_DI_Device_Keyboard->SetDataFormat(&c_dfDIKeyboard);
  if (FAILED(hr)) {
    LOG("[ERROR] SetDataFormat Keyboard failed: 0x%08X\n", (unsigned int)hr);
    return 0;
  }

  hr = A_DI_Device_Keyboard->SetCooperativeLevel(
      hWnd_Main, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
  if (FAILED(hr)) {
    LOG("[ERROR] SetCooperativeLevel Keyboard failed: 0x%08X\n",
        (unsigned int)hr);
    return 0;
  }

  A_DI_Device_Keyboard->Acquire();
  LOG("[INFO] Keyboard Initialized.\n");

  // 3. Mouse Init (DAT_00489ec0)
  // Using global lpDI_Device for this one
  hr = A_DirectInput_Interface->CreateDevice(GUID_SysMouse, &lpDI_Device, NULL);
  if (FAILED(hr)) {
    LOG("[ERROR] CreateDevice Mouse failed: 0x%08X\n", (unsigned int)hr);
    return 0;
  }

  hr = lpDI_Device->SetDataFormat(&c_dfDIMouse);
  if (FAILED(hr)) {
    LOG("[ERROR] SetDataFormat Mouse failed: 0x%08X\n", (unsigned int)hr);
    return 0;
  }

  hr = lpDI_Device->SetCooperativeLevel(hWnd_Main,
                                        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
  if (FAILED(hr)) {
    LOG("[ERROR] SetCooperativeLevel Mouse failed: 0x%08X\n", (unsigned int)hr);
    return 0;
  }

  lpDI_Device->Acquire();
  LOG("[INFO] Mouse Initialized.\n");

  LOG("[INFO] Init_DirectInput: Success\n");
  return 1;
}

void Handle_Init_Error(HWND hWnd, int ErrorCode) {
  const char *lpText;
  switch (ErrorCode) {
  case 0:
    lpText = "DirectDraw Init FAILED. Install DirectX 6.0 or higher.";
    break;
  case 1:
    lpText = "DirectDraw Init FAILED. Couldn't set display mode.";
    break;
  case 2:
    lpText = "DirectDraw Init FAILED. Not enough memory.";
    break;
  case 3:
    lpText = "DirectInput Init FAILED. Read readme.txt.";
    break;
  default:
    lpText = "Unknown error.";
    break;
  }
  MessageBoxA(hWnd, lpText, "Tunnels of Underworld v0.1", MB_ICONERROR);
  DestroyWindow(hWnd);
}

// Globals for Game Config (Init_Game_Config)
// Simplified declarations - need to be matched with actual usage later
int DAT_00481f58 = 0;
int DAT_00481f59 = 0;
int DAT_00481f5a = 0;
int DAT_00481f5b = 0;
int DAT_00481f5c = 0; // Likely start of array

// Structs inferred from decomp
typedef struct {
  char _0_1_;
  char _1_1_;
  char _2_1_;
  char _3_1_;
} ConfigStruct;

ConfigStruct DAT_0048227c;
ConfigStruct DAT_00483724;
ConfigStruct DAT_00483748;
ConfigStruct DAT_0048374c;
ConfigStruct DAT_00483750;
ConfigStruct DAT_00483754;
ConfigStruct DAT_00483758;
ConfigStruct DAT_004837bc;
int DAT_0048373f;
int DAT_004822ce;
int DAT_0048371e;
int DAT_0048371f;
int DAT_00483728;
int DAT_00483729;
int DAT_0048372b;
int DAT_0048372a;
int DAT_0048372c;
int DAT_0048372d;
int DAT_0048372e;
int DAT_0048372f;
int DAT_00483730;
int DAT_00483731;
int DAT_00483734;
int DAT_00483735;
int DAT_00483736;
int DAT_00483737;

// Init Game Config (004207c0)
// Sets up default values for game entities/logic
void Init_Game_Config(void) {
  DAT_00481f58 = 1;
  DAT_00481f59 = 0;
  DAT_00481f5a = 1;
  DAT_00481f5b = 0x32;

  // Clear 200 ints starting at DAT_00481f5c
  // memset(&DAT_00481f5c, 0, 200 * 4);

  DAT_0048227c._0_1_ = 2;
  DAT_0048227c._1_1_ = 1;

  // Loop filling DAT_0048227c array?
  // Decomp: do { ... } while ((int)puVar4 < 0x48330e);
  // Range 48240e to 48330e is ~3840 bytes.
  // Seems to initialize a large table of entities or tiles.
  // Stubbing the complex loop for now, just setting key globals.

  DAT_00483724._1_1_ = 5;
  DAT_0048373f = 5;

  // ... Setting many constants ...
  DAT_00483748._0_1_ = 0x14;

  // Key mappings?
  /*
  DAT_004837c2 = 200; // C8
  DAT_004837c3 = 0xd0;
  */
}

// Free Game Resources (0040ffc0)
void Free_Game_Resources(void) {
  // Mem_Free(DAT_00481f50);
  // Mem_Free(DAT_00487814);
  // Mem_Free(DAT_00489ea4);
  // ...
  // Using mocks for now
}
