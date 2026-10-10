#ifndef TOU_SHADER_EFFECTS_H
#define TOU_SHADER_EFFECTS_H
#include "render_backend.h"

void ShaderEffects_BeginFrame(void);
void ShaderEffects_BeginViewport(int index);
void ShaderEffects_Snapshot(const Framebuffer *frame);
void ShaderEffects_Capture(const Framebuffer *frame);
void ShaderEffects_ProtectHud(const Framebuffer *frame);
void ShaderEffects_SnapshotOverlay(const Framebuffer *frame);
void ShaderEffects_ProtectOverlay(const Framebuffer *frame);
void ShaderEffects_ProtectOverlayRect(int x, int y, int width, int height);
void ShaderEffects_SetVisibility(int x, int y, unsigned char visibility);
const unsigned char *ShaderEffects_GlowPixels(void);
const float *ShaderEffects_ViewportBounds(void);
bool ShaderEffects_Active(void);
int ShaderEffects_ExhaustPixel(int x, int y, int owner, unsigned short before, unsigned short after);
void ShaderEffects_ExhaustPlume(int x, int y, int owner, int velocityX, int velocityY, int brightness);
const unsigned char *ShaderEffects_FlamePixels(void);
void ShaderEffects_BuildExhaustPlumes(void);
void ShaderEffects_TagExhaustParticle(int index, int owner);
int ShaderEffects_ExhaustParticleOwner(int index);
void ShaderEffects_RemoveParticle(int index, int last);
void ShaderEffects_ResetParticles(void);
void ShaderEffects_ShipExplosion(int worldX, int worldY);
const float *ShaderEffects_Shockwaves(void);
bool ShaderEffects_HasShockwaves(void);
bool ShaderEffects_HasGlow(void);
#endif
