/* Optional GPU presentation. The authoritative RGB565 frame is still produced
 * by the software renderer. No simulation state enters this module. */
#include "tou.h"
#include "gfx_shader.h"
#include "shader_effects.h"
#include "screenshots.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <string>
#include "fsr_sources.h"

namespace {
SDL_Window *window = nullptr;
SDL_GLContext context = nullptr;
GLuint vao = 0, input = 0, easuTexture = 0, easuFbo = 0;
GLuint glowTexture = 0, flameTexture = 0;
GLuint bloomTextures[2] = {}, bloomFbos[2] = {}, bloomProgram = 0;
bool bloomReady = false;
bool reportedBloom = false;
GLuint plainProgram = 0, easuProgram = 0, rcasProgram = 0;
int inputWidth = 0, inputHeight = 0, outputWidth = 0, outputHeight = 0;
bool fsrTargetValid = false;

template<size_t N>
std::string JoinSources(const char *const (&chunks)[N])
{
    std::string result;
    for (const char *chunk : chunks) result += chunk;
    return result;
}

// Resolve every GL call through SDL, including 1.x functions, so there is no
// platform OpenGL link dependency (or Windows 1.1 import-table restriction).
#define GL_FUNCTIONS(X) \
 X(const GLubyte *, GetString, (GLenum)) \
 X(GLenum, GetError, (void)) \
 X(void, GenVertexArrays, (GLsizei, GLuint *)) \
 X(void, BindVertexArray, (GLuint)) \
 X(void, DeleteVertexArrays, (GLsizei, const GLuint *)) \
 X(GLuint, CreateShader, (GLenum)) \
 X(void, ShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
 X(void, CompileShader, (GLuint)) \
 X(void, GetShaderiv, (GLuint, GLenum, GLint *)) \
 X(void, GetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
 X(void, DeleteShader, (GLuint)) \
 X(GLuint, CreateProgram, (void)) \
 X(void, AttachShader, (GLuint, GLuint)) \
 X(void, LinkProgram, (GLuint)) \
 X(void, GetProgramiv, (GLuint, GLenum, GLint *)) \
 X(void, GetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
 X(void, DeleteProgram, (GLuint)) \
 X(void, UseProgram, (GLuint)) \
 X(GLint, GetUniformLocation, (GLuint, const GLchar *)) \
 X(void, Uniform1i, (GLint, GLint)) \
 X(void, Uniform2f, (GLint, GLfloat, GLfloat)) \
 X(void, Uniform4fv, (GLint, GLsizei, const GLfloat *)) \
 X(void, GenTextures, (GLsizei, GLuint *)) \
 X(void, DeleteTextures, (GLsizei, const GLuint *)) \
 X(void, ActiveTexture, (GLenum)) \
 X(void, BindTexture, (GLenum, GLuint)) \
 X(void, TexParameteri, (GLenum, GLenum, GLint)) \
 X(void, TexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
 X(void, TexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *)) \
 X(void, PixelStorei, (GLenum, GLint)) \
 X(void, ReadBuffer, (GLenum)) \
 X(void, ReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)) \
 X(void, GenFramebuffers, (GLsizei, GLuint *)) \
 X(void, DeleteFramebuffers, (GLsizei, const GLuint *)) \
 X(void, BindFramebuffer, (GLenum, GLuint)) \
 X(void, FramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
 X(GLenum, CheckFramebufferStatus, (GLenum)) \
 X(void, Viewport, (GLint, GLint, GLsizei, GLsizei)) \
 X(void, ClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
 X(void, Clear, (GLbitfield)) \
 X(void, Disable, (GLenum)) \
 X(void, DrawArrays, (GLenum, GLint, GLsizei))
#define DECLARE_GL(result, name, args) result (APIENTRY *p##name) args = nullptr;
GL_FUNCTIONS(DECLARE_GL)
#undef DECLARE_GL

const char *vertexSource = R"GLSL(
out vec2 uv;
uniform int flip;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    uv = vec2(p.x, flip != 0 ? 1.0 - p.y : p.y);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

const char *plainSource = R"GLSL(
in vec2 uv;
out vec4 color;
uniform sampler2D image;
uniform vec2 inputSize;
uniform vec2 outputSize;
uniform int pixelArt;
void main() {
    vec2 warped = WarpCoords(uv);
    vec2 coords = warped;
    if (pixelArt != 0) {
        vec2 scale = max(floor(outputSize / inputSize), vec2(1.0));
        vec2 pixel = coords * inputSize;
        coords = (floor(pixel) + clamp((fract(pixel) - 0.5) * scale + 0.5, 0.0, 1.0)) / inputSize;
    }
    color = vec4(ApplyGlow(texture(image, coords).rgb, warped), 1.0);
}
)GLSL";

const char *glowSource = R"GLSL(
uniform sampler2D glowImage;
uniform sampler2D bloomImage;
uniform sampler2D flameImage;
uniform int effectsOn;
uniform int flamesOn;
uniform int distortionOn;
uniform vec4 shockwaves[64]; // 16 per viewport: native centre, radius, strength.
uniform vec4 viewports[4];
float MaskId(vec2 p) {
    if (any(lessThan(p, vec2(0))) || any(greaterThanEqual(p, vec2(1)))) return 0.0;
    return floor(texelFetch(glowImage, ivec2(p * vec2(640,480)), 0).a * 255.0 + 0.5);
}
vec2 WarpCoords(vec2 p) {
    if (distortionOn == 0) return p;
    float id = MaskId(p);
    if (id < 1.0 || id > 4.0) return p;
    vec2 nativeSize = vec2(640,480);
    vec2 offset = vec2(0);
    int first = (int(id) - 1) * 16;
    for (int i = 0; i < 16; ++i) {
        vec4 wave = shockwaves[first + i];
        if (wave.w <= 0.0 || MaskId(wave.xy / nativeSize) != id) continue;
        vec2 delta = p * nativeSize - wave.xy;
        float distance = length(delta);
        float ring = (distance - wave.z) / 8.0;
        if (abs(ring) > 3.0 || distance < 0.01) continue;
        // Strong crest plus opposing trough: a visible refractive shock front,
        // rather than simply nudging the whole patch outward.
        float pulse = (1.0 - ring * ring) * exp(-0.5 * ring * ring);
        offset += delta / distance * wave.w * pulse;
    }
    float magnitude = length(offset);
    if (magnitude < 0.001) return p;
    offset *= min(1.0, 6.0 / magnitude); // Native-pixel displacement cap.
    vec2 target = p + offset / nativeSize;
    vec4 bounds = viewports[int(id) - 1];
    if (any(lessThan(target, bounds.xy)) || any(greaterThanEqual(target, bounds.zw))) return p;
    // Check the short path as well as the full bilinear sample footprint.
    for (int i = 1; i <= 8; ++i)
        if (MaskId(mix(p, target, float(i) * 0.125)) != id) return p;
    vec2 base = floor(target * nativeSize - 0.5);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
            if (MaskId((base + vec2(x,y) + 0.5) / nativeSize) != id) return p;
    return target;
}
vec3 ApplyGlow(vec3 base, vec2 p) {
    if (effectsOn == 0 && flamesOn == 0) return base;
    vec4 center = texelFetch(glowImage, clamp(ivec2(p * vec2(640,480)), ivec2(0), ivec2(639,479)), 0);
    float viewport = floor(center.a * 255.0 + 0.5);
    if (viewport == 0.0) return base; // Hidden world, HUD, borders, or overlays.
    vec3 halo = effectsOn != 0 ? texture(bloomImage, p).rgb : vec3(0);
    vec3 flame = flamesOn != 0 ? texture(flameImage, p).rgb : vec3(0);
    // Only filtered light: the native-resolution core made enlarged halos blocky.
    vec3 light = clamp(halo * 2.6 + flame * 0.75, 0.0, 0.90);
    return vec3(1.0) - (vec3(1.0) - base) * (vec3(1.0) - light);
}
)GLSL";

const char *bloomWrapper = R"GLSL(
in vec2 uv;
out vec4 color;
uniform sampler2D image;
uniform sampler2D glowImage;
uniform vec2 direction;
uniform vec4 viewports[4];
float viewportId;
vec4 bounds;
vec4 GatedSample(sampler2D source, vec2 p) {
    vec2 halfTexel = vec2(0.5/640.0, 0.5/480.0);
    p = clamp(p, bounds.xy + halfTexel, bounds.zw - halfTexel);
    ivec2 ip = clamp(ivec2(p * vec2(640,480)), ivec2(0), ivec2(639,479));
    float id = floor(texelFetch(glowImage, ip, 0).a * 255.0 + 0.5);
    return id == viewportId ? texture(source, p) : vec4(0.0);
}
#define texture2D GatedSample
)GLSL";
const char *bloomMain = R"GLSL(
#undef texture2D
void main() {
    ivec2 ip = clamp(ivec2(uv * vec2(640,480)), ivec2(0), ivec2(639,479));
    viewportId = floor(texelFetch(glowImage, ip, 0).a * 255.0 + 0.5);
    if (viewportId < 1.0 || viewportId > 4.0) { color = vec4(0.0); return; }
    bounds = viewports[int(viewportId) - 1];
    color = vec4(blur13(image, uv, vec2(640,480), direction).rgb, 1.0);
}
)GLSL";

// GLSL 4.1 (macOS) lacks core half packing. These IEEE binary16 conversion
// helpers also let the unmodified AMD portability header compile there.
const char *halfPacking = R"GLSL(
uint touHalf(float v) {
    uint b = floatBitsToUint(v), s = (b >> 16) & 0x8000u;
    uint m = b & 0x7fffffu, e = (b >> 23) & 255u;
    if (e == 255u) return s | 0x7c00u | (m != 0u ? 0x200u : 0u);
    int h = int(e) - 112;
    if (h >= 31) return s | 0x7c00u;
    if (h < -10) return s;
    if (h <= 0) {
        m |= 0x800000u;
        uint shift = uint(14 - h), base = m >> shift;
        uint rem = m & ((1u << shift) - 1u), mid = 1u << (shift - 1u);
        return s | (base + uint(rem > mid || (rem == mid && (base & 1u) != 0u)));
    }
    uint base = (uint(h) << 10) | (m >> 13);
    uint rem = m & 8191u;
    return s | (base + uint(rem > 4096u || (rem == 4096u && (base & 1u) != 0u)));
}
float touFloat(uint h) {
    float v = (h & 0x8000u) != 0u ? -1.0 : 1.0;
    uint e = (h >> 10) & 31u, m = h & 1023u;
    if (e == 0u) return v * float(m) * exp2(-24.0);
    if (e == 31u) return uintBitsToFloat(((h & 0x8000u) << 16) | 0x7f800000u | (m << 13));
    return v * (1.0 + float(m) / 1024.0) * exp2(float(int(e) - 15));
}
uint touPack(vec2 v) { return touHalf(v.x) | (touHalf(v.y) << 16); }
vec2 touUnpack(uint v) { return vec2(touFloat(v & 65535u), touFloat(v >> 16)); }
#define packHalf2x16 touPack
#define unpackHalf2x16 touUnpack
)GLSL";

const char *easuCallbacks = R"GLSL(
in vec2 uv;
out vec4 color;
uniform sampler2D image;
uniform vec2 inputSize;
uniform vec2 outputSize;
vec4 FsrEasuRF(vec2 p) { return textureGather(image, p, 0); }
vec4 FsrEasuGF(vec2 p) { return textureGather(image, p, 1); }
vec4 FsrEasuBF(vec2 p) { return textureGather(image, p, 2); }
)GLSL";
const char *easuMain = R"GLSL(
void main() {
    uvec4 c0, c1, c2, c3;
    FsrEasuCon(c0, c1, c2, c3, inputSize.x, inputSize.y,
              inputSize.x, inputSize.y, outputSize.x, outputSize.y);
    vec3 result;
    FsrEasuF(result, uvec2(gl_FragCoord.xy), c0, c1, c2, c3);
    color = vec4(result, 1.0);
}
)GLSL";
const char *rcasCallbacks = R"GLSL(
in vec2 uv;
out vec4 color;
uniform sampler2D image;
uniform vec2 outputSize;
vec4 FsrRcasLoadF(ivec2 p) {
    return texelFetch(image, clamp(p, ivec2(0), ivec2(outputSize) - 1), 0);
}
void FsrRcasInputF(inout float r, inout float g, inout float b) { }
)GLSL";
const char *rcasMain = R"GLSL(
void main() {
    uvec4 con;
    FsrRcasCon(con, -log2(0.70)); // Accepted visual preset: 70% strength.
    vec3 result;
    vec2 warped = WarpCoords(uv);
    uvec2 p = uvec2(clamp(warped * outputSize, vec2(0.0), outputSize - 1.0));
    FsrRcasF(result.r, result.g, result.b, p, con);
    color = vec4(ApplyGlow(result, warped), 1.0);
}
)GLSL";

GLuint Compile(GLenum type, const std::string &source)
{
    GLuint shader = pCreateShader(type);
    const char *text = source.c_str();
    pShaderSource(shader, 1, &text, nullptr);
    pCompileShader(shader);
    GLint ok = 0;
    pGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char error[4096] = {};
        pGetShaderInfoLog(shader, sizeof(error), nullptr, error);
        LOG("[SHADER] Compile failed: %s\n", error);
        pDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint Program(const std::string &prefix, const std::string &fragment)
{
    GLuint vs = Compile(GL_VERTEX_SHADER, prefix + vertexSource);
    GLuint fs = Compile(GL_FRAGMENT_SHADER, prefix + fragment);
    GLuint program = 0;
    if (vs && fs) {
        program = pCreateProgram();
        pAttachShader(program, vs);
        pAttachShader(program, fs);
        pLinkProgram(program);
        GLint ok = 0;
        pGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char error[4096] = {};
            pGetProgramInfoLog(program, sizeof(error), nullptr, error);
            LOG("[SHADER] Link failed: %s\n", error);
            pDeleteProgram(program);
            program = 0;
        }
    }
    if (vs) pDeleteShader(vs);
    if (fs) pDeleteShader(fs);
    return program;
}

void TextureParameters(GLint filter)
{
    pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Draw(GLuint program, GLuint texture, int width, int height, bool flip, bool pixelArt)
{
    pUseProgram(program);
    pActiveTexture(GL_TEXTURE0);
    pBindTexture(GL_TEXTURE_2D, texture);
    pUniform1i(pGetUniformLocation(program, "image"), 0);
    pUniform1i(pGetUniformLocation(program, "glowImage"), 1);
    pUniform1i(pGetUniformLocation(program, "bloomImage"), 2);
    pUniform1i(pGetUniformLocation(program, "flameImage"), 3);
    pUniform1i(pGetUniformLocation(program, "flamesOn"), ShaderEffects_FlamePixels() ? 1 : 0);
    pUniform1i(pGetUniformLocation(program, "effectsOn"), bloomReady ? 1 : 0);
    pUniform1i(pGetUniformLocation(program, "distortionOn"), ShaderEffects_HasShockwaves() ? 1 : 0);
    pUniform4fv(pGetUniformLocation(program, "shockwaves"), 64, ShaderEffects_Shockwaves());
    pUniform4fv(pGetUniformLocation(program, "viewports"), 4, ShaderEffects_ViewportBounds());
    pUniform1i(pGetUniformLocation(program, "flip"), flip ? 1 : 0);
    pUniform1i(pGetUniformLocation(program, "pixelArt"), pixelArt ? 1 : 0);
    pUniform2f(pGetUniformLocation(program, "inputSize"), (float)inputWidth, (float)inputHeight);
    pUniform2f(pGetUniformLocation(program, "outputSize"), (float)width, (float)height);
    pBindVertexArray(vao);
    pDrawArrays(GL_TRIANGLES, 0, 3);
}

bool ResizeFsrTarget(int width, int height)
{
    if (width == outputWidth && height == outputHeight) return fsrTargetValid;
    outputWidth = width;
    outputHeight = height;
    pBindTexture(GL_TEXTURE_2D, easuTexture);
    TextureParameters(GL_NEAREST);
    pTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    pBindFramebuffer(GL_FRAMEBUFFER, easuFbo);
    pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, easuTexture, 0);
    fsrTargetValid = pCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE && pGetError() == GL_NO_ERROR;
    pBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!fsrTargetValid)
        LOG("[SHADER] FSR target %dx%d unavailable; using filtered presentation\n", width, height);
    else
        LOG("[SHADER] FSR EASU/RCAS target ready: %dx%d\n", width, height);
    return fsrTargetValid;
}
} // namespace

bool ShaderPresentation_Initialize(void *platformWindow)
{
    window = static_cast<SDL_Window *>(platformWindow);
    context = SDL_GL_CreateContext(window);
    if (!context) {
        LOG("[SHADER] Context unavailable: %s\n", SDL_GetError());
        window = nullptr;
        return false;
    }
#define LOAD_GL(result, name, args) \
    p##name = reinterpret_cast<result (APIENTRY *) args>(SDL_GL_GetProcAddress("gl" #name)); \
    if (!p##name) { LOG("[SHADER] Missing gl%s\n", #name); SDL_GL_DestroyContext(context); context = nullptr; window = nullptr; return false; }
    GL_FUNCTIONS(LOAD_GL)
#undef LOAD_GL
    int profile = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);
    const std::string prefix = profile == SDL_GL_CONTEXT_PROFILE_ES
        ? "#version 310 es\nprecision highp float;\nprecision highp int;\n"
        : "#version 410 core\n";
    plainProgram = Program(prefix, std::string(glowSource) + plainSource);
    const std::string common = std::string(halfPacking) + "\n#define A_GPU 1\n#define A_GLSL 1\n" + JoinSources(kFsrPortability);
    const std::string algorithm = JoinSources(kFsrAlgorithm);
    easuProgram = Program(prefix, common + "\n#define FSR_EASU_F 1\n" + easuCallbacks + algorithm + easuMain);
    rcasProgram = Program(prefix, common + "\n#define FSR_RCAS_F 1\n#define FSR_RCAS_DENOISE 1\n" + rcasCallbacks + algorithm + glowSource + rcasMain);
    std::string gaussian = JoinSources(kGaussianBlur);
    const size_t pragma = gaussian.find("#pragma glslify:");
    if (pragma != std::string::npos) gaussian.erase(pragma);
    bloomProgram = Program(prefix, std::string(bloomWrapper) + gaussian + bloomMain);
    if (!plainProgram || !easuProgram || !rcasProgram || !bloomProgram) {
        ShaderPresentation_Shutdown();
        return false;
    }
    pGenVertexArrays(1, &vao);
    pGenTextures(1, &input);
    pGenTextures(1, &glowTexture);
    pBindTexture(GL_TEXTURE_2D, glowTexture);
    TextureParameters(GL_LINEAR);
    pTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    pGenTextures(1, &flameTexture);
    pActiveTexture(GL_TEXTURE3);
    pBindTexture(GL_TEXTURE_2D, flameTexture);
    TextureParameters(GL_LINEAR);
    pTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    pActiveTexture(GL_TEXTURE0);
    pGenTextures(1, &easuTexture);
    pGenFramebuffers(1, &easuFbo);
    pGenTextures(2, bloomTextures);
    pGenFramebuffers(2, bloomFbos);
    for (int i = 0; i < 2; ++i) {
        pBindTexture(GL_TEXTURE_2D, bloomTextures[i]);
        TextureParameters(GL_LINEAR);
        pTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        pBindFramebuffer(GL_FRAMEBUFFER, bloomFbos[i]);
        pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomTextures[i], 0);
        if (!bloomTextures[i] || !bloomFbos[i] || pCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            ShaderPresentation_Shutdown();
            return false;
        }
    }
    pBindFramebuffer(GL_FRAMEBUFFER, 0);
    pActiveTexture(GL_TEXTURE2);
    pBindTexture(GL_TEXTURE_2D, bloomTextures[1]);
    pActiveTexture(GL_TEXTURE0);
    pDisable(GL_DEPTH_TEST);
    pDisable(GL_BLEND);
    if (!vao || !input || !glowTexture || !flameTexture || !easuTexture || !easuFbo || pGetError() != GL_NO_ERROR) {
        ShaderPresentation_Shutdown();
        return false;
    }
    SDL_GL_SetSwapInterval(1);
    LOG("[SHADER] Experimental FSR1 presentation ready: %s / %s\n",
        reinterpret_cast<const char *>(pGetString(GL_RENDERER)),
        reinterpret_cast<const char *>(pGetString(GL_VERSION)));
    return true;
}

bool ShaderPresentation_Available(void) { return context != nullptr; }

void ShaderPresentation_Shutdown(void)
{
    if (context) {
        SDL_GL_MakeCurrent(window, context);
        if (input) pDeleteTextures(1, &input);
        if (glowTexture) pDeleteTextures(1, &glowTexture);
        if (flameTexture) pDeleteTextures(1, &flameTexture);
        pDeleteTextures(2, bloomTextures);
        pDeleteFramebuffers(2, bloomFbos);
        if (bloomProgram) pDeleteProgram(bloomProgram);
        if (easuTexture) pDeleteTextures(1, &easuTexture);
        if (easuFbo) pDeleteFramebuffers(1, &easuFbo);
        if (vao) pDeleteVertexArrays(1, &vao);
        if (plainProgram) pDeleteProgram(plainProgram);
        if (easuProgram) pDeleteProgram(easuProgram);
        if (rcasProgram) pDeleteProgram(rcasProgram);
        SDL_GL_DestroyContext(context);
    }
    context = nullptr;
    window = nullptr;
    input = easuTexture = easuFbo = vao = 0;
    glowTexture = flameTexture = 0;
    bloomTextures[0] = bloomTextures[1] = bloomFbos[0] = bloomFbos[1] = bloomProgram = 0;
    bloomReady = false;
    reportedBloom = false;
    plainProgram = easuProgram = rcasProgram = 0;
    inputWidth = inputHeight = outputWidth = outputHeight = 0;
    fsrTargetValid = false;
}

bool ShaderPresentation_Present(const Framebuffer *frame)
{
    if (!context || !frame || !frame->pixels || frame->width <= 0 ||
        frame->height <= 0 || frame->stride < frame->width) return false;
    if (!SDL_GL_MakeCurrent(window, context)) return false;
    pActiveTexture(GL_TEXTURE0);
    pBindTexture(GL_TEXTURE_2D, input);
    TextureParameters(g_PresentationScaling == PRESENTATION_CRISP ? GL_NEAREST : GL_LINEAR);
    pPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    pPixelStorei(GL_UNPACK_ROW_LENGTH, frame->stride);
    if (inputWidth != frame->width || inputHeight != frame->height) {
        inputWidth = frame->width;
        inputHeight = frame->height;
        pTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, inputWidth, inputHeight, 0,
                    GL_RGB, GL_UNSIGNED_SHORT_5_6_5, frame->pixels);
    } else {
        pTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, inputWidth, inputHeight,
                       GL_RGB, GL_UNSIGNED_SHORT_5_6_5, frame->pixels);
    }
    pPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    const unsigned char *glowPixels = ShaderEffects_GlowPixels();
    const unsigned char *flamePixels = ShaderEffects_FlamePixels();
    bloomReady = false;
    if (glowPixels) {
        pActiveTexture(GL_TEXTURE1);
        pBindTexture(GL_TEXTURE_2D, glowTexture);
        pTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 480,
                    GL_RGBA, GL_UNSIGNED_BYTE, glowPixels);
    }
    if (flamePixels) {
        pActiveTexture(GL_TEXTURE3);
        pBindTexture(GL_TEXTURE_2D, flameTexture);
        pTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 480,
                      GL_RGBA, GL_UNSIGNED_BYTE, flamePixels);
    }
    pActiveTexture(GL_TEXTURE0);
    if (pGetError() != GL_NO_ERROR) {
        LOG("[SHADER] Framebuffer upload failed\n");
        return false;
    }
    int windowWidth = 0, windowHeight = 0, pixelWidth = 0, pixelHeight = 0;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);
    if (windowWidth <= 0 || windowHeight <= 0 || pixelWidth <= 0 || pixelHeight <= 0) return true;
    PresentationRect rect;
    Get_Game_Presentation_Rect(&rect);
    const int x = rect.left * pixelWidth / windowWidth;
    const int right = rect.right * pixelWidth / windowWidth;
    const int top = rect.top * pixelHeight / windowHeight;
    const int bottom = rect.bottom * pixelHeight / windowHeight;
    const int width = right - x, height = bottom - top;
    if (width <= 0 || height <= 0) return true;
    if (glowPixels && ShaderEffects_HasGlow()) {
        pViewport(0, 0, 640, 480);
        // Repeat the contiguous optimized kernel instead of stretching its
        // bilinear taps apart (which leaves gaps and square lobes).
        for (int pass = 0; pass < 8; ++pass) {
            pBindFramebuffer(GL_FRAMEBUFFER, bloomFbos[pass & 1]);
            pUseProgram(bloomProgram);
            pUniform2f(pGetUniformLocation(bloomProgram, "direction"), (pass & 1) ? 0.0f : 1.0f, (pass & 1) ? 1.0f : 0.0f);
            Draw(bloomProgram, pass == 0 ? glowTexture : bloomTextures[(pass - 1) & 1], 640, 480, false, false);
        }
        bloomReady = pGetError() == GL_NO_ERROR;
        if (!reportedBloom) {
            LOG("[SHADER] Gaussian bloom passes: %s\n", bloomReady ? "active" : "failed (effect disabled)");
            reportedBloom = true;
        }
        pActiveTexture(GL_TEXTURE2);
        pBindTexture(GL_TEXTURE_2D, bloomTextures[1]);
        pActiveTexture(GL_TEXTURE0);
    }
    pBindFramebuffer(GL_FRAMEBUFFER, 0);
    pViewport(0, 0, pixelWidth, pixelHeight);
    pClearColor(0, 0, 0, 1);
    pClear(GL_COLOR_BUFFER_BIT);
    // EASU is an upscaler, not a downsampling filter.
    const bool fsr = g_PresentationScaling == PRESENTATION_FSR &&
        width >= inputWidth && height >= inputHeight && ResizeFsrTarget(width, height);
    if (fsr) {
        pBindFramebuffer(GL_FRAMEBUFFER, easuFbo);
        pViewport(0, 0, width, height);
        Draw(easuProgram, input, width, height, false, false);
        pBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    pViewport(x, pixelHeight - bottom, width, height);
    Draw(fsr ? rcasProgram : plainProgram, fsr ? easuTexture : input,
         width, height, true, g_PresentationScaling == PRESENTATION_PIXEL_ART);
    if (pGetError() != GL_NO_ERROR) {
        LOG("[SHADER] Presentation failed\n");
        return false;
    }
    if (Screenshot_Pending()) {
        // Read the final backbuffer after FSR, bloom, flames and refraction.
        SDL_Surface *surface = SDL_CreateSurface(pixelWidth, pixelHeight, SDL_PIXELFORMAT_RGBA32);
        if (!surface) Screenshot_Failed(SDL_GetError());
        else {
            pReadBuffer(GL_BACK);
            pPixelStorei(GL_PACK_ALIGNMENT, 1);
            pPixelStorei(GL_PACK_ROW_LENGTH, surface->pitch / 4);
            pReadPixels(0, 0, pixelWidth, pixelHeight, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
            const GLenum error = pGetError();
            pPixelStorei(GL_PACK_ROW_LENGTH, 0);
            pPixelStorei(GL_PACK_ALIGNMENT, 4);
            if (error != GL_NO_ERROR) Screenshot_Failed("OpenGL framebuffer readback failed");
            else if (!SDL_FlipSurface(surface, SDL_FLIP_VERTICAL)) Screenshot_Failed(SDL_GetError());
            else Screenshot_Save(surface);
            SDL_DestroySurface(surface);
        }
    }
    return SDL_GL_SwapWindow(window);
}
