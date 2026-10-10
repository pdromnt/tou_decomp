# Experimental Shader Presentation

The software renderer still produces the authoritative 640 x 480 RGB565 frame.
This prototype adds optional GPU presentation after that frame is complete.
Physics, collisions, RNG, particle spawning, terrain mutation, and netplay
remain untouched. No shader changes gameplay visibility or lighting rules.

## Run It

Windows PowerShell, from the directory containing `TOU.exe`:

```powershell
$env:TOU_SHADER_PRESENTATION = '1'
./TOU.exe --logging
Remove-Item Env:TOU_SHADER_PRESENTATION
```

Linux:

```sh
TOU_SHADER_PRESENTATION=1 ./TOU --logging
```

macOS app bundle:

```sh
TOU_SHADER_PRESENTATION=1 ./TOU.app/Contents/MacOS/TOU --logging
```

Choose **Options > Details & Visuals > Display and Rendering Settings > Scaling > FSR 1 (beta)**.
Setting the environment variable enables the shader backend, not the FSR
filter itself. All four filters can be compared without restarting once
the shader backend is active. To restore ordinary SDL presentation, close
the game and launch normally without the variable.

`settings.json` stores `display.scaling` as `crisp`, `smooth`, `pixel-art`,
or `fsr1`. Missing or invalid values preserve the previous default, Pixel art.
Factory reset restores Pixel art. A saved FSR choice on a normal SDL launch
falls back to Pixel art, with an explanation in the Display screen.

## What It Does

- Crisp uses nearest-neighbor filtering and whole-number enlargement, with
  extra borders when necessary. Small windows still fit the frame.
- Smooth uses linear filtering within the available 4:3 area.
- Pixel art preserves sharp pixel interiors with softer transitions at
  fractional enlargement. This remains the default.
- FSR 1 uses AMD's full-precision EASU edge-adaptive upscale pass into a
  display-sized RGBA8 target, then RCAS sharpening into the window. Sharpening
  is fixed at the accepted 70% strength with RCAS denoising enabled.
  The temporary comparison controls and unused shader variant have been removed.

FSR affects the whole final image, including text. It cannot reconstruct
detail missing from the original art, and may soften pixel-art fonts or
produce halos. It is not guaranteed to look better. Below native resolution
the prototype uses linear downsampling instead of EASU.

The shader backend requests desktop OpenGL 4.1. Linux retries OpenGL ES 3.1
if that profile cannot create a context. All GL calls are resolved through
SDL; no GL import library, shader compiler executable, or shader runtime DLL
is added. The FSR source is pinned and embedded at build time, with AMD's
MIT notice shipped as `TOU-FSR-LICENSE.txt`.

If context creation or shader compilation fails, initialization falls back
to the ordinary SDL renderer. An unavailable FSR intermediate target uses
filtered presentation. Diagnostics are written to `debug.txt` with `--logging`.
Shader availability is not a performance guarantee on low-power devices.
For Raspberry Pi / Compute Module testing, start with an X11 desktop session:
the CM4 Debian report identified Wayland as the source of severe performance
problems and match audio stuttering, rather than audio sample settings. Keep
that desktop-session issue separate from shader-backend performance measurements.

## Future VFX Boundary

### Shader Effects (Beta)

The Display and Rendering Settings screen now has an off-by-default
**Shader effects (beta)** switch, saved as `display.shaderEffects` (0/1).
It requires the same opt-in shader backend as FSR, but works independently
of scaling mode. Unsupported renderers ignore it. Factory reset disables it.

During active gameplay, the renderer captures newly added bright colour from
the projectile/explosion and particle passes into a separate RGBA glow mask.
The GPU runs horizontal and vertical Gaussian bloom passes using the
MIT-licensed Jam3 / Experience Monks `glsl-fast-gaussian-blur` 13-tap kernel,
repeated as four horizontal/vertical pairs with contiguous bilinear taps.
Only the smoothed halo is blended into the presented frame; there is no sharp
native-resolution core contribution. The targets remain 640 x 480, so the
extra passes increase GPU cost rather than mask resolution.
The source is pinned in `third_party/gaussian_blur`, embedded at build time,
and attributed in the installed `TOU-BLOOM-LICENSE.txt` notice.
It does not change the authoritative RGB565 framebuffer or rerun simulation.
This is artistic bloom, not physical lighting or a replacement for game LOS.

Ship exhaust contributes softer emission tinted from the emitting player's
configured team colour. A separate, linearly filtered flame layer adds narrow
tapered plumes with a hotter core and slight visual curl, without widening them
through the explosion bloom kernel. Plumes follow particle motion relative to
the emitting ship (reverse heading is the stationary fallback), scale with
actual added particle brightness, and are built after LOS filtering. This adds
bounded CPU mask work and an extra texture upload only with active exhaust;
compare performance in high-player-count scenes before promotion.
Complex exhaust uses its existing owner; simple flame
particles carry separate presentation-only owner tags, kept aligned with pool
compaction and cleared on pool reset or authoritative snapshot import. Existing
simple exhaust restored from a snapshot has no tag and uses the regular particle
glow until it expires; newly spawned exhaust is team-tinted. No gameplay record,
RNG call, particle lifetime, network checksum, or saved format is changed.

The mask is cleared each frame. It carries viewport IDs to prevent cross-view
sampling in local split screen. LOS-hidden and partially obscured pixels block
both glow sources and destinations; opaque LOS disables glow entirely.
HUD changes and their immediate outlines are excluded. Added glow is disabled
in menus and all non-active gameplay substates, including pause and round panels.

Ship deaths also emit a brief radial distortion wave under the same beta toggle.
The presentation-only event queue stores world positions and wall-clock ages;
each wave lasts 650 ms, expanding from 8 to 96 native pixels with a fading
strength capped at 6 pixels of displacement. A Gaussian crest and opposing
trough make the shock front more pronounced. The final plain/RCAS shader
warps sampling coordinates; EASU and the authoritative software frame are not
modified. This is refraction of the flattened image, not a gameplay shockwave.
At most 64 recent events and 16 on-screen events per viewport are presented.
Off-screen or LOS-hidden centres do not contribute. Destination, sample path,
and bilinear source footprint must share a visible viewport ID, protecting LOS,
HUD pixels/outlines, and split-screen borders. Pause, inactive states, level
reset, and authoritative snapshot import discard cosmetic waves.

Acceptance still requires comparing on/off in motion, heavy explosion scenes,
split screen, each LOS style, HUD/weapon selectors, pause/end-round screens,
level changes, and normal launches without the shader backend. On the CM4,
check frame pacing and audio as well as visual correctness. Bright particles
are selected by their actual added colour, so emission threshold/intensity may
need further art-direction tuning.
Compare exhaust from differently coloured teams and ship exhaust styles,
including fading trails and repeated level changes; explosions retain their
own emission colours. Check enlarged explosion halos for square patterns.
Check the tapered exhaust flames while thrusting, turning, and coasting,
including darker terrain and LOS edges; the original particle sprites stay intact.
Trigger ship deaths with effects on/off and compare the short expanding ripple.
Ordinary projectile blasts should not trigger it. Test deaths near HUD and LOS
edges, simultaneous deaths in split screen, pausing during a ripple, and level
changes. Assess extra fragment cost on lower-power GPUs before promotion.

`gfx_shader.cpp` owns the context, shaders, input texture, and intermediate
targets. FSR and bloom demonstrate multipass pipelines; additional passes could
apply CRT styling, colour grading, or screen-space distortion. Keep effects
optional and provide a no-effects path for pixel clarity and low-power GPUs.

The main colour input is a flattened image, not a modern scene with depth,
motion vectors, material IDs, or separate UI/world layers. Object-aware glow,
localized heat distortion would require more targeted render outputs or masks.
The beta now supplies a selective glow/visibility mask, not general scene layers.
Depth-aware effects and temporal FSR do not become
available merely by adding this presentation backend. A shader editing UI,
hot reload, and a general effect graph are not part of this prototype.

## Manual Acceptance

1. Compare all filters in the same menu at 800 x 600 and larger outputs.
   Check small glyphs, outlines, background detail, halos, and shimmering.
2. Compare them in a normal level and a GG level with movement, explosions,
   water, and fog. Confirm no inversion, frame remnants, or colour changes.
3. Change resolution, switch fullscreen/windowed repeatedly, and verify the
   image stays centered with the correct aspect ratio.
4. Click both the Scaling label and its value using left/right buttons.
   Each click should move exactly one choice. Check cursor and hitbox alignment
   after changing scaling; Crisp may add extra borders.
5. Alt-tab out and back during menus and a match. Verify focus, audio,
   pausing rules, and the returned frame.
6. Restart and confirm the saved filter. Launch without the shader variable
   after saving FSR and verify the documented Pixel art fallback.
7. Check translated Display text in English, Spanish, Brazilian Portuguese,
   and Finnish. Help text should wrap and center without clipping.
8. Try macOS Intel/Apple Silicon and Linux x64/ARM64, especially the CM4.
   Check shader diagnostics and responsiveness, not just successful launch.
9. Install/package a build and verify it runs without source or shader files
   and includes `THIRD_PARTY_NOTICES.md`, `TOU-FSR-LICENSE.txt`, and
   `TOU-BLOOM-LICENSE.txt` (also inside macOS app resources).

Local Windows compilation, GPU shader compilation, and hands-on visual
acceptance of FSR, bloom, team-coloured exhaust flames, ship-death refraction,
and corrected LOS colours are recorded. This does not establish every edge case
or low-power performance. Cross-platform shader runtime acceptance and the
remaining focused checks in `BACKLOG.md` remain pending.
