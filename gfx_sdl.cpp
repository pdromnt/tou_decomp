/* SDL3 implementation of the renderer backend.
 *
 * First migration boundary: SDL wraps the existing native window and presents
 * the recovered 640x480 RGB565 software framebuffer. Window creation, input,
 * audio, and the game loop remain on their verified legacy paths for now. */
#include "tou.h"
#include "render_backend.h"
#include <SDL3/SDL.h>

static SDL_Window *s_Window = NULL;
static SDL_Renderer *s_Renderer = NULL;
static SDL_Texture *s_Texture = NULL;
static int s_TextureWidth = 0;
static int s_TextureHeight = 0;

static void SDLBackend_ReleaseTexture(void)
{
    if (s_Texture != NULL) {
        SDL_DestroyTexture(s_Texture);
        s_Texture = NULL;
    }
    s_TextureWidth = 0;
    s_TextureHeight = 0;
}

static int SDLBackend_Initialize(void *window_handle)
{
    if (window_handle == NULL)
        return 0;
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        LOG("[GFX] SDL video initialization failed: %s\n", SDL_GetError());
        return 0;
    }

    SDL_PropertiesID properties = SDL_CreateProperties();
    if (properties == 0 ||
        !SDL_SetPointerProperty(properties,
            SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, window_handle)) {
        LOG("[GFX] SDL native-window properties failed: %s\n", SDL_GetError());
        if (properties != 0)
            SDL_DestroyProperties(properties);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 0;
    }

    s_Window = SDL_CreateWindowWithProperties(properties);
    SDL_DestroyProperties(properties);
    if (s_Window == NULL) {
        LOG("[GFX] SDL native-window wrapping failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 0;
    }

    s_Renderer = SDL_CreateRenderer(s_Window, NULL);
    if (s_Renderer == NULL) {
        LOG("[GFX] SDL renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(s_Window);
        s_Window = NULL;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 0;
    }

    SDL_SetRenderVSync(s_Renderer, 1);
    LOG("[GFX] SDL renderer initialized: %s\n", SDL_GetRendererName(s_Renderer));
    return 1;
}

static int SDLBackend_Configure(int width, int height)
{
    if (s_Renderer == NULL || width <= 0 || height <= 0)
        return 0;
    if (s_Texture != NULL && s_TextureWidth == width && s_TextureHeight == height)
        return 1;

    SDLBackend_ReleaseTexture();
    s_Texture = SDL_CreateTexture(s_Renderer, SDL_PIXELFORMAT_RGB565,
                                  SDL_TEXTUREACCESS_STREAMING, width, height);
    if (s_Texture == NULL) {
        LOG("[GFX] SDL texture creation failed: %s\n", SDL_GetError());
        return 0;
    }
    SDL_SetTextureScaleMode(s_Texture, SDL_SCALEMODE_PIXELART);
    s_TextureWidth = width;
    s_TextureHeight = height;
    g_SurfaceReady = 2;
    LOG("[GFX] SDL backend configured for %dx%d\n", width, height);
    return 1;
}

static void SDLBackend_Shutdown(void)
{
    SDLBackend_ReleaseTexture();
    if (s_Renderer != NULL) {
        SDL_DestroyRenderer(s_Renderer);
        s_Renderer = NULL;
    }
    if (s_Window != NULL) {
        SDL_DestroyWindow(s_Window);
        s_Window = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void SDLBackend_Restore(void)
{
    g_SurfaceReady = 2;
}

static int SDLBackend_Present(const Framebuffer *framebuffer)
{
    if (framebuffer == NULL || framebuffer->pixels == NULL ||
        framebuffer->width != s_TextureWidth ||
        framebuffer->height != s_TextureHeight ||
        framebuffer->stride < framebuffer->width ||
        s_Renderer == NULL || s_Texture == NULL)
        return 0;

    if (!SDL_UpdateTexture(s_Texture, NULL, framebuffer->pixels,
                           framebuffer->stride * (int)sizeof(uint16_t))) {
        LOG("[GFX] SDL texture upload failed: %s\n", SDL_GetError());
        return 0;
    }

    RECT destination;
    Get_Game_Presentation_Rect(&destination);
    SDL_FRect target = {
        (float)destination.left,
        (float)destination.top,
        (float)(destination.right - destination.left),
        (float)(destination.bottom - destination.top)
    };

    SDL_SetRenderDrawColor(s_Renderer, 0, 0, 0, 255);
    if (!SDL_RenderClear(s_Renderer) ||
        !SDL_RenderTexture(s_Renderer, s_Texture, NULL, &target) ||
        !SDL_RenderPresent(s_Renderer)) {
        LOG("[GFX] SDL presentation failed: %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

static int SDLBackend_ApplyDisplaySettings(int width, int height, int fullscreen)
{
    if (s_Window == NULL || width <= 0 || height <= 0)
        return 0;

    if (fullscreen) {
        if (!SDL_SetWindowFullscreen(s_Window, true)) {
            LOG("[GFX] SDL fullscreen transition failed: %s\n", SDL_GetError());
            return 0;
        }
        SDL_SyncWindow(s_Window);
    } else {
        /* Leaving fullscreen must complete before SDL will accept a window-size
         * request. This also replaces SDL's stale pre-fullscreen dimensions. */
        if (!SDL_SetWindowFullscreen(s_Window, false)) {
            LOG("[GFX] SDL windowed transition failed: %s\n", SDL_GetError());
            return 0;
        }
        SDL_SyncWindow(s_Window);
        if (!SDL_SetWindowSize(s_Window, width, height)) {
            LOG("[GFX] SDL window resize failed: %s\n", SDL_GetError());
            return 0;
        }
        SDL_SetWindowPosition(s_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_SyncWindow(s_Window);
    }

    LOG("[GFX] SDL display mode: %dx%d %s\n", width, height,
        fullscreen ? "fullscreen" : "windowed");
    return 1;
}

/* The recovered game-surface calls only manage presentation-era lifetime.
 * SDL uploads the software framebuffer directly and needs no second surface. */
static int SDLBackend_CreateGameSurface(void)
{
    return s_Renderer != NULL;
}

static void SDLBackend_ReleaseGameSurface(void)
{
}

const RenderBackend g_SdlRenderBackend = {
    "SDL3",
    SDLBackend_Initialize,
    SDLBackend_Configure,
    SDLBackend_Shutdown,
    SDLBackend_Restore,
    SDLBackend_Present,
    SDLBackend_ApplyDisplaySettings,
    SDLBackend_CreateGameSurface,
    SDLBackend_ReleaseGameSurface
};
