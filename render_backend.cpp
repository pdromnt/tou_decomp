#include "render_backend.h"
#include <string.h>

#ifdef TOU_HAS_SDL
static const RenderBackend *s_Backend = &g_SdlRenderBackend;
#else
static const RenderBackend *s_Backend = &g_DirectDrawRenderBackend;
#endif

int RenderBackend_SelectByName(const char *name)
{
    if (name == NULL)
        return 0;
#ifdef TOU_HAS_SDL
    if (strcmp(name, "sdl") == 0) {
        s_Backend = &g_SdlRenderBackend;
        return 1;
    }
#endif
    if (strcmp(name, "directdraw") == 0) {
        s_Backend = &g_DirectDrawRenderBackend;
        return 1;
    }
    return 0;
}

int RenderBackend_Initialize(void *window_handle)
{
    return s_Backend != NULL && s_Backend->initialize != NULL
        ? s_Backend->initialize(window_handle) : 0;
}

int RenderBackend_Configure(int width, int height)
{
    return s_Backend != NULL && s_Backend->configure != NULL
        ? s_Backend->configure(width, height) : 0;
}

void RenderBackend_Shutdown(void)
{
    if (s_Backend != NULL && s_Backend->shutdown != NULL)
        s_Backend->shutdown();
}

void RenderBackend_Restore(void)
{
    if (s_Backend != NULL && s_Backend->restore != NULL)
        s_Backend->restore();
}

int RenderBackend_Present(const Framebuffer *framebuffer)
{
    return s_Backend != NULL && s_Backend->present != NULL
        ? s_Backend->present(framebuffer) : 0;
}

int RenderBackend_CreateGameSurface(void)
{
    return s_Backend != NULL && s_Backend->create_game_surface != NULL
        ? s_Backend->create_game_surface() : 0;
}

void RenderBackend_ReleaseGameSurface(void)
{
    if (s_Backend != NULL && s_Backend->release_game_surface != NULL)
        s_Backend->release_game_surface();
}

const char *RenderBackend_Name(void)
{
    return s_Backend != NULL ? s_Backend->name : "none";
}
