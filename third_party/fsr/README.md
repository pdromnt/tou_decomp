# FidelityFX Super Resolution 1

Unmodified AMD MIT-licensed shader headers from:
https://github.com/GPUOpen-Effects/FidelityFX-FSR

Pinned revision: `a21ffb8f6c13233ba336352bdff293894c706575`.

`gfx_shader.cpp` wraps the full-precision EASU and RCAS algorithms in GLSL.
CMake embeds these sources in the executable; no external shader files,
offline shader compiler, or new runtime library is needed. The AMD license
is included in installed game packages as `TOU-FSR-LICENSE.txt`.
