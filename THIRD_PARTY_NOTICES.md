# Third-Party Notices

These notices cover the third-party source newly embedded by the optional
presentation and gamepad backends. They do not license TOU's original game assets or grant
a project-wide license to the reconstruction. Existing dependencies such as
SDL3, SDL_mixer, nlohmann/json, and stb_image retain their upstream terms.

## AMD FidelityFX Super Resolution 1

- Source: https://github.com/GPUOpen-Effects/FidelityFX-FSR
- Revision: `a21ffb8f6c13233ba336352bdff293894c706575`
- Files: `source/third_party/fsr/ffx_a.h`, `source/third_party/fsr/ffx_fsr1.h`
- Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
- License: MIT; full text in [source/third_party/fsr/LICENSE.txt](source/third_party/fsr/LICENSE.txt).
- Packaged full notice: `TOU-FSR-LICENSE.txt`.

The vendored headers are unmodified. TOU embeds them at build time and supplies
GLSL adapters for EASU and RCAS, including desktop/GLES presentation integration.

## Jam3 / Experience Monks Fast Gaussian Blur

- Source: https://github.com/Experience-Monks/glsl-fast-gaussian-blur
- Revision: `5dbb6e97aa43d4be9369bdd88e835f47023c5e2a`
- File: `source/third_party/gaussian_blur/13.glsl`
- Copyright (c) 2015 Jam3.
- License: MIT; full text in [source/third_party/gaussian_blur/LICENSE.md](source/third_party/gaussian_blur/LICENSE.md).
- Packaged full notice: `TOU-BLOOM-LICENSE.txt`.

The vendored kernel is unmodified. The runtime strips its glslify export pragma
and substitutes a viewport/visibility-gated sampler, then repeats the contiguous
kernel in horizontal/vertical passes for bloom. No glslify runtime is shipped.

## Project-Specific Effect Code

The emission capture, team-coloured tapered exhaust layer, visibility/HUD guards,
weapon-specific boost/waste/laser lights, cosmetic Digger sparks,
and radial ship/Gamma Boom distortion were written for this project, not copied from
another shader repository. They are not covered by the upstream MIT notices
merely because they interact with those components.

## SDL Xbox BLE endpoint compatibility patch

The pinned SDL source receives a marked build-time patch to its Windows Xbox
HID endpoint filter. Xbox Series BLE (`045e:0b13`) devices whose bus is explicitly
Bluetooth are allowed through the existing HIDAPI Xbox parser. Wired XGIP proxy
endpoints remain filtered. No external controller database is embedded; SDL's
existing upstream license applies to its driver code.

Full upstream notices accompany installed builds and release archives.
On macOS they are also inside `TOU.app/Contents/Resources`. In release packages,
use the packaged notice filenames above; repository-relative links in this index
refer to the source checkout, not bundled source files.
