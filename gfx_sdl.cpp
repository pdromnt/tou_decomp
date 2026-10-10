/* SDL3 implementation of the renderer backend.
 *
 * Presents the recovered 640x480 RGB565 software framebuffer through the
 * SDL-owned window. */
#include "tou.h"
#include "render_backend.h"
#include "platform.h"
#include "gfx_shader.h"
#include <SDL3/SDL.h>

static SDL_Window *s_Window = NULL;
static SDL_Renderer *s_Renderer = NULL;
static SDL_Texture *s_Texture = NULL;
static int s_TextureWidth = 0;
static int s_TextureHeight = 0;
static int s_Scaling = -1;

static void SDLBackend_ReleaseTexture(void)
{
    if (s_Texture != NULL) {
        SDL_DestroyTexture(s_Texture);
        s_Texture = NULL;
    }
    s_TextureWidth = 0;
    s_TextureHeight = 0;
    s_Scaling = -1;
}

static int SDLBackend_Initialize(void)
{
    s_Window = (SDL_Window *)Platform_GetSdlWindow();
    if (s_Window == NULL) {
        LOG("[GFX] SDL platform window is unavailable\n");
        return 0;
    }

    if ((SDL_GetWindowFlags(s_Window) & SDL_WINDOW_OPENGL) != 0 &&
        ShaderPresentation_Initialize(s_Window))
        return 1;
    if ((SDL_GetWindowFlags(s_Window) & SDL_WINDOW_OPENGL) != 0)
        LOG("[GFX] Shader initialization failed; using normal SDL renderer\n");

    s_Renderer = SDL_CreateRenderer(s_Window, NULL);
    if (s_Renderer == NULL) {
        LOG("[GFX] SDL renderer creation failed: %s\n", SDL_GetError());
        s_Window = NULL;
        return 0;
    }

    SDL_SetRenderVSync(s_Renderer, 1);
    LOG("[GFX] SDL renderer initialized: %s\n", SDL_GetRendererName(s_Renderer));
    return 1;
}

static int SDLBackend_Configure(int width, int height)
{
    if (ShaderPresentation_Available()) {
        g_SurfaceReady = 2;
        return width > 0 && height > 0;
    }
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
    s_TextureWidth = width;
    s_TextureHeight = height;
    g_SurfaceReady = 2;
    LOG("[GFX] SDL backend configured for %dx%d\n", width, height);
    return 1;
}

static void SDLBackend_Shutdown(void)
{
    ShaderPresentation_Shutdown();
    SDLBackend_ReleaseTexture();
    if (s_Renderer != NULL) {
        SDL_DestroyRenderer(s_Renderer);
        s_Renderer = NULL;
    }
    s_Window = NULL;
}

static void SDLBackend_Restore(void)
{
    g_SurfaceReady = 2;
}

static int SDLBackend_Present(const Framebuffer *framebuffer)
{
    if (ShaderPresentation_Available())
        return ShaderPresentation_Present(framebuffer) ? 1 : 0;
    if (framebuffer == NULL || framebuffer->pixels == NULL ||
        framebuffer->width != s_TextureWidth ||
        framebuffer->height != s_TextureHeight ||
        framebuffer->stride < framebuffer->width ||
        s_Renderer == NULL || s_Texture == NULL)
        return 0;

    if (s_Scaling != g_PresentationScaling) {
        SDL_ScaleMode mode = SDL_SCALEMODE_PIXELART;
        if (g_PresentationScaling == PRESENTATION_CRISP) mode = SDL_SCALEMODE_NEAREST;
        else if (g_PresentationScaling == PRESENTATION_SMOOTH) mode = SDL_SCALEMODE_LINEAR;
        if (!SDL_SetTextureScaleMode(s_Texture, mode)) {
            LOG("[GFX] Could not set scaling filter: %s\n", SDL_GetError());
            return 0;
        }
        s_Scaling = g_PresentationScaling;
    }

    if (!SDL_UpdateTexture(s_Texture, NULL, framebuffer->pixels,
                           framebuffer->stride * (int)sizeof(uint16_t))) {
        LOG("[GFX] SDL texture upload failed: %s\n", SDL_GetError());
        return 0;
    }

    PresentationRect destination;
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

/* The recovered game-surface calls only manage presentation-era lifetime.
 * SDL uploads the software framebuffer directly and needs no second surface. */
static int SDLBackend_CreateGameSurface(void)
{
    return s_Renderer != NULL || ShaderPresentation_Available();
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
    SDLBackend_CreateGameSurface,
    SDLBackend_ReleaseGameSurface
};
