#ifndef TOU_GFX_SHADER_H
#define TOU_GFX_SHADER_H
#include "render_backend.h"

bool ShaderPresentation_Initialize(void *window);
void ShaderPresentation_Shutdown(void);
bool ShaderPresentation_Present(const Framebuffer *framebuffer);
bool ShaderPresentation_Available(void);
#endif
