# Gaussian bloom kernel

Unmodified `13.glsl` from Jam3 / Experience Monks' MIT-licensed
https://github.com/Experience-Monks/glsl-fast-gaussian-blur

Pinned revision: `5dbb6e97aa43d4be9369bdd88e835f47023c5e2a`.

Embedded at build time. `gfx_shader.cpp` strips the glslify export pragma and
maps texture2D calls to a viewport/visibility-gated sampler, then runs the
kernel horizontally and vertically. Attribution ships in installed game
packages as `TOU-BLOOM-LICENSE.txt`. No npm or glslify runtime is required.
