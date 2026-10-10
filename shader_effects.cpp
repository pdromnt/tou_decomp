/* Optional presentation metadata only. Never changes the software frame,
 * simulation, RNG, or effect pools. Alpha identifies visible viewport pixels;
 * zero blocks glow sources and destinations (LOS, HUD, borders, overlays). */
#include <algorithm>
#include <cstring>
#include <cmath>
#include "tou.h"
#include "gfx_shader.h"
#include "shader_effects.h"
#include "platform.h"

namespace {
unsigned char glow[640 * 480 * 4];
unsigned char flames[640 * 480 * 4];
bool hasFlames = false;
struct ExhaustPlume { int x, y, owner, vx, vy, brightness; };
ExhaustPlume plumes[ENTITY_ACTIVE_CAPACITY + PARTICLE_CAPACITY];
int plumeCount = 0;
uint16_t snapshot[640 * 480];
bool exhaustPixels[640 * 480];
// Presentation-only ownership: never part of packed gameplay records/snapshots.
unsigned char exhaustOwners[PARTICLE_CAPACITY] = {};
bool active = false;
bool hasEmission = false;
bool reportedEmission = false;
bool snapshotValid = false;
int left = 0, top = 0, right = 0, bottom = 0;
float viewports[16] = {};
struct Shockwave { int x, y; uint32_t started; bool live; };
Shockwave waves[64] = {};
unsigned int nextWave = 0;
float shockwaves[4 * 16 * 4] = {};
bool hasShockwaves = false;

void Block(int x, int y)
{
    if (x < left || x >= right || y < top || y >= bottom) return;
    unsigned char *pixel = glow + (y * 640 + x) * 4;
    pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
    memset(flames + (y * 640 + x) * 4, 0, 4);
}
}

void ShaderEffects_BeginFrame(void)
{
    active = g_BetaShaderEffects != 0 && ShaderPresentation_Available() &&
        g_GameState == GAME_STATE_GAMEPLAY && g_SubState == GAMEPLAY_ACTIVE;
    snapshotValid = false;
    hasEmission = false;
    hasFlames = false;
    hasShockwaves = false;
    memset(shockwaves, 0, sizeof(shockwaves));
    if (!active) memset(waves, 0, sizeof(waves));
    memset(viewports, 0, sizeof(viewports));
    if (active) {
        memset(glow, 0, sizeof(glow));
        memset(flames, 0, sizeof(flames));
        memset(exhaustPixels, 0, sizeof(exhaustPixels));
    }
}

void ShaderEffects_BeginViewport(int index)
{
    if (!active) return;
    plumeCount = 0;
    left = std::max(0, g_Viewport.screen_x);
    top = std::max(0, g_Viewport.screen_y);
    right = std::min(640, g_Viewport.screen_x + g_Viewport.width);
    bottom = std::min(480, g_Viewport.screen_y + g_Viewport.height);
    snapshotValid = false;
    // Distinct IDs prevent blur taps from crossing split-screen boundaries.
    if (index < 0 || index >= 4) return;
    viewports[index * 4] = (float)left / 640.0f;
    viewports[index * 4 + 1] = (float)top / 480.0f;
    viewports[index * 4 + 2] = (float)right / 640.0f;
    viewports[index * 4 + 3] = (float)bottom / 480.0f;
    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x)
            glow[(y * 640 + x) * 4 + 3] = (unsigned char)(index + 1);
    const uint32_t now = Platform_GetTicks();
    int count = 0;
    // Newest first, at most 16 visible waves per viewport; no gameplay limit.
    for (unsigned int n = 0; n < 64 && count < 16; ++n) {
        Shockwave &wave = waves[(nextWave + 63 - n) % 64];
        if (!wave.live) continue;
        const uint32_t age = now - wave.started;
        if (age >= 650) { wave.live = false; continue; }
        const int sx = wave.x - DAT_004806dc + g_Viewport.screen_x;
        const int sy = wave.y - DAT_004806e0 + g_Viewport.screen_y;
        if (sx < left || sx >= right || sy < top || sy >= bottom) continue;
        const float t = (float)age / 650.0f;
        float *out = shockwaves + (index * 16 + count++) * 4;
        out[0] = (float)sx + 0.5f;
        out[1] = (float)sy + 0.5f;
        out[2] = 8.0f + 88.0f * t;
        out[3] = 8.0f * std::min(1.0f, (float)age / 45.0f) * (1.0f - t);
        hasShockwaves = true;
    }
}

void ShaderEffects_Snapshot(const Framebuffer *frame)
{
    if (!active || !frame || frame->width != 640 || frame->height != 480 ||
        frame->stride < 640 || right <= left || bottom <= top) return;
    for (int y = top; y < bottom; ++y)
    {
        memcpy(snapshot + y * 640 + left, frame->pixels + y * frame->stride + left,
               (right - left) * sizeof(uint16_t));
        memset(exhaustPixels + y * 640 + left, 0, (right - left) * sizeof(bool));
    }
    snapshotValid = true;
}

void ShaderEffects_Capture(const Framebuffer *frame)
{
    if (!active || !snapshotValid) return;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            if (exhaustPixels[y * 640 + x]) continue; // Already captured in team colour.
            const uint16_t before = snapshot[y * 640 + x];
            const uint16_t after = frame->pixels[y * frame->stride + x];
            if (before == after) continue;
            const int r = ((after >> 11) & 31) * 255 / 31;
            const int g = ((after >> 5) & 63) * 255 / 63;
            const int b = (after & 31) * 255 / 31;
            if (std::max(r, std::max(g, b)) < 170) continue;
            // Only newly added light, not the bright terrain underneath it.
            const int dr = std::max(0, r - ((before >> 11) & 31) * 255 / 31);
            const int dg = std::max(0, g - ((before >> 5) & 63) * 255 / 63);
            const int db = std::max(0, b - (before & 31) * 255 / 31);
            if (std::max(dr, std::max(dg, db)) < 24) continue;
            unsigned char *pixel = glow + (y * 640 + x) * 4;
            if (!pixel[3]) continue;
            pixel[0] = std::max(pixel[0], (unsigned char)dr);
            pixel[1] = std::max(pixel[1], (unsigned char)dg);
            pixel[2] = std::max(pixel[2], (unsigned char)db);
            hasEmission = true;
            if (!reportedEmission) {
                LOG("[SHADER] Beta glow mask captured visible gameplay emission\n");
                reportedEmission = true;
            }
        }
    }
    snapshotValid = false;
}

void ShaderEffects_SetVisibility(int x, int y, unsigned char visibility)
{
    // Conservative beta: partially obscured pixels receive no added glow either.
    if (active && visibility != 0) Block(x, y);
}

void ShaderEffects_ProtectHud(const Framebuffer *frame)
{
    if (!active || !snapshotValid) return;
    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x)
            if (snapshot[y * 640 + x] != frame->pixels[y * frame->stride + x])
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) Block(x + dx, y + dy);
    snapshotValid = false;
}

const unsigned char *ShaderEffects_GlowPixels(void) { return active && (hasEmission || hasShockwaves) ? glow : nullptr; }
const float *ShaderEffects_ViewportBounds(void) { return viewports; }

bool ShaderEffects_Active(void) { return active; }

int ShaderEffects_ExhaustPixel(int x, int y, int owner, unsigned short before, unsigned short after)
{
    if (!active || owner < 0 || owner >= GAMEPLAY_PLAYER_CAPACITY ||
        !DAT_00487810 || x < left || x >= right || y < top || y >= bottom) return 0;
    const int team = ((const PlayerData *)DAT_00487810)[owner].team;
    if (team < 0 || team >= 4) return 0;
    const int dr = ((after >> 11) & 31) * 255 / 31 - ((before >> 11) & 31) * 255 / 31;
    const int dg = ((after >> 5) & 63) * 255 / 63 - ((before >> 5) & 63) * 255 / 63;
    const int db = (after & 31) * 255 / 31 - (before & 31) * 255 / 31;
    const int brightness = std::max(dr, std::max(dg, db));
    if (brightness <= 0) return 0;
    unsigned char *pixel = glow + (y * 640 + x) * 4;
    if (!pixel[3]) return 0;
    const unsigned short color = DAT_00483838[team]; // X1R5G5B5, not RGB565.
    const int rgb[3] = {(color >> 10) & 31, (color >> 5) & 31, color & 31};
    const int peak = std::max(1, std::max(rgb[0], std::max(rgb[1], rgb[2])));
    for (int c = 0; c < 3; ++c)
        pixel[c] = std::max(pixel[c], (unsigned char)(rgb[c] * brightness * 3 / (peak * 5)));
    exhaustPixels[y * 640 + x] = true;
    hasEmission = true;
    return brightness;
}

// A narrow, tapered emission layer preserves the flame shape separately from
// the wide Gaussian bloom. One plume per rendered particle, not per source pixel.
static void DrawExhaustPlume(int x, int y, int owner, int velocityX, int velocityY, int brightness)
{
    if (!active || brightness < 24 || !DAT_00487810 || owner < 0 ||
        owner >= GAMEPLAY_PLAYER_CAPACITY || x < left || x >= right || y < top || y >= bottom) return;
    const PlayerData &player = ((const PlayerData *)DAT_00487810)[owner];
    if (player.team >= 4 || !glow[(y * 640 + x) * 4 + 3]) return;
    float dx = (float)velocityX - (float)player.velocity_x;
    float dy = (float)velocityY - (float)player.velocity_y;
    float length = std::sqrt(dx * dx + dy * dy);
    if (length < 1.0f) {
        const int *lut = (const int *)DAT_00487ab0;
        if (!lut) return;
        dx = -(float)lut[player.heading & 0x7ff];
        dy = -(float)lut[(player.heading + 0x200) & 0x7ff];
        length = std::sqrt(dx * dx + dy * dy);
    }
    if (length < 1.0f) return;
    dx /= length; dy /= length;
    const float strength = (float)brightness / 255.0f;
    const float plumeLength = 8.0f + 5.0f * strength;
    const float phase = (float)(Platform_GetTicks() % 10000) * 0.018f + x * 0.13f + y * 0.07f;
    const unsigned short color = DAT_00483838[player.team];
    const float rgb[3] = {(float)((color >> 10) & 31), (float)((color >> 5) & 31), (float)(color & 31)};
    const float peak = std::max(1.0f, std::max(rgb[0], std::max(rgb[1], rgb[2])));
    const int minX = std::max(left, (int)std::floor(std::min((float)x, x + dx * plumeLength) - 5));
    const int maxX = std::min(right - 1, (int)std::ceil(std::max((float)x, x + dx * plumeLength) + 5));
    const int minY = std::max(top, (int)std::floor(std::min((float)y, y + dy * plumeLength) - 5));
    const int maxY = std::min(bottom - 1, (int)std::ceil(std::max((float)y, y + dy * plumeLength) + 5));
    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            if (!glow[(py * 640 + px) * 4 + 3]) continue;
            const float along = (px - x) * dx + (py - y) * dy;
            if (along < -2.0f || along > plumeLength) continue;
            const float t = std::max(0.0f, along / plumeLength);
            const float curl = std::sin(t * 6.0f + phase) * t * 0.9f;
            const float across = -(px - x) * dy + (py - y) * dx - curl;
            const float width = (1.5f + strength) * (1.0f - t * 0.8f);
            const float edge = std::exp(-2.0f * across * across / (width * width));
            const float fade = (1.0f - t) * (1.0f - t) * std::exp(-0.5f * std::min(along, 0.0f) * std::min(along, 0.0f));
            const float heat = edge * fade * strength;
            if (heat < 0.01f) continue;
            const float core = edge * edge * (1.0f - t) * 0.35f;
            unsigned char *pixel = flames + (py * 640 + px) * 4;
            for (int c = 0; c < 3; ++c) {
                const float tint = rgb[c] / peak;
                const unsigned char value = (unsigned char)(255.0f * heat * (tint + (1.0f - tint) * core));
                pixel[c] = std::max(pixel[c], value);
            }
            hasFlames = true;
        }
    }
}
void ShaderEffects_ExhaustPlume(int x, int y, int owner, int velocityX, int velocityY, int brightness)
{
    if (!active || brightness < 24 || plumeCount >= ENTITY_ACTIVE_CAPACITY + PARTICLE_CAPACITY) return;
    plumes[plumeCount++] = {x, y, owner, velocityX, velocityY, brightness};
}
void ShaderEffects_BuildExhaustPlumes(void)
{
    if (!active) return;
    // Build after LOS: hidden origins must not emit a visible flame beyond fog.
    for (int i = 0; i < plumeCount; ++i) {
        const ExhaustPlume &p = plumes[i];
        DrawExhaustPlume(p.x, p.y, p.owner, p.vx, p.vy, p.brightness);
    }
    plumeCount = 0;
}
const unsigned char *ShaderEffects_FlamePixels(void) { return active && hasFlames ? flames : nullptr; }

void ShaderEffects_TagExhaustParticle(int index, int owner)
{
    if (index >= 0 && index < PARTICLE_CAPACITY)
        exhaustOwners[index] = owner >= 0 && owner < GAMEPLAY_PLAYER_CAPACITY ? (unsigned char)(owner + 1) : 0;
}
int ShaderEffects_ExhaustParticleOwner(int index)
{
    return index >= 0 && index < PARTICLE_CAPACITY ? (int)exhaustOwners[index] - 1 : -1;
}
void ShaderEffects_RemoveParticle(int index, int last)
{
    if (index < 0 || last < 0 || index >= PARTICLE_CAPACITY || last >= PARTICLE_CAPACITY) return;
    exhaustOwners[index] = exhaustOwners[last];
    exhaustOwners[last] = 0;
}
void ShaderEffects_ResetParticles(void)
{
    memset(exhaustOwners, 0, sizeof(exhaustOwners));
    memset(waves, 0, sizeof(waves));
    nextWave = 0;
}

void ShaderEffects_ShipExplosion(int worldX, int worldY)
{
    if (!g_BetaShaderEffects || !ShaderPresentation_Available() ||
        g_GameState != GAME_STATE_GAMEPLAY || g_SubState != GAMEPLAY_ACTIVE) return;
    waves[nextWave] = {worldX, worldY, Platform_GetTicks(), true};
    nextWave = (nextWave + 1) % 64;
}
const float *ShaderEffects_Shockwaves(void) { return shockwaves; }
bool ShaderEffects_HasShockwaves(void) { return active && hasShockwaves; }
bool ShaderEffects_HasGlow(void) { return active && hasEmission; }
