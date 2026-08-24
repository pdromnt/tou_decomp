/* SDL-owned window and event boundary.
 *
 * The native window handle remains exposed temporarily for DirectDraw fallback,
 * Win32 dialogs, and a few focus compatibility paths. Those consumers can
 * disappear independently in later passes. */
#include "tou.h"
#include "platform.h"
#include <SDL3/SDL.h>

static SDL_Window *s_PlatformWindow = NULL;
static void *s_NativeWindow = NULL;

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

#ifdef _WIN32
    SDL_PropertiesID properties = SDL_GetWindowProperties(s_PlatformWindow);
    s_NativeWindow = SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#else
    s_NativeWindow = NULL;
#endif
    if (s_NativeWindow == NULL) {
        LOG("[PLATFORM] SDL did not expose a native window handle: %s\n",
            SDL_GetError());
        Platform_DestroyWindow();
        return 0;
    }

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

void Platform_DestroyWindow(void)
{
    s_NativeWindow = NULL;
    if (s_PlatformWindow != NULL) {
        SDL_DestroyWindow(s_PlatformWindow);
        s_PlatformWindow = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void *Platform_GetNativeWindowHandle(void)
{
    return s_NativeWindow;
}

void *Platform_GetSdlWindow(void)
{
    return s_PlatformWindow;
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
