/* SDL-owned window and event boundary. */
#include "tou.h"
#include "platform.h"
#include <SDL3/SDL.h>
#include <string>

#if defined(_WIN32)
#include <direct.h>
#define TOU_CHDIR _chdir
#else
#include <unistd.h>
#define TOU_CHDIR chdir
#endif

static SDL_Window *s_PlatformWindow = NULL;

int Platform_SetRuntimeDirectory(void)
{
    const char *base_path = SDL_GetBasePath();
    if (base_path == NULL)
        return 0;

    std::string runtime_path(base_path);
#if defined(__APPLE__)
    /* SDL returns Contents/MacOS for an app bundle. Assets and the deliberately
     * app-local settings.json live together in Contents/Resources. */
    runtime_path += "../Resources";
#endif
    if (TOU_CHDIR(runtime_path.c_str()) != 0) {
        LOG("[PLATFORM] Unable to use runtime directory: %s\n",
            runtime_path.c_str());
        return 0;
    }
    return 1;
}

int Platform_CreateWindow(const char *title, int width, int height)
{
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        LOG("[PLATFORM] SDL video initialization failed: %s\n", SDL_GetError());
        return 0;
    }

    s_PlatformWindow = SDL_CreateWindow(title, width, height, SDL_WINDOW_HIDDEN);
    if (s_PlatformWindow == NULL) {
        LOG("[PLATFORM] SDL window creation failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 0;
    }

    SDL_SetWindowPosition(s_PlatformWindow, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);
    SDL_HideCursor();

    LOG("[PLATFORM] SDL window created\n");
    return 1;
}

void Platform_ShowWindow(void)
{
    if (s_PlatformWindow != NULL) {
        SDL_ShowWindow(s_PlatformWindow);
        SDL_RaiseWindow(s_PlatformWindow);
        SDL_SyncWindow(s_PlatformWindow);
    }
}

void Platform_SetWindowTitle(const char *title)
{
    if (s_PlatformWindow != NULL && title != NULL)
        SDL_SetWindowTitle(s_PlatformWindow, title);
}

void Platform_DestroyWindow(void)
{
    if (s_PlatformWindow != NULL) {
        SDL_DestroyWindow(s_PlatformWindow);
        s_PlatformWindow = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void *Platform_GetSdlWindow(void)
{
    return s_PlatformWindow;
}

int Platform_GetWindowSize(int *width, int *height)
{
    if (s_PlatformWindow == NULL)
        return 0;
    return SDL_GetWindowSize(s_PlatformWindow, width, height) ? 1 : 0;
}

int Platform_PollEvent(PlatformEvent *event)
{
    if (event == NULL)
        return 0;

    SDL_Event sdl_event;
    if (!SDL_PollEvent(&sdl_event)) {
        event->type = PLATFORM_EVENT_NONE;
        return 0;
    }

    event->type = PLATFORM_EVENT_NONE;
    event->x = 0;
    event->y = 0;
    switch (sdl_event.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        event->type = PLATFORM_EVENT_QUIT;
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        event->type = PLATFORM_EVENT_FOCUS_GAINED;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        event->type = PLATFORM_EVENT_FOCUS_LOST;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        event->type = PLATFORM_EVENT_MOUSE_MOTION;
        event->x = (int)sdl_event.motion.x;
        event->y = (int)sdl_event.motion.y;
        break;
    default:
        break;
    }
    return 1;
}

void Platform_ShowError(const char *message)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Tunnels of Underworld",
                             message != NULL ? message : "Unknown error",
                             s_PlatformWindow);
}

uint32_t Platform_GetTicks(void)
{
    /* Preserve the original WinMM clock's 32-bit millisecond wraparound. */
    return (uint32_t)SDL_GetTicks();
}

void Platform_Delay(uint32_t milliseconds)
{
    SDL_Delay(milliseconds);
}

int Platform_GetMousePosition(int *x, int *y)
{
    if (s_PlatformWindow == NULL || SDL_GetMouseFocus() != s_PlatformWindow)
        return 0;
    float mouse_x;
    float mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    if (x != NULL) *x = (int)mouse_x;
    if (y != NULL) *y = (int)mouse_y;
    return 1;
}

int Platform_ApplyDisplaySettings(int width, int height, int fullscreen)
{
    if (s_PlatformWindow == NULL || width <= 0 || height <= 0)
        return 0;

    if (fullscreen) {
        if (!SDL_SetWindowFullscreen(s_PlatformWindow, true)) {
            LOG("[PLATFORM] SDL fullscreen transition failed: %s\n", SDL_GetError());
            return 0;
        }
        SDL_SyncWindow(s_PlatformWindow);
    } else {
        if (!SDL_SetWindowFullscreen(s_PlatformWindow, false)) {
            LOG("[PLATFORM] SDL windowed transition failed: %s\n", SDL_GetError());
            return 0;
        }
        SDL_SyncWindow(s_PlatformWindow);
        if (!SDL_SetWindowSize(s_PlatformWindow, width, height)) {
            LOG("[PLATFORM] SDL window resize failed: %s\n", SDL_GetError());
            return 0;
        }
        SDL_SetWindowPosition(s_PlatformWindow, SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED);
        SDL_SyncWindow(s_PlatformWindow);
    }

    LOG("[PLATFORM] SDL display mode: %dx%d %s\n", width, height,
        fullscreen ? "fullscreen" : "windowed");
    return 1;
}
