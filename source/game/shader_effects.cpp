/* Optional presentation metadata only. Never changes the software frame,
 * simulation, RNG, or effect pools. Alpha identifies visible viewport pixels;
 * zero blocks glow sources and destinations (LOS, HUD, borders, overlays). */
#include <algorithm>
#include <cstring>
#include <cmath>
#include <memory>
#include <new>
#include "tou.h"
#include "gfx_shader.h"
#include "shader_effects.h"
#include "platform.h"
#include "presentation_events.h"

namespace {
unsigned char glow[640 * 480 * 4];
unsigned char flames[640 * 480 * 4];
bool hasFlames = false;
struct ExhaustPlume { int x, y, owner, vx, vy, brightness; bool turbo; };
ExhaustPlume plumes[ENTITY_ACTIVE_CAPACITY + PARTICLE_CAPACITY];
int plumeCount = 0;
uint16_t snapshot[640 * 480];
uint16_t overlaySnapshot[640 * 480];
bool overlaySnapshotValid = false;
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
uint32_t effectsTime = 0, lastWallTime = 0;
bool clockInitialized = false, clockWasPaused = false;
struct BeamLight { int x, y, endX, endY; uint32_t started; bool live; };
BeamLight beams[64] = {};
unsigned int nextBeam = 0;
struct CuttingSparks { int x, y; float angle; uint32_t started, seed; bool live; };
CuttingSparks sparks[64] = {};
unsigned int nextSparks = 0;
uint32_t turboStarted[GAMEPLAY_PLAYER_CAPACITY] = {};
bool turboLive[GAMEPLAY_PLAYER_CAPACITY] = {};
uint32_t diggerStarted[GAMEPLAY_PLAYER_CAPACITY] = {};
bool diggerLive[GAMEPLAY_PLAYER_CAPACITY] = {};
// 0 = ordinary, 1 = machine-gun tracer, 2 = Turbo Boost spark.
unsigned char weaponParticleKinds[ENTITY_ACTIVE_CAPACITY] = {};
// Expected tile/colour, not simulation state. Removed/repainted terrain loses its tag.
std::unique_ptr<uint32_t[]> organicPixels;
unsigned int organicWidth = 0, organicHeight = 0;
void CaptureOrganicTerrain(void);

bool EffectsPaused(void)
{
    return g_GameState == GAME_STATE_GAMEPLAY &&
        (g_SubState == GAMEPLAY_PAUSED || g_SubState == GAMEPLAY_EXIT_MENU);
}

uint32_t EffectsNow(void)
{
    const uint32_t wall = Platform_GetTicks();
    const bool paused = EffectsPaused();
    if (!clockInitialized) {
        effectsTime = wall;
        clockInitialized = true;
    } else if (!paused && !clockWasPaused) {
        effectsTime += wall - lastWallTime;
    }
    lastWallTime = wall;
    clockWasPaused = paused;
    return effectsTime;
}

void BlockScreen(int x, int y)
{
    if (x < 0 || x >= 640 || y < 0 || y >= 480) return;
    memset(glow + (y * 640 + x) * 4, 0, 4);
    memset(flames + (y * 640 + x) * 4, 0, 4);
}

void Block(int x, int y)
{
    if (x < left || x >= right || y < top || y >= bottom) return;
    BlockScreen(x, y);
}
}

void ShaderEffects_BeginFrame(void)
{
    active = g_BetaShaderEffects != 0 && ShaderPresentation_Available() &&
        g_GameState == GAME_STATE_GAMEPLAY && (g_SubState == GAMEPLAY_ACTIVE || EffectsPaused());
    EffectsNow();
    snapshotValid = false;
    overlaySnapshotValid = false;
    hasEmission = false;
    hasFlames = false;
    hasShockwaves = false;
    memset(shockwaves, 0, sizeof(shockwaves));
    if (!active) {
        memset(waves, 0, sizeof(waves));
        memset(beams, 0, sizeof(beams));
        memset(sparks, 0, sizeof(sparks));
        memset(turboLive, 0, sizeof(turboLive));
        memset(diggerLive, 0, sizeof(diggerLive));
    }
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
    CaptureOrganicTerrain();
    const uint32_t now = effectsTime;
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

void ShaderEffects_SnapshotOverlay(const Framebuffer *frame)
{
    overlaySnapshotValid = false;
    if (!active || !EffectsPaused() || !frame || frame->width != 640 || frame->height != 480 || frame->stride < 640) return;
    for (int y = 0; y < 480; ++y)
        memcpy(overlaySnapshot + y * 640, frame->pixels + y * frame->stride, 640 * sizeof(uint16_t));
    overlaySnapshotValid = true;
}

void ShaderEffects_ProtectOverlay(const Framebuffer *frame)
{
    if (!active || !overlaySnapshotValid) return;
    for (int y = 0; y < 480; ++y)
        for (int x = 0; x < 640; ++x)
            if (overlaySnapshot[y * 640 + x] != frame->pixels[y * frame->stride + x])
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) BlockScreen(x + dx, y + dy);
    overlaySnapshotValid = false;
}

void ShaderEffects_ProtectOverlayRect(int x, int y, int width, int height)
{
    if (!active || !EffectsPaused() || width <= 0 || height <= 0) return;
    for (int py = std::max(0, y - 1); py < std::min(480, y + height + 1); ++py)
        for (int px = std::max(0, x - 1); px < std::min(640, x + width + 1); ++px) BlockScreen(px, py);
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
static void DrawExhaustPlume(int x, int y, int owner, int velocityX, int velocityY, int brightness, bool turbo)
{
    if (!active || brightness < 24 || !DAT_00487810 || owner < 0 ||
        owner >= GAMEPLAY_PLAYER_CAPACITY || x < left || x >= right || y < top || y >= bottom) return;
    const PlayerData &player = ((const PlayerData *)DAT_00487810)[owner];
    if (player.team >= 4 || !glow[(y * 640 + x) * 4 + 3]) return;
    float dx = (float)velocityX - (float)player.velocity_x;
    float dy = (float)velocityY - (float)player.velocity_y;
    float length = std::sqrt(dx * dx + dy * dy);
    if (turbo || length < 1.0f) {
        const int *lut = (const int *)DAT_00487ab0;
        if (!lut) return;
        dx = -(float)lut[player.heading & 0x7ff];
        dy = -(float)lut[(player.heading + 0x200) & 0x7ff];
        length = std::sqrt(dx * dx + dy * dy);
    }
    if (length < 1.0f) return;
    dx /= length; dy /= length;
    const float strength = (float)brightness / 255.0f;
    const float plumeLength = (turbo ? 19.0f : 8.0f) + 5.0f * strength;
    const float phase = (float)(effectsTime % 10000) * 0.018f + x * 0.13f + y * 0.07f;
    const unsigned short color = DAT_00483838[player.team];
    const float rgb[3] = {turbo ? 31.0f : (float)((color >> 10) & 31),
                         turbo ? 27.0f : (float)((color >> 5) & 31),
                         turbo ? 1.0f : (float)(color & 31)};
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
    plumes[plumeCount++] = {x, y, owner, velocityX, velocityY, brightness, false};
}
void ShaderEffects_BuildExhaustPlumes(void)
{
    if (!active) return;
    // Build after LOS: hidden origins must not emit a visible flame beyond fog.
    for (int i = 0; i < plumeCount; ++i) {
        const ExhaustPlume &p = plumes[i];
        DrawExhaustPlume(p.x, p.y, p.owner, p.vx, p.vy, p.brightness, p.turbo);
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
    clockInitialized = false;
    clockWasPaused = false;
    memset(beams, 0, sizeof(beams));
    memset(sparks, 0, sizeof(sparks));
    memset(turboLive, 0, sizeof(turboLive));
    memset(diggerLive, 0, sizeof(diggerLive));
    memset(weaponParticleKinds, 0, sizeof(weaponParticleKinds));
    nextBeam = nextSparks = 0;
    organicPixels.reset();
    organicWidth = organicHeight = 0;
}

void ShaderEffects_SaveParticleTags(std::vector<uint8_t> *tags)
{
    tags->assign(exhaustOwners, exhaustOwners + sizeof(exhaustOwners));
    tags->insert(tags->end(), weaponParticleKinds, weaponParticleKinds + sizeof(weaponParticleKinds));
}
void ShaderEffects_RestoreParticleTags(const std::vector<uint8_t> &tags)
{
    if (tags.size() != sizeof(exhaustOwners) + sizeof(weaponParticleKinds)) return;
    memcpy(exhaustOwners, tags.data(), sizeof(exhaustOwners));
    memcpy(weaponParticleKinds, tags.data() + sizeof(exhaustOwners), sizeof(weaponParticleKinds));
}

void ShaderEffects_ShipExplosion(int worldX, int worldY)
{
    if (PresentationEvents_Recording()) {
        PresentationEvents_Record(PRESENT_EXPLOSION, worldX, worldY); return;
    }
    if (!g_BetaShaderEffects || !ShaderPresentation_Available() ||
        g_GameState != GAME_STATE_GAMEPLAY || g_SubState != GAMEPLAY_ACTIVE) return;
    waves[nextWave] = {worldX, worldY, EffectsNow(), true};
    nextWave = (nextWave + 1) % 64;
}
const float *ShaderEffects_Shockwaves(void) { return shockwaves; }
bool ShaderEffects_HasShockwaves(void) { return active && hasShockwaves; }
bool ShaderEffects_HasGlow(void) { return active && hasEmission; }

namespace {
bool CanRecordWeaponLight(void)
{
    return g_BetaShaderEffects && ShaderPresentation_Available() &&
        g_GameState == GAME_STATE_GAMEPLAY && g_SubState == GAMEPLAY_ACTIVE;
}

void EmitLight(int x, int y, float r, float g, float b, float core)
{
    if (x < left || x >= right || y < top || y >= bottom) return;
    unsigned char *pixel = glow + (y * 640 + x) * 4;
    if (!pixel[3]) return;
    const float rgb[3] = {r, g, b};
    unsigned char *flame = flames + (y * 640 + x) * 4;
    for (int c = 0; c < 3; ++c) {
        pixel[c] = std::max(pixel[c], (unsigned char)(255 * std::min(1.0f, rgb[c])));
        flame[c] = std::max(flame[c], (unsigned char)(255 * std::min(1.0f, rgb[c] * core)));
    }
    hasEmission = true;
    hasFlames |= core > 0;
}

// Analytic soft line, clipped to this viewport; no simulation particles or RNG.
void LightStroke(float x, float y, float endX, float endY, float width,
                 float r, float g, float b, float core)
{
    const float dx = endX - x, dy = endY - y;
    const float length2 = dx * dx + dy * dy;
    const float margin = width * 3;
    const int minX = std::max(left, (int)std::floor(std::min(x, endX) - margin));
    const int maxX = std::min(right - 1, (int)std::ceil(std::max(x, endX) + margin));
    const int minY = std::max(top, (int)std::floor(std::min(y, endY) - margin));
    const int maxY = std::min(bottom - 1, (int)std::ceil(std::max(y, endY) + margin));
    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            const float t = length2 > 0 ? std::max(0.0f, std::min(1.0f,
                ((px - x) * dx + (py - y) * dy) / length2)) : 0;
            const float sx = px - x - t * dx, sy = py - y - t * dy;
            const float distance2 = sx * sx + sy * sy;
            if (distance2 > 9 * width * width) continue;
            const float strength = std::exp(-distance2 / (width * width));
            if (strength >= 0.01f) EmitLight(px, py, r * strength, g * strength, b * strength, core);
        }
    }
}

bool VisibleOrigin(int x, int y)
{
    return x >= left && x < right && y >= top && y < bottom &&
        glow[(y * 640 + x) * 4 + 3] != 0;
}

void CaptureOrganicTerrain(void)
{
    if (!organicPixels || organicWidth != DAT_004879f0 || organicHeight != DAT_004879f4 ||
        !DAT_0048782c || !DAT_00481f50) return;
    const auto *tiles = static_cast<const unsigned char *>(DAT_0048782c);
    const auto *colors = static_cast<const unsigned short *>(DAT_00481f50);
    const float time = (float)(effectsTime % 100000) * 0.006f;
    for (int y = top; y < bottom; ++y) {
        const int wy = y - top + DAT_004806e0;
        if (wy < 0 || wy >= (int)organicHeight) continue;
        for (int x = left; x < right; ++x) {
            const int wx = x - left + DAT_004806dc;
            if (wx < 0 || wx >= (int)organicWidth) continue;
            uint32_t &tag = organicPixels[(size_t)wy * organicWidth + wx];
            if (!tag) continue;
            const size_t offset = (size_t)wy * DAT_00487a00 + wx;
            const unsigned short color = colors[offset];
            if (tag != (0x1000000u | ((uint32_t)tiles[offset] << 16) | color)) {
                tag = 0; continue;
            }
            const int r = ((color >> 11) & 31) * 255 / 31;
            const int g = ((color >> 5) & 63) * 255 / 63;
            const int b = (color & 31) * 255 / 31;
            // Brown stays brown: only the green material emits toxic light.
            if (g < 32 || g * 10 <= r * 12 || g * 10 <= b * 12) continue;
            const float pulse = 0.13f + 0.22f * (0.5f + 0.5f * std::sin(time + wx * 0.09f + wy * 0.06f));
            EmitLight(x, y, pulse * 0.14f, pulse, pulse * 0.025f, 0.8f);
        }
    }
}
} // namespace

void ShaderEffects_OrganicPixel(int worldX, int worldY, bool refreshOnly)
{
    if (PresentationEvents_Recording()) {
        if (!DAT_0048782c || !DAT_00481f50 || worldX < 0 || worldY < 0 ||
            worldX >= (int)DAT_004879f0 || worldY >= (int)DAT_004879f4) return;
        const size_t offset = (size_t)worldY * DAT_00487a00 + worldX;
        const uint32_t tag = 0x1000000u |
            ((uint32_t)static_cast<unsigned char *>(DAT_0048782c)[offset] << 16) |
            static_cast<unsigned short *>(DAT_00481f50)[offset];
        PresentationEvents_Record(PRESENT_ORGANIC, worldX, worldY, (int)tag, refreshOnly ? 1 : 0);
        return;
    }
    // Keep tags while effects are toggled off, so existing waste lights up on enable.
    if (!ShaderPresentation_Available() || !DAT_0048782c || !DAT_00481f50 ||
        worldX < 0 || worldY < 0 || worldX >= (int)DAT_004879f0 || worldY >= (int)DAT_004879f4) return;
    if (organicWidth != DAT_004879f0 || organicHeight != DAT_004879f4) {
        organicWidth = DAT_004879f0; organicHeight = DAT_004879f4;
        const size_t count = (size_t)organicWidth * organicHeight;
        // A failed/oversized cosmetic allocation must not interrupt the match.
        organicPixels.reset(count <= 16 * 1024 * 1024 ? new (std::nothrow) uint32_t[count]() : nullptr);
    }
    if (!organicPixels) return;
    uint32_t &tag = organicPixels[(size_t)worldY * organicWidth + worldX];
    if (refreshOnly && !tag) return;
    const size_t offset = (size_t)worldY * DAT_00487a00 + worldX;
    tag = 0x1000000u | ((uint32_t)static_cast<unsigned char *>(DAT_0048782c)[offset] << 16) |
        static_cast<unsigned short *>(DAT_00481f50)[offset];
}

void ShaderEffects_CommitOrganicPixel(int x, int y, uint32_t expected, bool refreshOnly)
{
    if (!DAT_0048782c || !DAT_00481f50 || x < 0 || y < 0 ||
        x >= (int)DAT_004879f0 || y >= (int)DAT_004879f4) return;
    const size_t offset = (size_t)y * DAT_00487a00 + x;
    const uint32_t actual = 0x1000000u |
        ((uint32_t)static_cast<unsigned char *>(DAT_0048782c)[offset] << 16) |
        static_cast<unsigned short *>(DAT_00481f50)[offset];
    if (actual == expected) ShaderEffects_OrganicPixel(x, y, refreshOnly);
}

void ShaderEffects_TurboBoost(int owner)
{
    if (PresentationEvents_Recording()) {
        PresentationEvents_Record(PRESENT_BOOST, owner); return;
    }
    if (!CanRecordWeaponLight() || owner < 0 || owner >= GAMEPLAY_PLAYER_CAPACITY) return;
    turboStarted[owner] = EffectsNow();
    turboLive[owner] = true;
}

void ShaderEffects_Laser(int startX, int startY, int endX, int endY)
{
    if (PresentationEvents_Recording()) {
        PresentationEvents_Record(PRESENT_LASER, startX, startY, endX, endY); return;
    }
    if (!CanRecordWeaponLight()) return;
    beams[nextBeam] = {startX, startY, endX, endY, EffectsNow(), true};
    nextBeam = (nextBeam + 1) % 64;
}

void ShaderEffects_DiggerContact(int worldX, int worldY, int velocityX, int velocityY)
{
    if (PresentationEvents_Recording()) {
        PresentationEvents_Record(PRESENT_SPARK, worldX, worldY, velocityX, velocityY); return;
    }
    if (!CanRecordWeaponLight()) return;
    const uint32_t now = EffectsNow();
    sparks[nextSparks] = {worldX, worldY, std::atan2(-(float)velocityY, -(float)velocityX),
                         now, now ^ (uint32_t)worldX * 73856093u ^ (uint32_t)worldY * 19349663u, true};
    nextSparks = (nextSparks + 1) % 64;
}

void ShaderEffects_DiggerShipContact(int owner)
{
    const bool recording = PresentationEvents_Recording();
    if ((!recording && !CanRecordWeaponLight()) || !DAT_00487810 || !DAT_0048782c || !DAT_00487928 ||
        !DAT_00487ab0 || owner < 0 || owner >= GAMEPLAY_PLAYER_CAPACITY) return;
    const uint32_t now = recording ? 0 : EffectsNow();
    if (!recording && diggerLive[owner] && now - diggerStarted[owner] < 40) return;
    const PlayerData &player = *Player_Get(owner);
    const int *lut = static_cast<const int *>(DAT_00487ab0);
    // Sample the cutting face before the original terrain-removal operation.
    for (int radius = 4; radius <= 24; radius += 4) {
        for (int side = 0; side < 5; ++side) {
            const int spread[5] = {0, -128, 128, -256, 256};
            const int angle = (player.heading + spread[side]) & 0x7ff;
            const float dx = (float)lut[angle], dy = (float)lut[(angle + 512) & 0x7ff];
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length < 1) continue;
            const int x = (player.position_x >> 18) + (int)(dx * radius / length);
            const int y = (player.position_y >> 18) + (int)(dy * radius / length);
            if (x < 0 || y < 0 || x >= (int)DAT_004879f0 || y >= (int)DAT_004879f4) continue;
            const unsigned char tile = static_cast<const unsigned char *>(DAT_0048782c)[(size_t)y * DAT_00487a00 + x];
            const auto *properties = static_cast<const unsigned char *>(DAT_00487928) + tile * 32;
            if (properties[1] != 0 || properties[4] != 0) continue;
            ShaderEffects_DiggerContact(x, y, lut[angle], lut[(angle + 512) & 0x7ff]);
            if (!recording) { diggerStarted[owner] = now; diggerLive[owner] = true; }
            return;
        }
    }
}

void ShaderEffects_BuildWeaponLights(void)
{
    if (!active) return;
    // After LOS, before HUD: hidden emitters cannot spray into visible regions.
    for (int i = 0; i < std::min<int>(g_EntityCount, ENTITY_ACTIVE_CAPACITY); ++i) {
        const unsigned char kind = weaponParticleKinds[i];
        if (!kind) continue;
        const Entity &particle = g_EntityPool[i];
        if (particle.type != 0x67) continue;
        const int x = (particle.position_x >> 18) - DAT_004806dc + g_Viewport.screen_x;
        const int y = (particle.position_y >> 18) - DAT_004806e0 + g_Viewport.screen_y;
        if (!VisibleOrigin(x, y)) continue;
        const unsigned short color = (unsigned short)((unsigned int)particle.palette_value - 30000u);
        const int brightness = std::max((color >> 10) & 31,
                                       std::max((color >> 5) & 31, color & 31));
        if (brightness == 0) continue;
        const float fade = (float)brightness / 31;
        float dx = (float)particle.velocity_x, dy = (float)particle.velocity_y;
        const float speed = std::sqrt(dx * dx + dy * dy);
        const bool boost = kind == 2;
        const float length = boost ? 1.8f + 2.8f * fade : 3.0f + 4.0f * fade;
        if (speed > 1) { dx *= length / speed; dy *= length / speed; }
        else { dx = 0; dy = 0; }
        const float intensity = boost ? std::sqrt(fade) : fade;
        LightStroke(x - dx, y - dy, (float)x, (float)y, boost ? 1.0f : 0.75f,
                    intensity, intensity * (boost ? 0.92f : 0.78f),
                    intensity * (boost ? 0.035f : 0.12f), 1.0f);
        if (boost) {
            // A small white-hot head, with a yellow tail and soft surrounding glow.
            LightStroke(x - dx * 0.3f, y - dy * 0.3f, (float)x, (float)y, 0.45f,
                        intensity, intensity * 0.98f, intensity * 0.6f, 1.0f);
        }
    }
    for (int p = 0; DAT_00487810 && p < std::min<int>(DAT_00489240, GAMEPLAY_PLAYER_CAPACITY); ++p) {
        if (!turboLive[p]) continue;
        if (effectsTime - turboStarted[p] >= 100) { turboLive[p] = false; continue; }
        const PlayerData &player = *Player_Get(p);
        if (player.state_24 != 0 || player.health <= 0 || !DAT_00487ab0) continue;
        const int *lut = static_cast<const int *>(DAT_00487ab0);
        const float dx = (float)lut[player.heading & 0x7ff];
        const float dy = (float)lut[(player.heading + 0x200) & 0x7ff];
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length < 1) continue;
        const int x = (player.position_x >> 18) - DAT_004806dc + g_Viewport.screen_x - (int)(dx * 8 / length);
        const int y = (player.position_y >> 18) - DAT_004806e0 + g_Viewport.screen_y - (int)(dy * 8 / length);
        if (!VisibleOrigin(x, y)) continue;
        DrawExhaustPlume(x, y, p, 0, 0, 255, true);
        LightStroke((float)x, (float)y, x - dx * 17 / length, y - dy * 17 / length,
                    1.8f, 0.75f, 0.65f, 0.02f, 0);
    }
    for (BeamLight &beam : beams) {
        if (!beam.live) continue;
        const uint32_t age = effectsTime - beam.started;
        if (age >= 120) { beam.live = false; continue; }
        const int x = beam.x - DAT_004806dc + g_Viewport.screen_x;
        const int y = beam.y - DAT_004806e0 + g_Viewport.screen_y;
        if (x >= left && x < right && y >= top && y < bottom && !VisibleOrigin(x, y)) continue;
        const float fade = 1.0f - (float)age / 120;
        LightStroke((float)x, (float)y, (float)(beam.endX - DAT_004806dc + g_Viewport.screen_x),
                    (float)(beam.endY - DAT_004806e0 + g_Viewport.screen_y),
                    1.1f, 0.65f * fade, 0.18f * fade, fade, 0.65f);
    }
    for (CuttingSparks &burst : sparks) {
        if (!burst.live) continue;
        const uint32_t age = effectsTime - burst.started;
        if (age >= 280) { burst.live = false; continue; }
        const int x = burst.x - DAT_004806dc + g_Viewport.screen_x;
        const int y = burst.y - DAT_004806e0 + g_Viewport.screen_y;
        if (!VisibleOrigin(x, y)) continue;
        const float t = (float)age / 1000, tail = std::max(0.0f, t - 0.025f);
        const float fade = 1.0f - (float)age / 280;
        uint32_t seed = burst.seed;
        for (int n = 0; n < 9; ++n) {
            // Cosmetic hash, deliberately not rand(): gameplay RNG order is untouched.
            seed = seed * 1664525u + 1013904223u;
            const float angle = burst.angle + ((float)(seed & 1023) / 1023 - 0.5f) * 3.6f;
            const float speed = 60 + (float)((seed >> 10) & 127);
            const float vx = std::cos(angle) * speed, vy = std::sin(angle) * speed;
            LightStroke(x + vx * tail, y + vy * tail + 100 * tail * tail,
                        x + vx * t, y + vy * t + 100 * t * t,
                        0.65f, fade, fade * 0.65f, fade * 0.12f, 1);
        }
    }
}

void ShaderEffects_TagMachinegunParticle(int index)
{
    if (index >= 0 && index < ENTITY_ACTIVE_CAPACITY) weaponParticleKinds[index] = 1;
}

void ShaderEffects_TagBoostParticle(int index)
{
    if (index >= 0 && index < ENTITY_ACTIVE_CAPACITY) weaponParticleKinds[index] = 2;
}

void ShaderEffects_RemoveEntity(int index, int last)
{
    if (index < 0 || last < 0 || index >= ENTITY_ACTIVE_CAPACITY || last >= ENTITY_ACTIVE_CAPACITY) return;
    weaponParticleKinds[index] = weaponParticleKinds[last];
    weaponParticleKinds[last] = 0;
}
