/* DirectDraw implementation of the renderer backend.
 * The game renderer itself only produces a 640x480 RGB565 Framebuffer. */
#include "tou.h"
#include "render_backend.h"
#include <string.h>

static LPDIRECTDRAW s_DirectDraw = NULL;
static LPDIRECTDRAWSURFACE s_Primary = NULL;
static LPDIRECTDRAWSURFACE s_Offscreen = NULL;
static LPDIRECTDRAWSURFACE s_Presentation = NULL;
static LPDIRECTDRAWSURFACE s_GameSurface = NULL;
static HWND s_Window = NULL;
static int s_PresentationWidth = 0;
static int s_PresentationHeight = 0;

static void DDraw_ReleaseSurfaces(void)
{
    if (s_GameSurface != NULL) {
        s_GameSurface->Release();
        s_GameSurface = NULL;
    }
    if (s_Presentation != NULL) {
        s_Presentation->Release();
        s_Presentation = NULL;
    }
    if (s_Offscreen != NULL) {
        s_Offscreen->Release();
        s_Offscreen = NULL;
    }
    if (s_Primary != NULL) {
        s_Primary->Release();
        s_Primary = NULL;
    }
    s_PresentationWidth = 0;
    s_PresentationHeight = 0;
}

static int DDraw_EnsurePresentationSurface(int width, int height)
{
    if (width <= 0 || height <= 0 || s_DirectDraw == NULL)
        return 0;
    if (s_Presentation != NULL &&
        s_PresentationWidth == width && s_PresentationHeight == height)
        return 1;

    if (s_Presentation != NULL) {
        s_Presentation->Release();
        s_Presentation = NULL;
    }

    DDSURFACEDESC desc = {};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    desc.dwWidth = width;
    desc.dwHeight = height;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    if (s_DirectDraw->CreateSurface(&desc, &s_Presentation, NULL) != DD_OK) {
        s_PresentationWidth = 0;
        s_PresentationHeight = 0;
        return 0;
    }
    s_PresentationWidth = width;
    s_PresentationHeight = height;
    return 1;
}

static int DDraw_Initialize(void *window_handle)
{
    s_Window = (HWND)window_handle;
    HRESULT hr = DirectDrawCreate(NULL, &s_DirectDraw, NULL);
    LOG("[GFX] DirectDrawCreate returned 0x%08lX\n", (unsigned long)hr);
    if (hr != DD_OK || s_DirectDraw == NULL)
        return 0;

    hr = s_DirectDraw->SetCooperativeLevel(s_Window, DDSCL_NORMAL);
    LOG("[GFX] SetCooperativeLevel returned 0x%08lX\n", (unsigned long)hr);
    if (hr != DD_OK) {
        s_DirectDraw->Release();
        s_DirectDraw = NULL;
        return 0;
    }
    return 1;
}

static int DDraw_Configure(int width, int height)
{
    if (s_DirectDraw == NULL)
        return 0;

    DDraw_ReleaseSurfaces();
    DDSURFACEDESC desc = {};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS;
    desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    HRESULT hr = s_DirectDraw->CreateSurface(&desc, &s_Primary, NULL);
    if (hr != DD_OK)
        return 0;

    LPDIRECTDRAWCLIPPER clipper = NULL;
    hr = s_DirectDraw->CreateClipper(0, &clipper, NULL);
    if (hr != DD_OK) {
        DDraw_ReleaseSurfaces();
        return 0;
    }
    clipper->SetHWnd(0, s_Window);
    s_Primary->SetClipper(clipper);
    clipper->Release();

    desc = {};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    desc.dwWidth = width;
    desc.dwHeight = height;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    hr = s_DirectDraw->CreateSurface(&desc, &s_Offscreen, NULL);
    if (hr != DD_OK) {
        DDraw_ReleaseSurfaces();
        return 0;
    }

    g_SurfaceReady = 2;
    LOG("[GFX] DirectDraw backend configured for %dx%d\n", width, height);
    return 1;
}

static void DDraw_Shutdown(void)
{
    DDraw_ReleaseSurfaces();
    if (s_DirectDraw != NULL) {
        s_DirectDraw->Release();
        s_DirectDraw = NULL;
    }
    s_Window = NULL;
}

static void DDraw_Restore(void)
{
    if (s_Primary != NULL) s_Primary->Restore();
    if (s_Offscreen != NULL) s_Offscreen->Restore();
    if (s_Presentation != NULL) s_Presentation->Restore();
    if (s_GameSurface != NULL) s_GameSurface->Restore();
    g_SurfaceReady = 2;
}

static int DDraw_Present(const Framebuffer *framebuffer)
{
    if (framebuffer == NULL || framebuffer->pixels == NULL ||
        framebuffer->width != 640 || framebuffer->height != 480 ||
        s_Offscreen == NULL || s_Primary == NULL)
        return 0;

    DDSURFACEDESC desc = {};
    desc.dwSize = sizeof(desc);
    HRESULT hr;
    do {
        hr = s_Offscreen->Lock(NULL, &desc, DDLOCK_WAIT, NULL);
        if (hr == DDERR_SURFACELOST) {
            DDraw_Restore();
            return 0;
        }
    } while (hr == DDERR_WASSTILLDRAWING);
    if (hr != DD_OK)
        return 0;

    for (int y = 0; y < framebuffer->height; y++) {
        unsigned int *dst = (unsigned int *)((char *)desc.lpSurface + y * desc.lPitch);
        const uint16_t *src = framebuffer->pixels + y * framebuffer->stride;
        for (int x = 0; x < framebuffer->width; x++) {
            uint16_t pixel = src[x];
            unsigned char r = (pixel >> 11) & 0x1F;
            unsigned char g = (pixel >> 5) & 0x3F;
            unsigned char b = pixel & 0x1F;
            r = (r << 3) | (r >> 2);
            g = (g << 2) | (g >> 4);
            b = (b << 3) | (b >> 2);
            dst[x] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
        }
    }
    s_Offscreen->Unlock(NULL);

    RECT client;
    GetClientRect(s_Window, &client);
    int client_width = client.right - client.left;
    int client_height = client.bottom - client.top;
    if (!DDraw_EnsurePresentationSurface(client_width, client_height))
        return 0;

    DDBLTFX fill = {};
    fill.dwSize = sizeof(fill);
    fill.dwFillColor = 0;
    s_Presentation->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fill);

    RECT destination;
    Get_Game_Presentation_Rect(&destination);
    HDC source_dc = NULL;
    HDC presentation_dc = NULL;
    HRESULT source_hr = s_Offscreen->GetDC(&source_dc);
    HRESULT presentation_hr = s_Presentation->GetDC(&presentation_dc);
    if (source_hr == DD_OK && presentation_hr == DD_OK) {
        int destination_width = destination.right - destination.left;
        int destination_height = destination.bottom - destination.top;
        int scale_x = destination_width / framebuffer->width;
        int scale_y = destination_height / framebuffer->height;
        int pixel_perfect = scale_x >= 1 && scale_x == scale_y &&
            destination_width == framebuffer->width * scale_x &&
            destination_height == framebuffer->height * scale_y;
        SetStretchBltMode(presentation_dc, pixel_perfect ? COLORONCOLOR : HALFTONE);
        SetBrushOrgEx(presentation_dc, 0, 0, NULL);
        StretchBlt(presentation_dc, destination.left, destination.top,
            destination_width, destination_height, source_dc, 0, 0,
            framebuffer->width, framebuffer->height, SRCCOPY);
    } else {
        if (presentation_hr == DD_OK) {
            s_Presentation->ReleaseDC(presentation_dc);
            presentation_dc = NULL;
        }
        if (source_hr == DD_OK) {
            s_Offscreen->ReleaseDC(source_dc);
            source_dc = NULL;
        }
        RECT source = {0, 0, framebuffer->width, framebuffer->height};
        s_Presentation->Blt(&destination, s_Offscreen, &source, DDBLT_WAIT, NULL);
    }
    if (presentation_dc != NULL) s_Presentation->ReleaseDC(presentation_dc);
    if (source_dc != NULL) s_Offscreen->ReleaseDC(source_dc);

    RECT source = {0, 0, client_width, client_height};
    RECT screen_destination = source;
    POINT origin = {0, 0};
    ClientToScreen(s_Window, &origin);
    OffsetRect(&screen_destination, origin.x, origin.y);
    do {
        hr = s_Primary->Blt(&screen_destination, s_Presentation, &source, DDBLT_WAIT, NULL);
        if (hr == DDERR_SURFACELOST) {
            DDraw_Restore();
            return 0;
        }
    } while (hr == DDERR_WASSTILLDRAWING);
    return hr == DD_OK;
}

static int DDraw_CreateGameSurface(void)
{
    if (s_DirectDraw == NULL)
        return 0;
    if (s_GameSurface != NULL)
        return 1;
    DDSURFACEDESC desc = {};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    desc.dwHeight = 480;
    desc.dwWidth = 640;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
    return s_DirectDraw->CreateSurface(&desc, &s_GameSurface, NULL) == DD_OK;
}

static void DDraw_ReleaseGameSurface(void)
{
    if (s_GameSurface != NULL) {
        s_GameSurface->Release();
        s_GameSurface = NULL;
    }
}

const RenderBackend g_DirectDrawRenderBackend = {
    "DirectDraw",
    DDraw_Initialize,
    DDraw_Configure,
    DDraw_Shutdown,
    DDraw_Restore,
    DDraw_Present,
    DDraw_CreateGameSurface,
    DDraw_ReleaseGameSurface
};
