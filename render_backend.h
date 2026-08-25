#ifndef TOU_RENDER_BACKEND_H
#define TOU_RENDER_BACKEND_H

#include <stdint.h>

typedef struct Framebuffer {
    uint16_t *pixels;
    int width;
    int height;
    int stride; /* pixels per row */
} Framebuffer;

typedef struct Viewport {
    int left;
    int top;
    int right;
    int bottom;
    int width;
    int height;
    int screen_y;
    int screen_x;
} Viewport;

typedef struct PresentationRect {
    int left;
    int top;
    int right;
    int bottom;
} PresentationRect;

typedef struct RenderBackend {
    const char *name;
    int  (*initialize)(void);
    int  (*configure)(int width, int height);
    void (*shutdown)(void);
    void (*restore)(void);
    int  (*present)(const Framebuffer *framebuffer);
    int  (*create_game_surface)(void);
    void (*release_game_surface)(void);
} RenderBackend;

extern const RenderBackend g_SdlRenderBackend;

int  RenderBackend_Initialize(void);
int  RenderBackend_Configure(int width, int height);
void RenderBackend_Shutdown(void);
void RenderBackend_Restore(void);
int  RenderBackend_Present(const Framebuffer *framebuffer);
int  RenderBackend_CreateGameSurface(void);
void RenderBackend_ReleaseGameSurface(void);
const char *RenderBackend_Name(void);

#endif
