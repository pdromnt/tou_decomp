#ifndef TOU_H
#define TOU_H

#include <ddraw.h>
#include <mmsystem.h>
#include <windows.h>

#define DIRECTINPUT_VERSION                                                    \
  0x0700 // Force explicit version if needed, or just include
#include "fmod_mock.h"
#include <dinput.h>

// Structures
typedef struct {
  unsigned int handle; // FSOUND_SAMPLE* handle cast to int
  unsigned char volume;
  unsigned char padding[3];
} SoundEntry;

// Global Variables
// Global Variables
extern HWND hWnd_Main;                     // DAT_00489edc
extern LPDIRECTDRAW7 lpDD;                 // DAT_00489ec8
extern LPDIRECTDRAWSURFACE7 lpDDS_Primary; // DAT_00489ed8
extern LPDIRECTDRAWSURFACE7 lpDDS_Back;    // DAT_00489ed0
extern BOOL g_bIsActive;                   // DAT_00489ec4
extern BYTE g_GameState;                   // DAT_004877a0

// Unknown Globals (to be named later)
extern int DAT_00489296;
extern int DAT_00489297;
extern int DAT_00489298;
extern DWORD DAT_004892b0;
extern int DAT_004892b4;
extern int DAT_004892b4;
extern char DAT_004877be;
extern int DAT_004892a0; // Memory Tracker

#include "ijl_mock.h"

// Function Prototypes
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Sound Manager
int Init_Sound_System(void); // 0040df60
void Load_Game_Sounds(void); // 0040e9e0
void Load_Sound_Sample(const char bLoop, int index, int vol_priority,
                       const char *filename); // 0040e8e0

// Asset Loader
unsigned char *Load_JPEG_Asset(int data_size, void *data_ptr,
                               int mode); // 00420c90

extern int DAT_00481d08; // Width

// Sprite Tables
extern int Sprite_Offsets[20000];            // DAT_00489234
extern unsigned char Sprite_Widths[20000];   // DAT_00489e8c
extern unsigned char Sprite_Heights[20000];  // DAT_00489e88
extern unsigned short Sprite_Data[0x200000]; // DAT_00487ab4

// Font Tables
extern int Font_Char_Table[0x2000];             // DAT_00483c84
extern unsigned char Font_Pixel_Data[0x100000]; // DAT_00487878

// Render
void Draw_Text_To_Buffer(char *text, int font_idx, int y_offset_base,
                         unsigned short *dest_buffer, int pitch,
                         int dest_pitch_pixels, int max_width,
                         int param_8); // 0040aed0
void Draw_Sprite_To_Buffer(int sprite_idx, unsigned short *dest_buffer,
                           int pitch, int flags, int palette_idx); // 0042fa70
void Load_Background_To_Buffer(char index);                        // 0042d710
// Sprite Tables
void Init_Game_Config(void);
void Input_Update(void);
int Init_DirectInput(void);

// Logic
void Game_State_Manager(void);              // 00461260
void Game_Update_Render(void);              // 00461710
void Render_Game_View(void);                // 0042f3a0
void Stop_All_Sounds(void);                 // 0040e7c0
int Init_DirectDraw(int width, int height); // 004610e0

// State Functions
void Free_Game_Resources(void); // 0040ffc0
void Handle_Menu_State(void);   // 004611d0
void Intro_Sequence(void);      // 0045c720
void Init_New_Game(void);       // 004228a0
void Cleanup_Sound(void);       // 0040e880
void Mem_Free(void *ptr);       // 0046f4e6
// Initialization
void Early_Init_Vars(void);                       // 0041ead0
int System_Init_Check(void);                      // 0041d480
int Init_DirectInput(void);                       // 004620f0
void Handle_Init_Error(HWND hWnd, int ErrorCode); // 00462010

extern struct IDirectInputDeviceA *lpDI_Device; // DAT_00489ec0
extern int DAT_00483c18;
extern int DAT_00483c58;

// Memory & Math
void Init_Memory_Pools(void);                           // 004201a0
void Init_Math_Tables(int *buffer, unsigned int count); // 00425780
void *Mem_Alloc(size_t size);                           // 0046f4bd

// Externs
extern void *DAT_00487ab0;

#endif // TOU_H
