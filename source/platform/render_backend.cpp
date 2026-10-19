#include "render_backend.h"
static const RenderBackend *s_Backend = &g_SdlRenderBackend;

int RenderBackend_Initialize(void)
{
    return s_Backend != nullptr && s_Backend->initialize != nullptr
        ? s_Backend->initialize() : 0;
}

int RenderBackend_Configure(int width, int height)
{
    return s_Backend != nullptr && s_Backend->configure != nullptr
        ? s_Backend->configure(width, height) : 0;
}

void RenderBackend_Shutdown(void)
{
    if (s_Backend != nullptr && s_Backend->shutdown != nullptr)
        s_Backend->shutdown();
}

void RenderBackend_Restore(void)
{
    if (s_Backend != nullptr && s_Backend->restore != nullptr)
        s_Backend->restore();
}

int RenderBackend_Present(const Framebuffer *framebuffer)
{
    return s_Backend != nullptr && s_Backend->present != nullptr
        ? s_Backend->present(framebuffer) : 0;
}

int RenderBackend_CreateGameSurface(void)
{
    return s_Backend != nullptr && s_Backend->create_game_surface != nullptr
        ? s_Backend->create_game_surface() : 0;
}

void RenderBackend_ReleaseGameSurface(void)
{
    if (s_Backend != nullptr && s_Backend->release_game_surface != nullptr)
        s_Backend->release_game_surface();
}

const char *RenderBackend_Name(void)
{
    return s_Backend != nullptr ? s_Backend->name : "none";
}
