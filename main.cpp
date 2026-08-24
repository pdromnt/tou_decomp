/* SDL-owned application entry point and lifecycle. */
#include "tou.h"
#include "platform.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string.h>

int       g_bIsActive = 0;     /* 00489EC4 */
GameState g_GameState = GAME_STATE_GAMEPLAY; /* 004877A0 */
uint32_t  g_TimerStart = 0;    /* 004892B0 */
int       g_TimerAux = 0;      /* 004892B4 */

static int g_QuitRequested = 0;
static int g_RuntimeShutdown = 0;

static int Has_Argument(int argc, char **argv, const char *argument)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], argument) == 0)
            return 1;
    }
    return 0;
}

void GameState_Transition(GameState next_state)
{
    LOG("[STATE] main %u -> %u\n", (unsigned)g_GameState, (unsigned)next_state);
    g_GameState = next_state;
}

void Request_App_Quit(void)
{
    g_QuitRequested = 1;
}

static void Shutdown_Runtime(void)
{
    if (g_RuntimeShutdown)
        return;
    g_RuntimeShutdown = 1;
    RenderBackend_Shutdown();
    Cleanup_Sound();
}

static void Handle_App_Focus(int active)
{
    g_bIsActive = active ? 1 : 0;
    if (g_bIsActive) {
        if (g_SoundEnabled)
            Audio_SetMuted(0);
        LOG("[FOCUS] activate state=%u page=%u\n",
            (unsigned)g_GameState, (unsigned)DAT_004877a4);
        return;
    }

    if (g_GameState == GAME_STATE_GAMEPLAY && g_SubState == GAMEPLAY_ACTIVE) {
        g_SubState = GAMEPLAY_PAUSED;
        g_NeedsRedraw = 1;
        g_SurfaceReady = 2;
        g_TimerStart = Platform_GetTicks();
        g_TimerAux = 0;
        g_FrameTimer = Platform_GetTicks();
    }
    if (g_SoundEnabled)
        Audio_SetMuted(1);
    LOG("[FOCUS] deactivate state=%u page=%u substate=%u\n",
        (unsigned)g_GameState, (unsigned)DAT_004877a4, (unsigned)g_SubState);
}

static void Handle_Mouse_Motion(int client_x, int client_y)
{
    if (!g_bIsActive || g_InputMode != 0)
        return;
    int mouse_x;
    int mouse_y;
    Client_To_Game_Coordinates(client_x, client_y, &mouse_x, &mouse_y);
    g_MouseDeltaX = mouse_x << 18;
    g_MouseDeltaY = mouse_y << 18;
}

int main(int argc, char **argv)
{
    if (Has_Argument(argc, argv, "--logging"))
        g_LogEnabled = 1;

    SDL_SetAppMetadata("Tunnels of Underworld", "0.4", "fi.iobox.tou");
    Early_Init_Vars();

    if (!Platform_CreateWindow(STR_TITLE, 640, 480)) {
        Platform_ShowError(STR_ERR_RENDER_INIT);
        SDL_Quit();
        return 1;
    }

    LOG("[INIT] System_Init_Check...\n");
    int init_result = System_Init_Check();
    LOG("[INIT] System_Init_Check returned %d\n", init_result);
    if (init_result != 1) {
        Platform_ShowError(init_result == 0
            ? STR_ERR_INIT_FILENOTFOUND : STR_ERR_INIT_NOLEVELS);
        Shutdown_Runtime();
        Platform_DestroyWindow();
        SDL_Quit();
        return 1;
    }

    Apply_Display_Settings();
    LOG("[INIT] Initializing %s renderer...\n", RenderBackend_Name());
    if (!RenderBackend_Initialize()) {
        Platform_ShowError(STR_ERR_RENDER_INIT);
        Shutdown_Runtime();
        Platform_DestroyWindow();
        SDL_Quit();
        return 1;
    }

    Platform_ShowWindow();
    Handle_App_Focus(1);
    g_MouseButtons = 0;
    GameState_Transition(GAME_STATE_INTRO_INIT);

    while (!g_QuitRequested) {
        uint32_t event_deadline = Platform_GetTicks() + 4;
        PlatformEvent event;
        while (Platform_PollEvent(&event)) {
            if (event.type == PLATFORM_EVENT_QUIT) {
                Request_App_Quit();
            } else if (event.type == PLATFORM_EVENT_FOCUS_GAINED) {
                Handle_App_Focus(1);
            } else if (event.type == PLATFORM_EVENT_FOCUS_LOST) {
                Handle_App_Focus(0);
            } else if (event.type == PLATFORM_EVENT_MOUSE_MOTION) {
                Handle_Mouse_Motion(event.x, event.y);
            }
            if (g_QuitRequested || Platform_GetTicks() >= event_deadline)
                break;
        }

        if (g_QuitRequested)
            break;

        if (!g_bIsActive) {
            Platform_Delay(1);
            continue;
        }

        Input_UpdateKeyboardState();
        int client_x;
        int client_y;
        if (Platform_GetMousePosition(&client_x, &client_y))
            Handle_Mouse_Motion(client_x, client_y);

        g_ProcessInput = 1;
        if (g_GameState == GAME_STATE_GAMEPLAY)
            Game_Update_Render();
        else
            Game_State_Manager();
    }

    Save_Options_Config();
    Shutdown_Runtime();
    Platform_DestroyWindow();
    SDL_Quit();
    return 0;
}
