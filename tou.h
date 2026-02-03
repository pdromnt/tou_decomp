#ifndef TOU_H
#define TOU_H

#define DIRECTDRAW_VERSION 0x0100
#include <ddraw.h>
#include <mmsystem.h>
#include <windows.h>

#define DIRECTINPUT_VERSION 0x0700
#include "fmod_mock.h"
#include <dinput.h>

// Structures
typedef struct {
  unsigned int handle; // FSOUND_SAMPLE* handle cast to int
  unsigned char volume;
  unsigned char padding[3];
} SoundEntry;
extern SoundEntry *g_SoundTable; // DAT_00487874

// Global Variables
extern HWND hWnd_Main;                    // DAT_00489edc
extern LPDIRECTDRAW lpDD;                 // DAT_00489ec8
extern LPDIRECTDRAWSURFACE lpDDS_Primary; // DAT_00489ed8
extern LPDIRECTDRAWSURFACE lpDDS_Back;    // DAT_00489ecc
extern FSOUND_STREAM *lpIntroStream;      // DAT_004806f8
extern BOOL g_bIsActive;                  // DAT_00489ec4
extern BYTE g_GameState;                  // DAT_004877a0
extern LPDIRECTINPUTDEVICE lpDI_Device;   // DAT_00489ec0

// Logic Globals
extern int DAT_0048924c;           // Intro Splash Index
extern unsigned char DAT_004877c8; // Software Buffer Frame Index
extern char DAT_0048769c;          // Loaded Background Index
extern char DAT_004877be;          // Input button flag
extern int DAT_004892a0;           // Memory Tracker / Debug
extern int *DAT_00487880;          // Math Tables / Config
extern void *DAT_00487ab0;         // Math Buffer

// Unknown Globals
extern int DAT_00489296;
extern int DAT_00489297;
extern int DAT_00489298;

// Debug Logging
void Log(const char *format, ...);
#define LOG Log

// Function Prototypes
extern "C" LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam);

// Sound Manager
int Init_Sound_System(void);
void Load_Game_Sounds(void);
void Play_Music(void);
void Stop_All_Sounds(void);
void Cleanup_Sound(void);

// Asset Loader
int Load_Background_To_Buffer(char index); // 0042d710
void *Load_JPEG_Asset(const char *filename, int *width, int *height);

extern int DAT_00481d08; // Width
extern int DAT_00481cfc; // Height
extern int DAT_00481d04; // Channels
extern int DAT_00481d00; // Buffer Size

// Render
void Render_Frame(void);     // 0045d800
void Render_Game_View(void); // 0042f3a0
void Release_DirectDraw_Surfaces(void);
int Init_DirectDraw(int width, int height);

// Memory
void Init_Memory_Pools(void);
void *Mem_Alloc(size_t size);
void Mem_Free(void *ptr);

// State functions
void Game_State_Manager(void);
void Game_Update_Render(void);
void Handle_Menu_State(void);
void Intro_Sequence(void);
void Init_New_Game(void);
void Free_Game_Resources(void);
void FUN_0040e130(void); // Init Sound/Grahpics for Menu

// Font Data
typedef struct {
  int pixel_offset;
  int width;
  int height;
  int unknown;
} FontChar;

extern FontChar Font_Char_Table[1024]; // 4 fonts * 256 chars
extern unsigned char *Font_Pixel_Data; // Large buffer for font pixels
extern void Load_Fonts(void);
extern void Draw_Text_To_Buffer(const char *str, int font_idx, int color_idx,
                                unsigned short *dest_buf, int stride, int x,
                                int max_x, int len);

// System / Optimization
void Init_Game_Config(void); // 004207c0
void Early_Init_Vars(void);  // 0041ead0
int System_Init_Check(void); // 0041d480
void Input_Update(void);
int Init_DirectInput(void);
void Init_Math_Tables(int *buffer, unsigned int count); // 00425780

extern unsigned short *DAT_004877c0; // Software_Buffer (640x480x16b)

#endif // TOU_H
