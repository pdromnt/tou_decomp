#include "tou_level/level.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "stb_image.h"

#ifndef TOU_ASSET_ROOT
#define TOU_ASSET_ROOT ""
#endif

namespace {

constexpr float kSidebarWidth = 310.0F;
constexpr float kToolbarHeight = 52.0F;
constexpr float kStatusBarHeight = 28.0F;
constexpr float kContentTop = kToolbarHeight + 14.0F;
constexpr float kPaletteRowHeight = 24.0F;
constexpr std::size_t kHistoryLimit = 64;
constexpr std::array<int, 7> kTurretSpriteBases{
    13500, 13000, 12500, 11000, 11500, 14000, 12000,
};

enum class EditMode {
    Terrain,
    Placements,
    LevelRules,
    GroundGeneration,
};

enum class TextField {
    None,
    Maker,
    Email,
    SignFirst,
    SignSecond,
    ThemeName,
};

struct EditorSnapshot {
    std::vector<std::uint8_t> attributes;
    std::vector<tou::level::Placement> placements;
    tou::level::LevelConfig config;
    std::filesystem::path parallax_path;
    std::vector<std::uint8_t> parallax_payload;
};

struct PlacementTemplate {
    const char *name;
    tou::level::PlacementType type;
    std::array<std::uint8_t, 5> parameters;
};

struct SpritePreview {
    SDL_Texture *texture = nullptr;
    float width = 0.0F;
    float height = 0.0F;
};

struct FileDialogResult {
    std::optional<std::filesystem::path> path;
    std::string error;
};

struct FileDialogRequest {
    std::promise<FileDialogResult> completion;
};

void SDLCALL CompleteFileDialog(void *userdata,
                                const char *const *filelist, int) {
    auto *request = static_cast<FileDialogRequest *>(userdata);
    FileDialogResult result;
    if (filelist == nullptr) {
        result.error = SDL_GetError();
    } else if (filelist[0] != nullptr) {
        result.path = std::filesystem::path(filelist[0]);
    }
    request->completion.set_value(std::move(result));
}

std::optional<std::filesystem::path> WaitForFileDialog(
    SDL_Window *window, bool save, const SDL_DialogFileFilter *filters,
    int filter_count, const std::string &default_location) {
    FileDialogRequest request;
    std::future<FileDialogResult> future = request.completion.get_future();
    if (save) {
        SDL_ShowSaveFileDialog(CompleteFileDialog, &request, window, filters,
                               filter_count, default_location.c_str());
    } else {
        SDL_ShowOpenFileDialog(CompleteFileDialog, &request, window, filters,
                               filter_count, default_location.empty()
                                   ? nullptr : default_location.c_str(), false);
    }
    while (future.wait_for(std::chrono::milliseconds(10)) !=
           std::future_status::ready) {
        SDL_PumpEvents();
        SDL_Delay(10);
    }
    FileDialogResult result = future.get();
    if (!result.error.empty()) {
        throw std::runtime_error("File dialog failed: " + result.error);
    }
    return result.path;
}

int ShowChoice(SDL_Window *window, const char *title, const char *message,
               const SDL_MessageBoxButtonData *buttons, int button_count) {
    SDL_MessageBoxData data{};
    data.flags = SDL_MESSAGEBOX_INFORMATION;
    data.window = window;
    data.title = title;
    data.message = message;
    data.numbuttons = button_count;
    data.buttons = buttons;
    int choice = -1;
    if (!SDL_ShowMessageBox(&data, &choice)) {
        throw std::runtime_error(SDL_GetError());
    }
    return choice;
}

std::filesystem::path ProjectSuffix(std::filesystem::path path) {
    const std::string suffix = ".toulevel.json";
    std::string value = path.string();
    if (value.size() < suffix.size() ||
        value.compare(value.size() - suffix.size(), suffix.size(), suffix) != 0) {
        value += suffix;
    }
    return std::filesystem::path(value);
}

std::filesystem::path AssetRoot() {
    const char *base_text = SDL_GetBasePath();
    const std::filesystem::path base = base_text == nullptr
        ? std::filesystem::path{} : std::filesystem::path(base_text);
    const std::array<std::filesystem::path, 5> candidates{{
        base,
        base / "..",
        base / "../TOU.app/Contents/Resources",
        base / "../Resources",
        std::filesystem::path(TOU_ASSET_ROOT),
    }};
    for (const std::filesystem::path &candidate : candidates) {
        if (std::filesystem::is_directory(candidate / "data") &&
            std::filesystem::is_directory(candidate / "ggstuff")) {
            return std::filesystem::absolute(candidate).lexically_normal();
        }
    }
    return std::filesystem::absolute(std::filesystem::path(TOU_ASSET_ROOT));
}

std::optional<std::filesystem::path> ChooseStartupProject() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(SDL_GetError());
    }
    SDL_Window *window = SDL_CreateWindow("TOU Level Editor", 560, 260, 0);
    if (window == nullptr) {
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }

    std::optional<std::filesystem::path> result;
    try {
        static constexpr SDL_MessageBoxButtonData startup_buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
            {0, 1, "Open"},
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 2, "New normal"},
            {0, 3, "New GG level"},
        };
        const int action = ShowChoice(
            window, "TOU Level Editor",
            "Open a .toulevel.json project or import an existing .lev level.\n"
            "You can also create a new normal or GG level from scratch.",
            startup_buttons, 4);

        static constexpr SDL_DialogFileFilter open_filter[] = {
            {"TOU project or compiled level", "toulevel.json;lev"},
            {"TOU level project", "toulevel.json"},
            {"Compiled TOU level", "lev"},
        };
        static constexpr SDL_DialogFileFilter project_filter[] = {
            {"TOU level project", "toulevel.json"},
            {"JSON", "json"},
        };
        static constexpr SDL_DialogFileFilter jpeg_filter[] = {
            {"JPEG image", "jpg;jpeg"},
        };

        if (action == 1) {
            const auto opened = WaitForFileDialog(window, false, open_filter, 3, "");
            std::string extension = opened ? opened->extension().string() : "";
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char character) {
                               return static_cast<char>(std::tolower(character));
                           });
            if (opened && extension == ".lev") {
                const std::filesystem::path suggested =
                    opened->parent_path() /
                    (opened->stem().string() + ".toulevel.json");
                const auto project = WaitForFileDialog(
                    window, true, project_filter, 2, suggested.string());
                if (project) {
                    const std::filesystem::path path = ProjectSuffix(*project);
                    tou::level::ImportLevelProject(*opened, path);
                    result = path;
                }
            } else {
                result = opened;
            }
        } else if (action == 2) {
            const auto visual = WaitForFileDialog(window, false, jpeg_filter, 1, "");
            if (visual) {
                static constexpr SDL_MessageBoxButtonData parallax_buttons[] = {
                    {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No parallax"},
                    {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Choose parallax"},
                };
                std::filesystem::path parallax;
                if (ShowChoice(window, "Optional parallax",
                               "Does this normal level use a parallax JPEG?",
                               parallax_buttons, 2) == 1) {
                    const auto chosen = WaitForFileDialog(
                        window, false, jpeg_filter, 1, visual->parent_path().string());
                    if (!chosen) throw std::runtime_error("Parallax selection canceled");
                    parallax = *chosen;
                }
                const std::filesystem::path suggested =
                    visual->parent_path() /
                    (visual->stem().string() + ".toulevel.json");
                const auto project = WaitForFileDialog(
                    window, true, project_filter, 2, suggested.string());
                if (project) {
                    const std::filesystem::path path = ProjectSuffix(*project);
                    tou::level::CreateNormalProject(path, *visual, parallax);
                    result = path;
                }
            }
        } else if (action == 3) {
            static constexpr SDL_MessageBoxButtonData size_buttons[] = {
                {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
                {0, 1, "640 x 480"},
                {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 2, "1280 x 720"},
                {0, 3, "1920 x 1080"},
            };
            const int size = ShowChoice(
                window, "New GG level", "Choose the constraint-map dimensions.",
                size_buttons, 4);
            if (size != 0) {
                static constexpr std::array<std::pair<std::uint16_t, std::uint16_t>, 4>
                    dimensions{{{0, 0}, {640, 480}, {1280, 720}, {1920, 1080}}};
                const auto project = WaitForFileDialog(
                    window, true, project_filter, 2, "new-gg.toulevel.json");
                if (project) {
                    const std::filesystem::path path = ProjectSuffix(*project);
                    tou::level::CreateGroundGeneratedProject(
                        path, dimensions[size].first, dimensions[size].second,
                        "the earth");
                    result = path;
                }
            }
        }
    } catch (...) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        throw;
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}

const std::array<PlacementTemplate, 9> &PlacementTemplates() {
    static const std::array<PlacementTemplate, 9> templates{{
        {"Turret", tou::level::PlacementType::Turret, {0, 0, 0, 0, 0}},
        {"Gate", tou::level::PlacementType::Gate, {0, 0, 0, 0, 0}},
        {"Smoke generator", tou::level::PlacementType::Object, {0, 0, 0, 0, 0}},
        {"Flame generator", tou::level::PlacementType::Object, {1, 0, 0, 0, 0}},
        {"Mine", tou::level::PlacementType::Object, {2, 0, 0, 0, 0}},
        {"Base building", tou::level::PlacementType::Object, {3, 0, 0, 0, 0}},
        {"Water creator", tou::level::PlacementType::Object, {4, 0, 0, 0, 0}},
        {"Starting place", tou::level::PlacementType::StartingPlace, {0, 0, 0, 0, 0}},
        {"Teleport", tou::level::PlacementType::Teleport, {0, 0, 0, 0, 0}},
    }};
    return templates;
}

class Editor {
public:
    explicit Editor(const std::filesystem::path &project_path)
        : project_(tou::level::LoadProject(project_path)),
          level_(tou::level::CompileProject(project_)) {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error(SDL_GetError());
        }
        window_ = SDL_CreateWindow("TOU Level Editor", 1280, 800,
                                   SDL_WINDOW_RESIZABLE);
        if (window_ == nullptr) {
            throw std::runtime_error(SDL_GetError());
        }
        SDL_SetWindowMinimumSize(window_, 1280, 600);
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (renderer_ == nullptr) {
            throw std::runtime_error(SDL_GetError());
        }
        SDL_SetRenderVSync(renderer_, 1);
        LoadThemes();
        LoadPlacementSpritePreviews();
        LoadVisualTexture();
        CreateAttributeTexture();
        FitCanvas();
        available_parallax_payload_ = level_.parallax_payload;
        saved_ = Snapshot();
        UpdateTitle("ready");
    }

    ~Editor() {
        for (auto &styles : turret_previews_) {
            for (auto &teams : styles) {
                for (SpritePreview &preview : teams) {
                    if (preview.texture != nullptr) SDL_DestroyTexture(preview.texture);
                }
            }
        }
        for (SpritePreview &preview : gate_previews_) {
            if (preview.texture != nullptr) SDL_DestroyTexture(preview.texture);
        }
        if (gg_preview_texture_ != nullptr) SDL_DestroyTexture(gg_preview_texture_);
        if (attribute_texture_ != nullptr) SDL_DestroyTexture(attribute_texture_);
        if (visual_texture_ != nullptr) SDL_DestroyTexture(visual_texture_);
        if (renderer_ != nullptr) SDL_DestroyRenderer(renderer_);
        if (window_ != nullptr) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    int Run() {
        while (running_) {
            SDL_Event event;
            bool close_seen = false;
            while (SDL_PollEvent(&event)) {
                const bool close_event = event.type == SDL_EVENT_QUIT ||
                    event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED;
                if (close_event && close_seen) continue;
                close_seen = close_seen || close_event;
                try {
                    HandleEvent(event);
                } catch (const std::exception &error) {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                             "TOU Level Editor",
                                             error.what(), window_);
                    UpdateTitle("operation failed");
                }
                if (!running_) break;
            }
            Render();
        }
        return 0;
    }

private:
    static std::uint16_t ReadLe16(std::istream &stream) {
        const int low = stream.get();
        const int high = stream.get();
        if (low == EOF || high == EOF) {
            throw std::runtime_error("Unexpected end of all3.gfx");
        }
        return static_cast<std::uint16_t>(low | (high << 8));
    }

    static std::vector<std::uint8_t> RawSpriteRgba(
        const std::vector<std::uint8_t> &bgr) {
        std::vector<std::uint8_t> rgba(bgr.size() / 3U * 4U);
        for (std::size_t pixel = 0; pixel < bgr.size() / 3U; ++pixel) {
            const std::uint8_t blue = bgr[pixel * 3U];
            const std::uint8_t green = bgr[pixel * 3U + 1U];
            const std::uint8_t red = bgr[pixel * 3U + 2U];
            rgba[pixel * 4U] = red;
            rgba[pixel * 4U + 1U] = green;
            rgba[pixel * 4U + 2U] = blue;
            rgba[pixel * 4U + 3U] =
                red == 0 && green == 0 && blue == 0 ? 0 : 255;
        }
        return rgba;
    }

    static std::vector<std::uint8_t> TintedTurretRgba(
        const std::vector<std::uint8_t> &bgr, int sprite_index,
        std::size_t team) {
        /* Exact type-9 transformation used by FUN_004236f0. The source image
         * channels are masks, not display colors, which is why showing the raw
         * all3.gfx pixels made the old selector preview the wrong color. */
        static constexpr std::array<std::uint16_t, 4> team_colors{
            0x1A56, 0x2ACA, 0x6508, 0x7FF0,
        };
        const std::uint16_t palette = team_colors[std::min(team, team_colors.size() - 1U)];
        int palette_rgb[3]{
            static_cast<int>((palette >> 10U) & 0x1FU) << 3,
            static_cast<int>((palette >> 5U) & 0x1FU) << 3,
            static_cast<int>(palette & 0x1FU) << 3,
        };
        int c0[3]{0x40, 0x40, 0x40};
        int c1[3]{0x40, 0x40, 0x40};
        int boundary[3]{0xB4, 0xB4, 0xB4};
        int upper[3]{0x122, 0x122, 0x122};
        int palette_scale = 0x80;
        switch ((sprite_index / 500) * 500) {
        case 11000:
            c0[0] = c0[1] = c0[2] = 0x60;
            boundary[1] = 0x100;
            break;
        case 11500:
            c0[0] = c0[1] = c0[2] = 0x80;
            boundary[0] = 0xC8; boundary[1] = 0x100;
            upper[0] = upper[1] = 0xFF;
            break;
        case 12000:
            c0[0] = 0xAA; c0[1] = 0xAC; c0[2] = 0x70;
            boundary[0] = 0xAA; boundary[1] = 0x100;
            upper[0] = 0xFF; upper[1] = 400;
            palette_scale = 0x8C;
            break;
        case 12500:
            c0[0] = 0x68; c0[1] = 0x7C; c0[2] = 0xE1;
            boundary[1] = 0xBE; boundary[2] = 0xA0;
            upper[1] = upper[2] = 0xFF;
            break;
        case 13000:
            c0[0] = 0x80;
            break;
        case 13500:
            c0[0] = c0[1] = c0[2] = 0x60;
            boundary[1] = 0xE6;
            upper[1] = 0x136;
            break;
        case 14000:
            c0[0] = c0[1] = 0x80; c0[2] = 0x40;
            c1[0] = c1[1] = 0x80; c1[2] = 0x60;
            boundary[0] = boundary[1] = boundary[2] = 0xDC;
            upper[1] = upper[2] = 300;
            break;
        default:
            break;
        }
        for (int &channel : palette_rgb) channel = channel * palette_scale >> 7;
        const int targets[9]{
            palette_rgb[0], palette_rgb[1], palette_rgb[2],
            c0[0], c0[1], c0[2], c1[0], c1[1], c1[2],
        };
        int lut[9][256]{};
        for (int input = 0; input < 3; ++input) {
            for (int output = 0; output < 3; ++output) {
                int *table = lut[input * 3 + output];
                int accumulator = 0;
                const int limit = std::min(boundary[input], 256);
                for (int value = 0; value < limit; ++value) {
                    table[value] = (accumulator / boundary[input]) >> 8;
                    accumulator += targets[input * 3 + output] * 256;
                }
                const int span = std::max(1, upper[input] - boundary[input]);
                int delta = 0;
                for (int value = limit; value < 256; ++value) {
                    table[value] = delta / span + targets[input * 3 + output];
                    delta += 256 - targets[input * 3 + output];
                }
            }
        }

        std::vector<std::uint8_t> rgba(bgr.size() / 3U * 4U);
        for (std::size_t pixel = 0; pixel < bgr.size() / 3U; ++pixel) {
            const int blue = (bgr[pixel * 3U] >> 3U) << 3U;
            const int green = (bgr[pixel * 3U + 1U] >> 3U) << 3U;
            const int red = (bgr[pixel * 3U + 2U] >> 3U) << 3U;
            if (red == 0 && green == 0 && blue == 0) continue;
            int output[3]{};
            for (int channel = 0; channel < 3; ++channel) {
                const int sum =
                    lut[channel][red] * lut[channel][red] +
                    lut[3 + channel][green] * lut[3 + channel][green] +
                    lut[6 + channel][blue] * lut[6 + channel][blue];
                output[channel] = std::min(255, static_cast<int>(std::sqrt(sum)));
            }
            if (output[0] < 8 && output[1] < 4 && output[2] < 8) continue;
            rgba[pixel * 4U] = static_cast<std::uint8_t>((output[0] >> 3U) << 3U);
            rgba[pixel * 4U + 1U] = static_cast<std::uint8_t>((output[1] >> 3U) << 3U);
            rgba[pixel * 4U + 2U] = static_cast<std::uint8_t>((output[2] >> 3U) << 3U);
            rgba[pixel * 4U + 3U] = 255;
        }
        return rgba;
    }

    SpritePreview CreateSpritePreview(const std::vector<std::uint8_t> &rgba,
                                      int width, int height) {
        SpritePreview preview;
        preview.texture = SDL_CreateTexture(
            renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
            width, height);
        if (preview.texture != nullptr) {
            SDL_UpdateTexture(preview.texture, nullptr, rgba.data(), width * 4);
            SDL_SetTextureBlendMode(preview.texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(preview.texture, SDL_SCALEMODE_NEAREST);
            preview.width = static_cast<float>(width);
            preview.height = static_cast<float>(height);
        }
        return preview;
    }

    void LoadPlacementSpritePreviews() {
        const std::filesystem::path path =
            AssetRoot() / "data/all3.gfx";
        std::ifstream stream(path, std::ios::binary);
        if (!stream) return;
        while (stream.peek() != EOF) {
            const int type = stream.get();
            const std::uint16_t index = ReadLe16(stream);
            stream.seekg(12, std::ios::cur);
            const std::uint16_t width = ReadLe16(stream);
            const std::uint16_t height = ReadLe16(stream);
            stream.seekg(2, std::ios::cur);
            const std::size_t byte_count =
                static_cast<std::size_t>(width) * height * 3U;
            std::vector<std::uint8_t> bgr(byte_count);
            stream.read(reinterpret_cast<char *>(bgr.data()),
                        static_cast<std::streamsize>(bgr.size()));
            if (!stream) return;

            if ((index == 58 || index == 59) && (type == 0 || type == 1)) {
                gate_previews_[index - 58] = CreateSpritePreview(
                    RawSpriteRgba(bgr), width, height);
                continue;
            }
            if (type == 9) {
                for (std::size_t style = 0; style < kTurretSpriteBases.size(); ++style) {
                    const int frame = static_cast<int>(index) - kTurretSpriteBases[style];
                    if (frame < 0 || frame >= 32) continue;
                    for (std::size_t team = 0; team < 4; ++team) {
                        turret_previews_[style][team][frame] = CreateSpritePreview(
                            TintedTurretRgba(bgr, index, team), width, height);
                    }
                    break;
                }
            }
        }
    }

    void LoadThemes() {
        themes_.clear();
        const std::filesystem::path root =
            AssetRoot() / "ggstuff";
        std::error_code error;
        for (const std::filesystem::directory_entry &entry :
             std::filesystem::directory_iterator(root, error)) {
            if (entry.is_directory()) {
                themes_.push_back(entry.path().filename().string());
            }
        }
        std::sort(themes_.begin(), themes_.end());
        if (themes_.empty() && !level_.config.gg_theme.empty()) {
            themes_.push_back(level_.config.gg_theme);
        }
    }

    void LoadVisualTexture() {
        if (project_.config.mode != tou::level::LevelMode::Normal ||
            project_.visual_path.empty()) {
            return;
        }
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char *pixels = stbi_load(project_.visual_path.string().c_str(),
                                          &width, &height, &channels, 4);
        if (pixels == nullptr) {
            throw std::runtime_error("Could not decode visual JPEG");
        }
        visual_texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                            SDL_TEXTUREACCESS_STATIC,
                                            width, height);
        if (visual_texture_ == nullptr ||
            !SDL_UpdateTexture(visual_texture_, nullptr, pixels, width * 4)) {
            stbi_image_free(pixels);
            throw std::runtime_error(SDL_GetError());
        }
        stbi_image_free(pixels);
    }

    void CreateAttributeTexture() {
        attribute_texture_ = SDL_CreateTexture(
            renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
            level_.width, level_.height);
        if (attribute_texture_ == nullptr) {
            throw std::runtime_error(SDL_GetError());
        }
        SDL_SetTextureBlendMode(attribute_texture_, SDL_BLENDMODE_BLEND);
        UpdateAttributeTexture();
    }

    void UpdateAttributeTexture() {
        const auto &palette = tou::level::Palette();
        std::vector<std::uint8_t> pixels(level_.attributes.size() * 4U);
        for (std::size_t index = 0; index < level_.attributes.size(); ++index) {
            const tou::level::PaletteEntry &entry = palette[level_.attributes[index]];
            pixels[index * 4U] = entry.rgb.r;
            pixels[index * 4U + 1U] = entry.rgb.g;
            pixels[index * 4U + 2U] = entry.rgb.b;
            pixels[index * 4U + 3U] = entry.index == 0
                ? 0 : static_cast<std::uint8_t>(overlay_alpha_);
        }
        if (!SDL_UpdateTexture(attribute_texture_, nullptr, pixels.data(),
                               static_cast<int>(level_.width) * 4)) {
            throw std::runtime_error(SDL_GetError());
        }
    }

    void FitCanvas() {
        int width = 1280;
        int height = 800;
        SDL_GetWindowSizeInPixels(window_, &width, &height);
        const float available_width = std::max(1.0F, width - kSidebarWidth - 24.0F);
        const float available_height =
            std::max(1.0F, height - kToolbarHeight - kStatusBarHeight - 24.0F);
        zoom_ = std::min(available_width / level_.width,
                         available_height / level_.height);
        zoom_ = std::clamp(zoom_, 0.05F, 32.0F);
        pan_x_ = 12.0F;
        pan_y_ = 12.0F;
    }

    SDL_FRect CanvasRect() const {
        return {
            kSidebarWidth + pan_x_,
            kToolbarHeight + pan_y_,
            level_.width * zoom_,
            level_.height * zoom_,
        };
    }

    bool MapPoint(float screen_x, float screen_y, int &map_x, int &map_y) const {
        int output_width = 0;
        int output_height = 0;
        SDL_GetRenderOutputSize(renderer_, &output_width, &output_height);
        static_cast<void>(output_width);
        if (screen_y >= static_cast<float>(output_height) - kStatusBarHeight) {
            return false;
        }
        const SDL_FRect canvas = CanvasRect();
        map_x = static_cast<int>(std::floor((screen_x - canvas.x) / zoom_));
        map_y = static_cast<int>(std::floor((screen_y - canvas.y) / zoom_));
        return map_x >= 0 && map_y >= 0 &&
               map_x < level_.width && map_y < level_.height;
    }

    EditorSnapshot Snapshot() const {
        return {level_.attributes, level_.placements, level_.config,
                project_.parallax_path, available_parallax_payload_};
    }

    static bool SameConfig(const tou::level::LevelConfig &left,
                           const tou::level::LevelConfig &right) {
        return left.maker == right.maker && left.email == right.email &&
               left.parallax == right.parallax &&
               left.civilians == right.civilians && left.bombing == right.bombing &&
               left.water == right.water &&
               left.disable_running_water == right.disable_running_water &&
               left.gravity_tenths == right.gravity_tenths &&
               left.resistance_tenths == right.resistance_tenths &&
               left.collision_damage_tenths == right.collision_damage_tenths &&
               left.bouncing_tenths == right.bouncing_tenths &&
               left.ambient == right.ambient &&
               left.parallax_aftertouch == right.parallax_aftertouch &&
               left.mode == right.mode && left.gg_theme == right.gg_theme &&
               left.gg_shape == right.gg_shape &&
               left.repair_density == right.repair_density &&
               left.stuff_density == right.stuff_density &&
               left.sign_density == right.sign_density &&
               left.random_seed == right.random_seed &&
               left.sign_texts.size() == right.sign_texts.size() &&
               std::equal(left.sign_texts.begin(), left.sign_texts.end(),
                          right.sign_texts.begin(),
                          [](const tou::level::SignText &a,
                             const tou::level::SignText &b) {
                              return a.first == b.first && a.second == b.second;
                          });
    }

    static bool SamePlacement(const tou::level::Placement &left,
                              const tou::level::Placement &right) {
        return left.x == right.x && left.y == right.y &&
               left.type == right.type && left.parameters == right.parameters;
    }

    bool IsSavedState() const {
        return level_.attributes == saved_.attributes &&
               level_.placements.size() == saved_.placements.size() &&
               std::equal(level_.placements.begin(), level_.placements.end(),
                          saved_.placements.begin(), SamePlacement) &&
               SameConfig(level_.config, saved_.config) &&
               project_.parallax_path == saved_.parallax_path &&
               available_parallax_payload_ == saved_.parallax_payload;
    }

    void Restore(const EditorSnapshot &snapshot) {
        level_.attributes = snapshot.attributes;
        level_.placements = snapshot.placements;
        level_.config = snapshot.config;
        project_.parallax_path = snapshot.parallax_path;
        available_parallax_payload_ = snapshot.parallax_payload;
        level_.parallax_payload = level_.config.parallax
            ? available_parallax_payload_ : std::vector<std::uint8_t>{};
        if (selected_placement_ && *selected_placement_ >= level_.placements.size()) {
            selected_placement_.reset();
        }
        pairing_source_.reset();
        if (selected_gg_rule_ >= GroundGenerationRuleCount()) {
            selected_gg_rule_ = GroundGenerationRuleCount() - 1U;
        }
        dirty_ = !IsSavedState();
        UpdateAttributeTexture();
        UpdateTitle(dirty_ ? "modified" : "ready");
    }

    void BeginEdit() {
        undo_.push_back(Snapshot());
        if (undo_.size() > kHistoryLimit) {
            undo_.erase(undo_.begin());
        }
        redo_.clear();
        if (level_.config.mode == tou::level::LevelMode::GroundGenerated)
            gg_preview_stale_ = true;
    }

    void Undo() {
        if (undo_.empty()) return;
        redo_.push_back(Snapshot());
        const EditorSnapshot snapshot = std::move(undo_.back());
        undo_.pop_back();
        Restore(snapshot);
    }

    void Redo() {
        if (redo_.empty()) return;
        undo_.push_back(Snapshot());
        const EditorSnapshot snapshot = std::move(redo_.back());
        redo_.pop_back();
        Restore(snapshot);
    }

    std::optional<std::size_t> HitPlacement(float screen_x, float screen_y) const {
        const SDL_FRect canvas = CanvasRect();
        const float radius = std::max(7.0F, zoom_ * 4.0F);
        for (std::size_t index = level_.placements.size(); index-- > 0;) {
            const tou::level::Placement &placement = level_.placements[index];
            const float x = canvas.x + placement.x * zoom_;
            const float y = canvas.y + placement.y * zoom_;
            if (std::abs(screen_x - x) <= radius &&
                std::abs(screen_y - y) <= radius) {
                return index;
            }
        }
        return std::nullopt;
    }

    void AddOrSelectPlacement(float screen_x, float screen_y) {
        if (const std::optional<std::size_t> hit = HitPlacement(screen_x, screen_y)) {
            if (pairing_source_) {
                const std::size_t source = *pairing_source_;
                pairing_source_.reset();
                if (source != *hit && source < level_.placements.size() &&
                    level_.placements[source].type == tou::level::PlacementType::Teleport &&
                    level_.placements[*hit].type == tou::level::PlacementType::Teleport) {
                    BeginEdit();
                    auto &first = level_.placements[source];
                    auto &second = level_.placements[*hit];
                    first.parameters[2] = second.parameters[0];
                    second.parameters[2] = first.parameters[0];
                    dirty_ = true;
                    selected_placement_ = hit;
                    UpdateTitle("teleports paired");
                    return;
                }
            }
            selected_placement_ = hit;
            dragging_placement_ = true;
            drag_edit_started_ = false;
            UpdateTitle(dirty_ ? "modified" : "ready");
            return;
        }
        if (pairing_source_) {
            UpdateTitle("click another teleport to pair (Esc cancels)");
            return;
        }
        int map_x = 0;
        int map_y = 0;
        if (!MapPoint(screen_x, screen_y, map_x, map_y) ||
            level_.placements.size() >= tou::level::kMaxPlacements) {
            return;
        }
        BeginEdit();
        const PlacementTemplate &source = PlacementTemplates()[selected_template_];
        tou::level::Placement placement;
        placement.x = map_x;
        placement.y = map_y;
        placement.type = source.type;
        placement.parameters = source.parameters;
        if (placement.type == tou::level::PlacementType::Teleport) {
            std::array<bool, 64> used{};
            for (const auto &existing : level_.placements) {
                if (existing.type == tou::level::PlacementType::Teleport &&
                    existing.parameters[0] < used.size()) {
                    used[existing.parameters[0]] = true;
                }
            }
            const auto available = std::find(used.begin(), used.end(), false);
            if (available != used.end()) {
                const std::uint8_t number = static_cast<std::uint8_t>(
                    std::distance(used.begin(), available));
                placement.parameters[0] = number;
                placement.parameters[2] = number;
            }
        }
        level_.placements.push_back(placement);
        selected_placement_ = level_.placements.size() - 1U;
        dragging_placement_ = true;
        drag_edit_started_ = true;
        dirty_ = true;
        UpdateTitle("modified");
    }

    void MoveSelectedPlacement(float screen_x, float screen_y) {
        if (!selected_placement_) return;
        int map_x = 0;
        int map_y = 0;
        if (!MapPoint(screen_x, screen_y, map_x, map_y)) return;
        tou::level::Placement &placement = level_.placements[*selected_placement_];
        if (placement.x == map_x && placement.y == map_y) return;
        if (!drag_edit_started_) {
            BeginEdit();
            drag_edit_started_ = true;
        }
        placement.x = map_x;
        placement.y = map_y;
        dirty_ = true;
        UpdateTitle("modified");
    }

    void DeleteSelectedPlacement() {
        if (!selected_placement_) return;
        BeginEdit();
        level_.placements.erase(level_.placements.begin() +
                                static_cast<std::ptrdiff_t>(*selected_placement_));
        selected_placement_.reset();
        pairing_source_.reset();
        dirty_ = true;
        UpdateTitle("modified");
    }

    void PairSelectedPlacement() {
        if (!selected_placement_) return;
        tou::level::Placement &placement = level_.placements[*selected_placement_];
        if (placement.type == tou::level::PlacementType::Gate) {
            BeginEdit();
            placement.parameters[1] = placement.parameters[1] == 0 ? 1 : 0;
            dirty_ = true;
            UpdateTitle(placement.parameters[1] ? "linked gate enabled"
                                                : "linked gate disabled");
        } else if (placement.type == tou::level::PlacementType::Teleport) {
            pairing_source_ = selected_placement_;
            UpdateTitle("click another teleport to pair");
        }
    }

    static int ParameterLimit(const tou::level::Placement &placement,
                              std::size_t parameter) {
        using tou::level::PlacementType;
        switch (placement.type) {
        case PlacementType::Turret:
            return std::array<int, 5>{6, 15, 3, 32, 0}[parameter];
        case PlacementType::Gate:
            return std::array<int, 5>{4, 1, 3, 3, 1}[parameter];
        case PlacementType::Object:
            if (placement.parameters[0] == 0) {
                return std::array<int, 5>{4, 7, 3, 3, 0}[parameter];
            }
            if (placement.parameters[0] == 2) {
                return std::array<int, 5>{4, 7, 0, 0, 0}[parameter];
            }
            if (placement.parameters[0] == 3) {
                return std::array<int, 5>{4, 3, 0, 0, 0}[parameter];
            }
            return parameter == 0 ? 4 : 0;
        case PlacementType::StartingPlace:
            return parameter == 0 ? 3 : 0;
        case PlacementType::Teleport:
            return std::array<int, 5>{63, 3, 63, 0, 0}[parameter];
        }
        return 0;
    }

    static const char *ParameterName(const tou::level::Placement &placement,
                                     std::size_t parameter) {
        using tou::level::PlacementType;
        static constexpr std::array<const char *, 5> turret{
            "Style", "Armor", "Team", "Direction", "Unused"};
        static constexpr std::array<const char *, 5> gate{
            "Motion profile", "Linked pair", "Team", "Facing", "Mirrored"};
        static constexpr std::array<const char *, 5> smoke{
            "Object type", "Direction", "Size", "Density", "Unused"};
        static constexpr std::array<const char *, 5> object{
            "Object type", "Setting", "Unused", "Unused", "Unused"};
        static constexpr std::array<const char *, 5> start{
            "Team", "Unused", "Unused", "Unused", "Unused"};
        static constexpr std::array<const char *, 5> teleport{
            "Number", "Team", "Target", "Unused", "Unused"};
        switch (placement.type) {
        case PlacementType::Turret: return turret[parameter];
        case PlacementType::Gate: return gate[parameter];
        case PlacementType::Object:
            return placement.parameters[0] == 0 ? smoke[parameter] : object[parameter];
        case PlacementType::StartingPlace: return start[parameter];
        case PlacementType::Teleport: return teleport[parameter];
        }
        return "Unknown";
    }

    static std::string ParameterValue(const tou::level::Placement &placement,
                                      std::size_t parameter) {
        using tou::level::PlacementType;
        const auto &p = placement.parameters;
        static constexpr std::array<const char *, 7> turret_styles{
            "Basic", "Heavy shot", "Guided missile", "Slow homing",
            "Spread shot", "Rapid fire", "Laser",
        };
        static constexpr std::array<const char *, 4> teams{
            "Team 1", "Team 2", "Team 3", "Team 4",
        };
        static constexpr std::array<const char *, 4> facings{
            "Right", "Down", "Left", "Up",
        };
        static constexpr std::array<const char *, 5> object_types{
            "Smoke generator", "Flame generator", "Mine", "Base building",
            "Water creator",
        };
        if (placement.type == PlacementType::Turret) {
            if (parameter == 0) return turret_styles[std::min<std::size_t>(p[0], 6U)];
            if (parameter == 1) return p[1] == 15 ? "Indestructible" : std::to_string(p[1] + 1U);
            if (parameter == 2) return teams[std::min<std::size_t>(p[2], 3U)];
            if (parameter == 3) return p[3] == 0 ? "Random" :
                std::to_string((p[3] - 1U) * 360U / 32U) + " degrees";
        } else if (placement.type == PlacementType::Gate) {
            if (parameter == 0) return "Profile " + std::to_string(p[0] + 1U);
            if (parameter == 1) return p[1] ? "Yes" : "No";
            if (parameter == 2) return teams[std::min<std::size_t>(p[2], 3U)];
            if (parameter == 3) return facings[std::min<std::size_t>(p[3], 3U)];
            if (parameter == 4) return p[4] ? "Yes (graphic 59)" : "No (graphic 58)";
        } else if (placement.type == PlacementType::Object) {
            if (parameter == 0) return object_types[std::min<std::size_t>(p[0], 4U)];
            if (p[0] == 0 && parameter == 1)
                return std::to_string(p[1] * 45U) + " degrees";
            if (p[0] == 0 && parameter == 2) {
                static constexpr std::array<const char *, 4> sizes{
                    "Huge", "Large", "Medium", "Small"};
                return sizes[std::min<std::size_t>(p[2], 3U)];
            }
            if (p[0] == 0 && parameter == 3) {
                static constexpr std::array<const char *, 4> densities{
                    "Sparse", "Normal", "Dense", "Very dense"};
                return densities[std::min<std::size_t>(p[3], 3U)];
            }
            if (p[0] == 2 && parameter == 1)
                return "Style " + std::to_string(p[1] + 1U);
            if (p[0] == 3 && parameter == 1)
                return teams[std::min<std::size_t>(p[1], 3U)];
        } else if (placement.type == PlacementType::StartingPlace && parameter == 0) {
            return teams[std::min<std::size_t>(p[0], 3U)];
        } else if (placement.type == PlacementType::Teleport) {
            if (parameter == 0) return "Teleport " + std::to_string(p[0] + 1U);
            if (parameter == 1) return teams[std::min<std::size_t>(p[1], 3U)];
            if (parameter == 2) return "Teleport " + std::to_string(p[2] + 1U);
        }
        return std::to_string(p[parameter]);
    }

    void ChangeSelectedParameter(int delta) {
        if (!selected_placement_) return;
        tou::level::Placement &placement = level_.placements[*selected_placement_];
        const int limit = ParameterLimit(placement, selected_parameter_);
        if (limit == 0) return;
        const int current = placement.parameters[selected_parameter_];
        const int next = std::clamp(current + delta, 0, limit);
        if (next == current) return;
        BeginEdit();
        placement.parameters[selected_parameter_] = static_cast<std::uint8_t>(next);
        if (placement.type == tou::level::PlacementType::Teleport &&
            selected_parameter_ == 0U) {
            for (auto &candidate : level_.placements) {
                if (candidate.type == tou::level::PlacementType::Teleport &&
                    candidate.parameters[2] == current) {
                    candidate.parameters[2] = static_cast<std::uint8_t>(next);
                }
            }
        }
        dirty_ = true;
        UpdateTitle("modified");
    }

    void Paint(float screen_x, float screen_y) {
        int map_x = 0;
        int map_y = 0;
        if (!MapPoint(screen_x, screen_y, map_x, map_y)) {
            return;
        }
        const int radius = brush_size_ / 2;
        for (int y = map_y - radius; y <= map_y + radius; ++y) {
            for (int x = map_x - radius; x <= map_x + radius; ++x) {
                if (x >= 0 && y >= 0 && x < level_.width && y < level_.height) {
                    level_.attributes[static_cast<std::size_t>(y) * level_.width + x] =
                        selected_palette_;
                }
            }
        }
        dirty_ = true;
        UpdateAttributeTexture();
        UpdateTitle("modified");
    }

    void Pick(float screen_x, float screen_y) {
        int map_x = 0;
        int map_y = 0;
        if (MapPoint(screen_x, screen_y, map_x, map_y)) {
            selected_palette_ =
                level_.attributes[static_cast<std::size_t>(map_y) * level_.width + map_x];
            UpdateTitle(dirty_ ? "modified" : "ready");
        }
    }

    void SelectPalette(float x, float y) {
        if (x < 8.0F || x >= kSidebarWidth - 8.0F || y < kContentTop) {
            return;
        }
        const int index = static_cast<int>(
            (y - kContentTop + sidebar_scroll_) / kPaletteRowHeight);
        if (index >= 0 && index < 34) {
            selected_palette_ = static_cast<std::uint8_t>(index);
            UpdateTitle(dirty_ ? "modified" : "ready");
        }
    }

    void SelectPlacementTemplate(float x, float y) {
        if (x < 8.0F || x >= kSidebarWidth - 8.0F || y < kContentTop) return;
        const int index = static_cast<int>(
            (y - kContentTop + sidebar_scroll_) / 58.0F);
        if (index >= 0 &&
            index < static_cast<int>(PlacementTemplates().size())) {
            selected_template_ = static_cast<std::size_t>(index);
            selected_placement_.reset();
            UpdateTitle(dirty_ ? "modified" : "ready");
            return;
        }
        if (selected_placement_) {
            const float help_y = kContentTop - sidebar_scroll_ +
                static_cast<float>(PlacementTemplates().size()) * 58.0F + 14.0F;
            const float property_y = y - (help_y + 66.0F);
            if (property_y >= 0.0F) {
                const std::size_t visible = static_cast<std::size_t>(property_y / 24.0F);
                std::size_t row = 0;
                const auto &placement = level_.placements[*selected_placement_];
                for (std::size_t parameter = 0; parameter < 5; ++parameter) {
                    if (ParameterLimit(placement, parameter) == 0 && parameter != 0)
                        continue;
                    if (row++ == visible) {
                        selected_parameter_ = parameter;
                        UpdateTitle(dirty_ ? "modified" : "ready");
                        return;
                    }
                }
            }
        }
    }

    static const std::array<const char *, 16> &RuleNames() {
        static constexpr std::array<const char *, 16> names{
            "Maker", "Email", "Parallax image", "Parallax enabled",
            "Civilians", "Bombing", "Water red", "Water green", "Water blue",
            "Disable running water", "Gravity x0.1", "Resistance x0.1",
            "Collision damage x0.1", "Bouncing x0.1", "Ambient",
            "Parallax aftertouch",
        };
        return names;
    }

    std::string RuleValue(std::size_t index) const {
        const tou::level::LevelConfig &config = level_.config;
        switch (index) {
        case 0: return config.maker;
        case 1: return config.email.empty() ? "(none)" : config.email;
        case 2: return project_.parallax_path.empty()
            ? "(none)" : project_.parallax_path.filename().string();
        case 3: return config.parallax ? "Yes" : "No";
        case 4: return std::to_string(config.civilians);
        case 5: return std::to_string(config.bombing);
        case 6: return std::to_string(config.water.r);
        case 7: return std::to_string(config.water.g);
        case 8: return std::to_string(config.water.b);
        case 9: return config.disable_running_water ? "Yes" : "No";
        case 10: return std::to_string(config.gravity_tenths);
        case 11: return std::to_string(config.resistance_tenths);
        case 12: return std::to_string(config.collision_damage_tenths);
        case 13: return std::to_string(config.bouncing_tenths);
        case 14: return std::to_string(config.ambient);
        case 15: return std::to_string(config.parallax_aftertouch);
        default: return "?";
        }
    }

    void BeginTextEdit(TextField field, std::size_t sign = 0) {
        text_field_ = field;
        text_sign_index_ = sign;
        if (field == TextField::Maker) text_buffer_ = level_.config.maker;
        else if (field == TextField::Email) text_buffer_ = level_.config.email;
        else if (sign < level_.config.sign_texts.size()) {
            text_buffer_ = field == TextField::SignFirst
                ? level_.config.sign_texts[sign].first
                : level_.config.sign_texts[sign].second;
        }
        SDL_StartTextInput(window_);
    }

    void CommitTextEdit() {
        if (text_field_ == TextField::None) return;
        if (text_field_ == TextField::ThemeName) {
            std::string name = text_buffer_;
            name.erase(std::remove_if(name.begin(), name.end(), [](unsigned char value) {
                return !(std::isalnum(value) || value == ' ' || value == '-' || value == '_');
            }), name.end());
            if (name.empty()) throw std::runtime_error("Theme name cannot be empty");
            const std::filesystem::path directory = AssetRoot() / "ggstuff" / name;
            if (std::filesystem::exists(directory)) {
                throw std::runtime_error("That GG theme already exists");
            }
            std::filesystem::create_directories(directory);
            std::ofstream info(directory / "info.txt", std::ios::binary);
            info << "/MAKER\nAnonymous\n/EMAIL\n\n/TEXDARK\n80\n/PARDARK\n96\n"
                    "/TEXATTR\n1\n/GRASSATTR\n1\n/PICATTR\n1\n/SIGNATTR\n2\n"
                    "/SIGNCOLOR\n1\n/SIGNTEXT\n10,22\n/FIXEDSIZE\n0,0\n"
                    "/WATERLEVEL\n0\n/WATERC\n50,50,200\n/BEACHSTYLE\nno\n";
            if (!info) throw std::runtime_error("Could not create theme info.txt");
            LoadThemes();
            BeginEdit();
            level_.config.gg_theme = name;
            dirty_ = true;
            text_field_ = TextField::None;
            SDL_StopTextInput(window_);
            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_INFORMATION, "GG theme created",
                "The theme folder and info.txt were created. Add s1.tga and t1.jpg, "
                "then run Check before previewing/exporting.", window_);
            UpdateTitle("theme scaffold created");
            return;
        }
        std::string *destination = nullptr;
        if (text_field_ == TextField::Maker) destination = &level_.config.maker;
        else if (text_field_ == TextField::Email) destination = &level_.config.email;
        else if (text_sign_index_ < level_.config.sign_texts.size()) {
            destination = text_field_ == TextField::SignFirst
                ? &level_.config.sign_texts[text_sign_index_].first
                : &level_.config.sign_texts[text_sign_index_].second;
        }
        if (destination != nullptr && *destination != text_buffer_) {
            BeginEdit();
            *destination = text_buffer_;
            dirty_ = true;
        }
        text_field_ = TextField::None;
        SDL_StopTextInput(window_);
        UpdateTitle(dirty_ ? "modified" : "ready");
    }

    void CancelTextEdit() {
        text_field_ = TextField::None;
        text_buffer_.clear();
        SDL_StopTextInput(window_);
        UpdateTitle(dirty_ ? "modified" : "ready");
    }

    void StartThemeCreation() {
        text_buffer_.clear();
        text_field_ = TextField::ThemeName;
        SDL_StartTextInput(window_);
    }

    static std::vector<std::uint8_t> ReadAssetBytes(
        const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) throw std::runtime_error("Could not open " + path.string());
        stream.seekg(0, std::ios::end);
        const std::streamoff length = stream.tellg();
        stream.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
        if (!bytes.empty()) stream.read(reinterpret_cast<char *>(bytes.data()), length);
        if (!stream) throw std::runtime_error("Could not read " + path.string());
        return bytes;
    }

    void ChooseParallax() {
        static constexpr SDL_DialogFileFilter filters[] = {
            {"JPEG image", "jpg;jpeg"},
        };
        const auto selected = WaitForFileDialog(
            window_, false, filters, 1, project_.project_path.parent_path().string());
        if (!selected) return;
        int width = 0;
        int height = 0;
        int channels = 0;
        if (!stbi_info(selected->string().c_str(), &width, &height, &channels)) {
            throw std::runtime_error("Selected parallax is not a readable image");
        }
        const std::vector<std::uint8_t> payload = ReadAssetBytes(*selected);
        if (payload.size() < 2U || payload[0] != 0xFFU || payload[1] != 0xD8U) {
            throw std::runtime_error("Selected parallax must be a JPEG file");
        }
        BeginEdit();
        project_.parallax_path = std::filesystem::absolute(*selected).lexically_normal();
        available_parallax_payload_ = payload;
        level_.parallax_payload = available_parallax_payload_;
        level_.config.parallax = true;
        dirty_ = true;
        UpdateTitle("parallax selected");
    }

    void SelectRule(float x, float y) {
        if (x < 8.0F || x >= kSidebarWidth - 8.0F || y < kContentTop) return;
        const int index = static_cast<int>(
            (y - kContentTop + sidebar_scroll_) / 40.0F);
        if (index >= 0 && index < static_cast<int>(RuleNames().size())) {
            selected_rule_ = static_cast<std::size_t>(index);
            UpdateTitle(dirty_ ? "modified" : "ready");
        }
    }

    void ChangeRule(int delta) {
        tou::level::LevelConfig &config = level_.config;
        if (selected_rule_ == 0) {
            BeginTextEdit(TextField::Maker);
            return;
        } else if (selected_rule_ == 1) {
            BeginTextEdit(TextField::Email);
            return;
        } else if (selected_rule_ == 2) {
            if (config.mode == tou::level::LevelMode::GroundGenerated) return;
            if (delta > 0) {
                ChooseParallax();
            } else if (!project_.parallax_path.empty()) {
                BeginEdit();
                project_.parallax_path.clear();
                available_parallax_payload_.clear();
                level_.parallax_payload.clear();
                config.parallax = false;
                dirty_ = true;
            }
            return;
        } else if (selected_rule_ == 3) {
            if (!config.parallax && available_parallax_payload_.empty()) return;
            BeginEdit();
            config.parallax = !config.parallax;
            level_.parallax_payload = config.parallax
                ? available_parallax_payload_ : std::vector<std::uint8_t>{};
        } else if (selected_rule_ == 9) {
            BeginEdit();
            config.disable_running_water = !config.disable_running_water;
        } else {
            std::uint8_t *value = nullptr;
            switch (selected_rule_) {
            case 4: value = &config.civilians; break;
            case 5: value = &config.bombing; break;
            case 6: value = &config.water.r; break;
            case 7: value = &config.water.g; break;
            case 8: value = &config.water.b; break;
            case 10: value = &config.gravity_tenths; break;
            case 11: value = &config.resistance_tenths; break;
            case 12: value = &config.collision_damage_tenths; break;
            case 13: value = &config.bouncing_tenths; break;
            case 14: value = &config.ambient; break;
            case 15: value = &config.parallax_aftertouch; break;
            default: break;
            }
            if (value == nullptr) return;
            const int limit = selected_rule_ == 4 || selected_rule_ == 5 ? 100 : 255;
            const int next = std::clamp(static_cast<int>(*value) + delta, 0, limit);
            if (next == *value) return;
            BeginEdit();
            *value = static_cast<std::uint8_t>(next);
        }
        dirty_ = !IsSavedState();
        UpdateTitle(dirty_ ? "modified" : "ready");
    }

    std::size_t GroundGenerationRuleCount() const {
        return 7U + level_.config.sign_texts.size() * 2U;
    }

    std::string GroundGenerationName(std::size_t index) const {
        static constexpr std::array<const char *, 7> names{
            "Theme", "Generator shape", "Repair density", "Stuff density",
            "Sign density", "Random seed", "Sign records",
        };
        if (index < names.size()) return names[index];
        const std::size_t relative = index - names.size();
        return "Sign " + std::to_string(relative / 2U + 1U) +
            (relative % 2U == 0U ? " line 1" : " line 2");
    }

    std::string GroundGenerationValue(std::size_t index) const {
        const tou::level::LevelConfig &config = level_.config;
        switch (index) {
        case 0: return config.gg_theme;
        case 1: return config.gg_shape ? "Yes" : "No";
        case 2: return std::to_string(config.repair_density);
        case 3: return std::to_string(config.stuff_density);
        case 4: return std::to_string(config.sign_density);
        case 5: return std::to_string(config.random_seed);
        case 6: return std::to_string(config.sign_texts.size());
        default: break;
        }
        const std::size_t relative = index - 7U;
        const std::size_t sign = relative / 2U;
        if (sign >= config.sign_texts.size()) return "?";
        const std::string &value = relative % 2U == 0U
            ? config.sign_texts[sign].first : config.sign_texts[sign].second;
        return value.empty() ? "(empty)" : value;
    }

    void SelectGroundGenerationRule(float x, float y) {
        if (x < 8.0F || x >= kSidebarWidth - 8.0F || y < kContentTop) return;
        if (y - kContentTop + sidebar_scroll_ < 46.0F) return;
        const int index = static_cast<int>(
            (y - kContentTop - 46.0F + sidebar_scroll_) / 48.0F);
        if (index >= 0 &&
            index < static_cast<int>(GroundGenerationRuleCount())) {
            selected_gg_rule_ = static_cast<std::size_t>(index);
            UpdateTitle(dirty_ ? "modified" : "ready");
        }
    }

    void ChangeGroundGenerationRule(int delta) {
        tou::level::LevelConfig &config = level_.config;
        if (selected_gg_rule_ == 0) {
            if (themes_.empty()) return;
            const auto found = std::find(themes_.begin(), themes_.end(), config.gg_theme);
            std::ptrdiff_t index = found == themes_.end()
                ? 0 : std::distance(themes_.begin(), found);
            index = (index + delta) % static_cast<std::ptrdiff_t>(themes_.size());
            if (index < 0) index += static_cast<std::ptrdiff_t>(themes_.size());
            if (config.gg_theme == themes_[static_cast<std::size_t>(index)]) return;
            BeginEdit();
            config.gg_theme = themes_[static_cast<std::size_t>(index)];
        } else if (selected_gg_rule_ == 1) {
            BeginEdit();
            config.gg_shape = !config.gg_shape;
        } else if (selected_gg_rule_ >= 2 && selected_gg_rule_ <= 4) {
            std::uint8_t *value = selected_gg_rule_ == 2 ? &config.repair_density
                : selected_gg_rule_ == 3 ? &config.stuff_density
                                         : &config.sign_density;
            const int next = std::clamp(static_cast<int>(*value) + delta, 0, 100);
            if (next == *value) return;
            BeginEdit();
            *value = static_cast<std::uint8_t>(next);
        } else if (selected_gg_rule_ == 5) {
            const std::int64_t next = std::clamp<std::int64_t>(
                static_cast<std::int64_t>(config.random_seed) + delta,
                0, std::numeric_limits<std::uint32_t>::max());
            if (next == config.random_seed) return;
            BeginEdit();
            config.random_seed = static_cast<std::uint32_t>(next);
        } else if (selected_gg_rule_ == 6) {
            const int count = std::clamp(
                static_cast<int>(config.sign_texts.size()) + delta, 0, 16);
            if (count == static_cast<int>(config.sign_texts.size())) return;
            BeginEdit();
            config.sign_texts.resize(static_cast<std::size_t>(count));
            if (selected_gg_rule_ >= GroundGenerationRuleCount()) {
                selected_gg_rule_ = GroundGenerationRuleCount() - 1U;
            }
        } else if (selected_gg_rule_ >= 7) {
            const std::size_t relative = selected_gg_rule_ - 7U;
            BeginTextEdit(relative % 2U == 0U
                              ? TextField::SignFirst : TextField::SignSecond,
                          relative / 2U);
            return;
        }
        dirty_ = !IsSavedState();
        UpdateTitle(dirty_ ? "modified" : "ready");
    }

    std::filesystem::path ExportPath() const {
        std::string filename = project_.project_path.filename().string();
        const std::string suffix = ".toulevel.json";
        if (filename.size() >= suffix.size() &&
            filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) == 0) {
            filename.erase(filename.size() - suffix.size());
        }
        return project_.project_path.parent_path() / (filename + ".lev");
    }

    std::filesystem::path GameExecutablePath() const {
        const std::filesystem::path root = AssetRoot();
        const char *base_text = SDL_GetBasePath();
        const std::filesystem::path base = base_text == nullptr
            ? std::filesystem::path{} : std::filesystem::path(base_text);
#ifdef _WIN32
        const std::array<std::filesystem::path, 3> candidates{{
            root / "TOU.exe", root / "Tunnels of Underworld.exe",
            base / "../TOU.exe",
        }};
#elif defined(__APPLE__)
        const std::array<std::filesystem::path, 4> candidates{{
            root / "TOU", root / "../MacOS/TOU",
            root / "TOU.app/Contents/MacOS/TOU",
            base / "../TOU.app/Contents/MacOS/TOU",
        }};
#else
        const std::array<std::filesystem::path, 3> candidates{{
            root / "TOU", root / "tou", base / "../TOU",
        }};
#endif
        for (const auto &candidate : candidates) {
            if (std::filesystem::is_regular_file(candidate)) return candidate;
        }
        return {};
    }

    void GenerateGroundPreview() {
        if (level_.config.mode != tou::level::LevelMode::GroundGenerated) return;
        const auto validation = ValidateCurrent();
        const auto error = std::find_if(validation.begin(), validation.end(),
            [](const auto &message) {
                return message.severity == tou::level::ValidationMessage::Severity::Error;
            });
        if (error != validation.end()) {
            throw std::runtime_error("Cannot preview: " + error->text);
        }
        const std::filesystem::path executable = GameExecutablePath();
        if (executable.empty()) {
            throw std::runtime_error("Could not locate the TOU runtime for GG preview");
        }
        const std::filesystem::path level_path =
            project_.project_path.parent_path() / ".tou-gg-preview.lev";
        const std::filesystem::path bitmap_path =
            project_.project_path.parent_path() / ".tou-gg-preview.bmp";
        tou::level::WriteLevel(level_, level_path);
        std::filesystem::remove(bitmap_path);
        const std::string executable_text = executable.string();
        const std::string level_text = level_path.string();
        const std::string bitmap_text = bitmap_path.string();
        const char *arguments[] = {
            executable_text.c_str(), "--render-level-preview", level_text.c_str(),
            "--preview-output", bitmap_text.c_str(), nullptr,
        };
        SDL_Process *process = SDL_CreateProcess(arguments, false);
        if (process == nullptr) {
            std::filesystem::remove(level_path);
            throw std::runtime_error("Could not start TOU runtime: " +
                                     std::string(SDL_GetError()));
        }
        int result = -1;
        const bool waited = SDL_WaitProcess(process, true, &result);
        SDL_DestroyProcess(process);
        std::filesystem::remove(level_path);
        if (!waited || result != 0 || !std::filesystem::is_regular_file(bitmap_path)) {
            std::filesystem::remove(bitmap_path);
            throw std::runtime_error("The TOU runtime could not generate the GG preview");
        }
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char *pixels = stbi_load(bitmap_path.string().c_str(),
                                          &width, &height, &channels, 4);
        std::filesystem::remove(bitmap_path);
        if (pixels == nullptr) {
            throw std::runtime_error("Could not decode the generated GG preview");
        }
        SDL_Texture *texture = SDL_CreateTexture(
            renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
            width, height);
        if (texture == nullptr ||
            !SDL_UpdateTexture(texture, nullptr, pixels, width * 4)) {
            stbi_image_free(pixels);
            if (texture != nullptr) SDL_DestroyTexture(texture);
            throw std::runtime_error(SDL_GetError());
        }
        stbi_image_free(pixels);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        if (gg_preview_texture_ != nullptr) SDL_DestroyTexture(gg_preview_texture_);
        gg_preview_texture_ = texture;
        gg_preview_stale_ = false;
        UpdateTitle("GG preview generated by runtime");
    }

    std::vector<tou::level::ValidationMessage> ValidateCurrent() const {
        tou::level::Project project = project_;
        project.config = level_.config;
        std::vector<tou::level::ValidationMessage> messages =
            tou::level::ValidateProject(project);
        std::vector<tou::level::ValidationMessage> level_messages =
            tou::level::ValidateLevel(level_);
        messages.insert(messages.end(), level_messages.begin(), level_messages.end());
        const auto add = [&messages](tou::level::ValidationMessage::Severity severity,
                                     const std::string &text) {
            messages.push_back({severity, text});
        };
        if (level_.config.mode == tou::level::LevelMode::Normal &&
            std::find(level_.attributes.begin(), level_.attributes.end(), 33U) !=
                level_.attributes.end()) {
            add(tou::level::ValidationMessage::Severity::Warning,
                "Normal level contains GG sign-marker pixels (palette entry 33)");
        }
        if (level_.config.mode == tou::level::LevelMode::GroundGenerated) {
            const std::filesystem::path theme =
                AssetRoot() / "ggstuff" / level_.config.gg_theme;
            if (!std::filesystem::is_regular_file(theme / "info.txt")) {
                add(tou::level::ValidationMessage::Severity::Error,
                    "GG theme is missing info.txt: " + theme.string());
            }
            if (!std::filesystem::is_regular_file(theme / "s1.tga")) {
                add(tou::level::ValidationMessage::Severity::Error,
                    "GG theme is missing s1.tga: " + theme.string());
            }
            if (!std::filesystem::is_regular_file(theme / "t1.jpg")) {
                add(tou::level::ValidationMessage::Severity::Error,
                    "GG theme is missing t1.jpg: " + theme.string());
            }
            const bool has_sign_markers =
                std::find(level_.attributes.begin(), level_.attributes.end(), 33U) !=
                level_.attributes.end();
            if (has_sign_markers && level_.config.sign_texts.empty()) {
                add(tou::level::ValidationMessage::Severity::Warning,
                    "Constraint map has sign markers but no sign records");
            }
        }
        for (const auto &placement : level_.placements) {
            float half_width = 0.0F;
            float half_height = 0.0F;
            if (placement.type == tou::level::PlacementType::Turret) {
                const auto &preview = turret_previews_[
                    std::min<std::size_t>(placement.parameters[0], 6U)][
                    std::min<std::size_t>(placement.parameters[2], 3U)][0];
                half_width = preview.width / 2.0F;
                half_height = preview.height / 2.0F;
            } else if (placement.type == tou::level::PlacementType::Gate) {
                const auto &preview = gate_previews_[placement.parameters[4] == 0 ? 0U : 1U];
                const bool quarter_turn = (placement.parameters[3] & 1U) != 0;
                half_width = (quarter_turn ? preview.height : preview.width) / 2.0F;
                half_height = (quarter_turn ? preview.width : preview.height) / 2.0F;
            }
            if ((half_width > 0.0F || half_height > 0.0F) &&
                (placement.x < half_width || placement.y < half_height ||
                 placement.x + half_width >= level_.width ||
                 placement.y + half_height >= level_.height)) {
                add(tou::level::ValidationMessage::Severity::Warning,
                    "A turret/gate sprite at (" + std::to_string(placement.x) +
                    ", " + std::to_string(placement.y) + ") crosses the map edge");
            }
        }
        return messages;
    }

    bool ShowValidation(bool for_export) {
        const std::vector<tou::level::ValidationMessage> messages = ValidateCurrent();
        const bool errors = std::any_of(messages.begin(), messages.end(),
            [](const auto &message) {
                return message.severity == tou::level::ValidationMessage::Severity::Error;
            });
        std::string report = messages.empty() ? "No problems found." : "";
        for (const auto &message : messages) {
            report += message.severity == tou::level::ValidationMessage::Severity::Error
                ? "ERROR: " : "WARNING: ";
            report += message.text + "\n";
        }
        if (errors || !for_export || messages.empty()) {
            SDL_ShowSimpleMessageBox(errors ? SDL_MESSAGEBOX_ERROR
                                            : SDL_MESSAGEBOX_INFORMATION,
                                     "Level validation", report.c_str(), window_);
            return !errors;
        }
        static constexpr SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Export anyway"},
        };
        report += "\nExport despite these warnings?";
        return ShowChoice(window_, "Level validation", report.c_str(), buttons, 2) == 1;
    }

    void Save() {
        project_.config = level_.config;
        tou::level::SaveAttributeTga(level_, project_.attribute_path);
        tou::level::SaveProject(project_, project_.project_path);
        saved_ = Snapshot();
        dirty_ = false;
        UpdateTitle("saved");
    }

    void Export() {
        if (!ShowValidation(true)) return;
        tou::level::WriteLevel(level_, ExportPath());
        UpdateTitle("exported " + ExportPath().filename().string());
    }

    void RequestQuit() {
        if (!dirty_) {
            running_ = false;
            return;
        }
        const SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
            {0, 1, "Discard"},
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 2, "Save"},
        };
        const SDL_MessageBoxData data{
            SDL_MESSAGEBOX_WARNING,
            window_,
            "Unsaved level changes",
            "Save your changes before closing?",
            3,
            buttons,
            nullptr,
        };
        int chosen = 0;
        if (!SDL_ShowMessageBox(&data, &chosen)) {
            throw std::runtime_error(SDL_GetError());
        }
        if (chosen == 1) running_ = false;
        if (chosen == 2) {
            Save();
            running_ = false;
        }
    }

    void UpdateTitle(const std::string &status) {
        std::string selection;
        if (mode_ == EditMode::Terrain) {
            selection = tou::level::Palette()[selected_palette_].name;
        } else if (mode_ == EditMode::Placements) {
            selection = PlacementTemplates()[selected_template_].name;
        } else if (mode_ == EditMode::LevelRules) {
            selection = RuleNames()[selected_rule_];
        } else {
            selection = GroundGenerationName(selected_gg_rule_);
        }
        const std::string title = "TOU Level Editor - " +
            project_.project_path.filename().string() + " - " + selection +
            " - brush " + std::to_string(brush_size_) + " px - " + status;
        SDL_SetWindowTitle(window_, title.c_str());
    }

    void SetMode(EditMode mode) {
        if (mode == EditMode::GroundGeneration &&
            level_.config.mode != tou::level::LevelMode::GroundGenerated) {
            return;
        }
        mode_ = mode;
        sidebar_scroll_ = 0.0F;
        UpdateTitle(dirty_ ? "modified" : "ready");
    }

    float SidebarMaxScroll() const {
        int width = 0;
        int height = 0;
        SDL_GetRenderOutputSize(renderer_, &width, &height);
        static_cast<void>(width);
        float content = 0.0F;
        if (mode_ == EditMode::Terrain) {
            content = 34.0F * kPaletteRowHeight + 20.0F;
        } else if (mode_ == EditMode::Placements) {
            content = static_cast<float>(PlacementTemplates().size()) * 58.0F +
                      14.0F + 66.0F + 5.0F * 24.0F + 30.0F;
        } else if (mode_ == EditMode::LevelRules) {
            content = static_cast<float>(RuleNames().size()) * 40.0F + 30.0F;
        } else {
            content = 46.0F + static_cast<float>(GroundGenerationRuleCount()) *
                      48.0F + 36.0F;
        }
        return std::max(0.0F, content -
            (static_cast<float>(height) - kContentTop - kStatusBarHeight));
    }

    static bool Inside(float x, float y, const SDL_FRect &rect) {
        return x >= rect.x && y >= rect.y &&
               x < rect.x + rect.w && y < rect.y + rect.h;
    }

    bool InStatusBar(float y) const {
        int width = 0;
        int height = 0;
        SDL_GetRenderOutputSize(renderer_, &width, &height);
        static_cast<void>(width);
        return y >= static_cast<float>(height) - kStatusBarHeight;
    }

    static std::string Truncated(std::string value, std::size_t characters) {
        if (value.size() <= characters) return value;
        if (characters <= 3U) return value.substr(0, characters);
        value.resize(characters - 3U);
        return value + "...";
    }

    void AdjustCurrentValue(int delta) {
        if (mode_ == EditMode::Terrain) {
            overlay_alpha_ = std::clamp(overlay_alpha_ + delta * 16, 16, 255);
            UpdateAttributeTexture();
        } else if (mode_ == EditMode::Placements) {
            ChangeSelectedParameter(delta);
        } else if (mode_ == EditMode::LevelRules) {
            ChangeRule(delta);
        } else {
            ChangeGroundGenerationRule(delta);
        }
    }

    void AdjustBrush(int delta) {
        brush_size_ = std::clamp(brush_size_ + delta * 2, 1, 63);
        UpdateTitle(dirty_ ? "modified" : "ready");
    }

    const char *MinusLabel() const {
        if (mode_ == EditMode::Terrain) return "Opacity -";
        if (mode_ == EditMode::LevelRules && selected_rule_ <= 1U) return "Edit";
        if (mode_ == EditMode::LevelRules && selected_rule_ == 2U) return "Clear";
        if (mode_ == EditMode::GroundGeneration && selected_gg_rule_ >= 7U) return "Edit";
        return "Value -";
    }

    const char *PlusLabel() const {
        if (mode_ == EditMode::Terrain) return "Opacity +";
        if (mode_ == EditMode::LevelRules && selected_rule_ <= 1U) return "Edit";
        if (mode_ == EditMode::LevelRules && selected_rule_ == 2U) return "Choose";
        if (mode_ == EditMode::GroundGeneration && selected_gg_rule_ >= 7U) return "Edit";
        return "Value +";
    }

    const char *ActionLabel() const {
        if (mode_ == EditMode::GroundGeneration) return "Preview";
        if (mode_ == EditMode::Placements && selected_placement_) {
            const auto type = level_.placements[*selected_placement_].type;
            if (type == tou::level::PlacementType::Gate ||
                type == tou::level::PlacementType::Teleport) {
                return type == tou::level::PlacementType::Gate ? "Linked" : "Pair";
            }
        }
        return "Check";
    }

    void RunToolbarAction() {
        if (mode_ == EditMode::GroundGeneration) GenerateGroundPreview();
        else if (std::strcmp(ActionLabel(), "Pair") == 0 ||
                 std::strcmp(ActionLabel(), "Linked") == 0) PairSelectedPlacement();
        else static_cast<void>(ShowValidation(false));
    }

    bool HandleToolbarClick(float x, float y) {
        if (y >= kToolbarHeight) return false;
        const std::array<SDL_FRect, 4> tabs{{
            {10.0F, 10.0F, 80.0F, 32.0F},
            {96.0F, 10.0F, 90.0F, 32.0F},
            {192.0F, 10.0F, 80.0F, 32.0F},
            {278.0F, 10.0F, 66.0F, 32.0F},
        }};
        if (Inside(x, y, tabs[0])) SetMode(EditMode::Terrain);
        else if (Inside(x, y, tabs[1])) SetMode(EditMode::Placements);
        else if (Inside(x, y, tabs[2])) SetMode(EditMode::LevelRules);
        else if (Inside(x, y, tabs[3])) SetMode(EditMode::GroundGeneration);
        else if (Inside(x, y, {360.0F, 10.0F, 64.0F, 32.0F})) Undo();
        else if (Inside(x, y, {430.0F, 10.0F, 64.0F, 32.0F})) Redo();
        else if (Inside(x, y, {500.0F, 10.0F, 64.0F, 32.0F})) Save();
        else if (Inside(x, y, {570.0F, 10.0F, 72.0F, 32.0F})) Export();
        else if (Inside(x, y, {648.0F, 10.0F, 54.0F, 32.0F})) FitCanvas();
        else if (Inside(x, y, {708.0F, 10.0F, 76.0F, 32.0F})) {
            show_visual_ = !show_visual_;
        } else if (Inside(x, y, {790.0F, 10.0F, 92.0F, 32.0F})) {
            show_attributes_ = !show_attributes_;
        } else if (Inside(x, y, {890.0F, 10.0F, 80.0F, 32.0F})) AdjustCurrentValue(-1);
        else if (Inside(x, y, {974.0F, 10.0F, 80.0F, 32.0F})) AdjustCurrentValue(1);
        else if (Inside(x, y, {1058.0F, 10.0F, 66.0F, 32.0F})) AdjustBrush(-1);
        else if (Inside(x, y, {1128.0F, 10.0F, 66.0F, 32.0F})) AdjustBrush(1);
        else if (Inside(x, y, {1198.0F, 10.0F, 74.0F, 32.0F})) RunToolbarAction();
        return true;
    }

    void HandleEvent(const SDL_Event &event) {
        if (text_field_ != TextField::None) {
            if (event.type == SDL_EVENT_QUIT ||
                event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                CancelTextEdit();
                RequestQuit();
            } else if (event.type == SDL_EVENT_TEXT_INPUT) {
                const std::size_t limit =
                    text_field_ == TextField::SignFirst ||
                    text_field_ == TextField::SignSecond ? 15U : 127U;
                const std::string proposed = text_buffer_ + event.text.text;
                const std::size_t characters = static_cast<std::size_t>(
                    std::count_if(proposed.begin(), proposed.end(), [](unsigned char value) {
                        return (value & 0xC0U) != 0x80U;
                    }));
                if (characters <= limit) {
                    text_buffer_ = proposed;
                }
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_RETURN ||
                    event.key.scancode == SDL_SCANCODE_KP_ENTER) {
                    CommitTextEdit();
                } else if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    CancelTextEdit();
                } else if (event.key.scancode == SDL_SCANCODE_BACKSPACE &&
                           !text_buffer_.empty()) {
                    do {
                        text_buffer_.pop_back();
                    } while (!text_buffer_.empty() &&
                             (static_cast<unsigned char>(text_buffer_.back()) & 0xC0U) == 0x80U);
                }
            }
            return;
        }
        switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            RequestQuit();
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (InStatusBar(event.button.y)) break;
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (HandleToolbarClick(event.button.x, event.button.y)) {
                    break;
                }
                if (event.button.x < kSidebarWidth) {
                    if (mode_ == EditMode::Terrain) {
                        SelectPalette(event.button.x, event.button.y);
                    } else if (mode_ == EditMode::Placements) {
                        SelectPlacementTemplate(event.button.x, event.button.y);
                    } else if (mode_ == EditMode::LevelRules) {
                        SelectRule(event.button.x, event.button.y);
                    } else {
                        SelectGroundGenerationRule(event.button.x, event.button.y);
                    }
                } else if (mode_ == EditMode::Terrain) {
                    int map_x = 0;
                    int map_y = 0;
                    if (MapPoint(event.button.x, event.button.y, map_x, map_y)) {
                        BeginEdit();
                        painting_ = true;
                        Paint(event.button.x, event.button.y);
                    }
                } else if (mode_ == EditMode::Placements) {
                    AddOrSelectPlacement(event.button.x, event.button.y);
                }
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                if (mode_ == EditMode::Terrain) {
                    Pick(event.button.x, event.button.y);
                } else if (mode_ == EditMode::Placements) {
                    selected_placement_ = HitPlacement(event.button.x, event.button.y);
                    UpdateTitle(dirty_ ? "modified" : "ready");
                }
            } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                panning_ = true;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                painting_ = false;
                dragging_placement_ = false;
                drag_edit_started_ = false;
            }
            if (event.button.button == SDL_BUTTON_MIDDLE) {
                panning_ = false;
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (panning_) {
                pan_x_ += event.motion.xrel;
                pan_y_ += event.motion.yrel;
            } else if (painting_ && mode_ == EditMode::Terrain) {
                Paint(event.motion.x, event.motion.y);
            } else if (dragging_placement_ && mode_ == EditMode::Placements) {
                MoveSelectedPlacement(event.motion.x, event.motion.y);
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL: {
            float mouse_x = 0.0F;
            float mouse_y = 0.0F;
            SDL_GetMouseState(&mouse_x, &mouse_y);
            if (InStatusBar(mouse_y)) break;
            if (mouse_x < kSidebarWidth && mouse_y >= kToolbarHeight) {
                sidebar_scroll_ = std::clamp(
                    sidebar_scroll_ - event.wheel.y * 56.0F,
                    0.0F, SidebarMaxScroll());
                break;
            }
            const float old_zoom = zoom_;
            zoom_ = std::clamp(zoom_ * (event.wheel.y > 0 ? 1.2F : 1.0F / 1.2F),
                               0.05F, 32.0F);
            pan_x_ = mouse_x - kSidebarWidth -
                (mouse_x - kSidebarWidth - pan_x_) * (zoom_ / old_zoom);
            pan_y_ = mouse_y - kToolbarHeight -
                (mouse_y - kToolbarHeight - pan_y_) * (zoom_ / old_zoom);
            break;
        }
        case SDL_EVENT_KEY_DOWN: {
            const bool control = (event.key.mod & SDL_KMOD_CTRL) != 0;
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                if (pairing_source_) {
                    pairing_source_.reset();
                    UpdateTitle("teleport pairing cancelled");
                } else {
                    RequestQuit();
                }
            }
            if (event.key.scancode == SDL_SCANCODE_TAB) {
                if (mode_ == EditMode::Terrain) mode_ = EditMode::Placements;
                else if (mode_ == EditMode::Placements) mode_ = EditMode::LevelRules;
                else if (mode_ == EditMode::LevelRules &&
                         level_.config.mode == tou::level::LevelMode::GroundGenerated) {
                    mode_ = EditMode::GroundGeneration;
                } else mode_ = EditMode::Terrain;
                sidebar_scroll_ = 0.0F;
                UpdateTitle(dirty_ ? "modified" : "ready");
            }
            if (event.key.scancode == SDL_SCANCODE_F) FitCanvas();
            if (event.key.scancode == SDL_SCANCODE_V) show_visual_ = !show_visual_;
            if (event.key.scancode == SDL_SCANCODE_A) show_attributes_ = !show_attributes_;
            if (event.key.scancode == SDL_SCANCODE_LEFTBRACKET) {
                brush_size_ = std::max(1, brush_size_ - 2);
                UpdateTitle(dirty_ ? "modified" : "ready");
            }
            if (event.key.scancode == SDL_SCANCODE_RIGHTBRACKET) {
                brush_size_ = std::min(63, brush_size_ + 2);
                UpdateTitle(dirty_ ? "modified" : "ready");
            }
            if (event.key.scancode == SDL_SCANCODE_MINUS) {
                if (mode_ == EditMode::Terrain) {
                    overlay_alpha_ = std::max(16, overlay_alpha_ - 16);
                    UpdateAttributeTexture();
                } else if (mode_ == EditMode::Placements) {
                    ChangeSelectedParameter(-1);
                } else if (mode_ == EditMode::LevelRules) {
                    ChangeRule(-1);
                } else {
                    ChangeGroundGenerationRule(-1);
                }
            }
            if (event.key.scancode == SDL_SCANCODE_EQUALS) {
                if (mode_ == EditMode::Terrain) {
                    overlay_alpha_ = std::min(255, overlay_alpha_ + 16);
                    UpdateAttributeTexture();
                } else if (mode_ == EditMode::Placements) {
                    ChangeSelectedParameter(1);
                } else if (mode_ == EditMode::LevelRules) {
                    ChangeRule(1);
                } else {
                    ChangeGroundGenerationRule(1);
                }
            }
            if (control && event.key.scancode == SDL_SCANCODE_S) Save();
            if (control && event.key.scancode == SDL_SCANCODE_E) Export();
            if (control && event.key.scancode == SDL_SCANCODE_Z) Undo();
            if (control && event.key.scancode == SDL_SCANCODE_Y) Redo();
            if (control && event.key.scancode == SDL_SCANCODE_N &&
                level_.config.mode == tou::level::LevelMode::GroundGenerated) {
                StartThemeCreation();
            }
            if (event.key.scancode == SDL_SCANCODE_P &&
                level_.config.mode == tou::level::LevelMode::GroundGenerated) {
                GenerateGroundPreview();
            }
            if (event.key.scancode == SDL_SCANCODE_RETURN) {
                if (mode_ == EditMode::LevelRules && selected_rule_ <= 1U) {
                    ChangeRule(1);
                } else if (mode_ == EditMode::GroundGeneration &&
                           selected_gg_rule_ >= 7U) {
                    ChangeGroundGenerationRule(1);
                }
            }
            if (mode_ == EditMode::Placements) {
                if (event.key.scancode >= SDL_SCANCODE_1 &&
                    event.key.scancode <= SDL_SCANCODE_5) {
                    selected_parameter_ = static_cast<std::size_t>(
                        event.key.scancode - SDL_SCANCODE_1);
                }
                if (event.key.scancode == SDL_SCANCODE_UP || event.key.key == SDLK_UP) {
                    ChangeSelectedParameter(1);
                }
                if (event.key.scancode == SDL_SCANCODE_DOWN || event.key.key == SDLK_DOWN) {
                    ChangeSelectedParameter(-1);
                }
                if (event.key.scancode == SDL_SCANCODE_DELETE ||
                    event.key.scancode == SDL_SCANCODE_BACKSPACE) {
                    DeleteSelectedPlacement();
                }
            } else if (mode_ == EditMode::LevelRules) {
                if (event.key.scancode == SDL_SCANCODE_UP || event.key.key == SDLK_UP) {
                    ChangeRule(1);
                }
                if (event.key.scancode == SDL_SCANCODE_DOWN || event.key.key == SDLK_DOWN) {
                    ChangeRule(-1);
                }
            } else if (mode_ == EditMode::GroundGeneration) {
                if (event.key.scancode == SDL_SCANCODE_UP || event.key.key == SDLK_UP) {
                    ChangeGroundGenerationRule(1);
                }
                if (event.key.scancode == SDL_SCANCODE_DOWN || event.key.key == SDLK_DOWN) {
                    ChangeGroundGenerationRule(-1);
                }
            }
            break;
        }
        default:
            break;
        }
    }

    void DrawPlacementPreview(std::size_t index, float y) {
        SDL_FRect box{14.0F, y + 5.0F, 42.0F, 40.0F};
        SDL_SetRenderDrawColor(renderer_, 16, 18, 24, 255);
        SDL_RenderFillRect(renderer_, &box);
        SDL_SetRenderDrawColor(renderer_, 74, 82, 98, 255);
        SDL_RenderRect(renderer_, &box);
        if (index == 0) {
            std::size_t style = PlacementTemplates()[0].parameters[0];
            std::size_t team = PlacementTemplates()[0].parameters[2];
            std::size_t direction = PlacementTemplates()[0].parameters[3];
            if (selected_placement_ &&
                level_.placements[*selected_placement_].type ==
                    tou::level::PlacementType::Turret) {
                style = level_.placements[*selected_placement_].parameters[0];
                team = level_.placements[*selected_placement_].parameters[2];
                direction = level_.placements[*selected_placement_].parameters[3];
            }
            style = std::min<std::size_t>(style, kTurretSpriteBases.size() - 1U);
            team = std::min<std::size_t>(team, 3U);
            const std::size_t frame = direction == 0
                ? 0U : std::min<std::size_t>(direction - 1U, 31U);
            const SpritePreview &preview = turret_previews_[style][team][frame];
            if (preview.texture != nullptr) {
                const float scale = std::min(36.0F / preview.width,
                                             34.0F / preview.height);
                SDL_FRect destination{
                    box.x + (box.w - preview.width * scale) / 2.0F,
                    box.y + (box.h - preview.height * scale) / 2.0F,
                    preview.width * scale,
                    preview.height * scale,
                };
                SDL_RenderTexture(renderer_, preview.texture, nullptr, &destination);
            }
        } else if (index == 1) {
            std::size_t graphic = 0;
            std::size_t facing = 0;
            if (selected_placement_ &&
                level_.placements[*selected_placement_].type ==
                    tou::level::PlacementType::Gate) {
                graphic = level_.placements[*selected_placement_].parameters[4] == 0 ? 0U : 1U;
                facing = level_.placements[*selected_placement_].parameters[3] & 3U;
            }
            const SpritePreview &preview = gate_previews_[graphic];
            if (preview.texture != nullptr) {
                const bool quarter_turn = (facing & 1U) != 0;
                const float rotated_width = quarter_turn ? preview.height : preview.width;
                const float rotated_height = quarter_turn ? preview.width : preview.height;
                const float scale = std::min(36.0F / rotated_width,
                                             34.0F / rotated_height);
                SDL_FRect destination{
                    box.x + (box.w - preview.width * scale) / 2.0F,
                    box.y + (box.h - preview.height * scale) / 2.0F,
                    preview.width * scale,
                    preview.height * scale,
                };
                SDL_RenderTextureRotated(renderer_, preview.texture, nullptr,
                                         &destination, facing * 90.0, nullptr,
                                         SDL_FLIP_NONE);
            }
        } else {
            SDL_SetRenderDrawColor(renderer_, 220, 80, 190, 255);
            SDL_FRect marker{box.x + 17.0F, box.y + 16.0F, 8.0F, 8.0F};
            SDL_RenderFillRect(renderer_, &marker);
        }
    }

    void DrawButton(const SDL_FRect &rect, const char *label,
                    bool active, bool enabled = true) {
        if (active) SDL_SetRenderDrawColor(renderer_, 54, 93, 112, 255);
        else SDL_SetRenderDrawColor(renderer_, 38, 42, 52, 255);
        SDL_RenderFillRect(renderer_, &rect);
        SDL_SetRenderDrawColor(renderer_, enabled ? 100 : 55,
                               enabled ? 122 : 62,
                               enabled ? 138 : 70, 255);
        SDL_RenderRect(renderer_, &rect);
        SDL_SetRenderDrawColor(renderer_, enabled ? 225 : 100,
                               enabled ? 232 : 108,
                               enabled ? 240 : 116, 255);
        const float text_width = static_cast<float>(std::strlen(label)) * 8.0F;
        SDL_RenderDebugText(renderer_,
                            rect.x + std::max(5.0F, (rect.w - text_width) / 2.0F),
                            rect.y + 12.0F, label);
    }

    void DrawToolbar() {
        int width = 0;
        int height = 0;
        SDL_GetRenderOutputSize(renderer_, &width, &height);
        static_cast<void>(height);
        SDL_FRect toolbar{0.0F, 0.0F, static_cast<float>(width), kToolbarHeight};
        SDL_SetRenderDrawColor(renderer_, 21, 24, 31, 255);
        SDL_RenderFillRect(renderer_, &toolbar);
        DrawButton({10.0F, 10.0F, 80.0F, 32.0F}, "Terrain",
                   mode_ == EditMode::Terrain);
        DrawButton({96.0F, 10.0F, 90.0F, 32.0F}, "Objects",
                   mode_ == EditMode::Placements);
        DrawButton({192.0F, 10.0F, 80.0F, 32.0F}, "Rules",
                   mode_ == EditMode::LevelRules);
        DrawButton({278.0F, 10.0F, 66.0F, 32.0F}, "GG",
                   mode_ == EditMode::GroundGeneration,
                   level_.config.mode == tou::level::LevelMode::GroundGenerated);
        DrawButton({360.0F, 10.0F, 64.0F, 32.0F}, "Undo", false, !undo_.empty());
        DrawButton({430.0F, 10.0F, 64.0F, 32.0F}, "Redo", false, !redo_.empty());
        DrawButton({500.0F, 10.0F, 64.0F, 32.0F}, "Save", dirty_);
        DrawButton({570.0F, 10.0F, 72.0F, 32.0F}, "Export", false);
        DrawButton({648.0F, 10.0F, 54.0F, 32.0F}, "Fit", false);
        DrawButton({708.0F, 10.0F, 76.0F, 32.0F}, "Visual", show_visual_);
        DrawButton({790.0F, 10.0F, 92.0F, 32.0F}, "Attributes", show_attributes_);
        DrawButton({890.0F, 10.0F, 80.0F, 32.0F}, MinusLabel(), false);
        DrawButton({974.0F, 10.0F, 80.0F, 32.0F}, PlusLabel(), false);
        DrawButton({1058.0F, 10.0F, 66.0F, 32.0F}, "Brush -", false);
        DrawButton({1128.0F, 10.0F, 66.0F, 32.0F}, "Brush +", false);
        DrawButton({1198.0F, 10.0F, 74.0F, 32.0F}, ActionLabel(), false);
    }

    void DrawStatusBar(int width, int height) {
        SDL_FRect bar{0.0F, static_cast<float>(height) - kStatusBarHeight,
                      static_cast<float>(width), kStatusBarHeight};
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer_, 21, 24, 31, 255);
        SDL_RenderFillRect(renderer_, &bar);
        SDL_SetRenderDrawColor(renderer_, 74, 82, 98, 255);
        SDL_RenderLine(renderer_, 0.0F, bar.y, static_cast<float>(width), bar.y);
        std::string controls = "BRUSH: " + std::to_string(brush_size_) + " PX   |   ";
        if (mode_ == EditMode::Terrain) {
            controls += "LEFT: PAINT   RIGHT: PICK";
        } else if (mode_ == EditMode::Placements) {
            controls += "LEFT: ADD / SELECT / DRAG   RIGHT: SELECT";
        } else {
            controls += "CLICK A SIDEBAR ROW   +/-: CHANGE";
        }
        controls += "   |   MIDDLE: PAN   WHEEL: ZOOM   F: FIT";
        SDL_SetRenderDrawColor(renderer_, 190, 202, 220, 255);
        SDL_RenderDebugText(renderer_, 10.0F, bar.y + 10.0F, controls.c_str());
    }

    void Render() {
        int width = 0;
        int height = 0;
        SDL_GetRenderOutputSize(renderer_, &width, &height);
        SDL_SetRenderDrawColor(renderer_, 16, 18, 24, 255);
        SDL_RenderClear(renderer_);

        SDL_FRect sidebar{0.0F, kToolbarHeight, kSidebarWidth,
                          static_cast<float>(height) - kToolbarHeight};
        SDL_SetRenderDrawColor(renderer_, 28, 31, 40, 255);
        SDL_RenderFillRect(renderer_, &sidebar);
        SDL_Rect sidebar_clip{0, static_cast<int>(kToolbarHeight),
                              static_cast<int>(kSidebarWidth),
                              std::max(0, height - static_cast<int>(kToolbarHeight))};
        SDL_SetRenderClipRect(renderer_, &sidebar_clip);
        SDL_SetRenderDrawColor(renderer_, 210, 218, 232, 255);
        if (mode_ == EditMode::Terrain) {
            const auto &palette = tou::level::Palette();
            for (std::size_t index = 0; index < palette.size(); ++index) {
                const float y = kContentTop - sidebar_scroll_ +
                    static_cast<float>(index) * kPaletteRowHeight;
                SDL_FRect row{8.0F, y, kSidebarWidth - 16.0F, kPaletteRowHeight - 2.0F};
                SDL_FRect swatch{10.0F, y + 2.0F, 18.0F, 16.0F};
                if (index == selected_palette_) {
                    SDL_SetRenderDrawColor(renderer_, 58, 63, 78, 255);
                    SDL_RenderFillRect(renderer_, &row);
                }
                SDL_SetRenderDrawColor(renderer_, palette[index].rgb.r,
                                       palette[index].rgb.g, palette[index].rgb.b, 255);
                SDL_RenderFillRect(renderer_, &swatch);
                SDL_SetRenderDrawColor(renderer_, index == selected_palette_ ? 255 : 70,
                                       index == selected_palette_ ? 220 : 74,
                                       index == selected_palette_ ? 40 : 84, 255);
                SDL_RenderRect(renderer_, &swatch);
                SDL_SetRenderDrawColor(renderer_, index == selected_palette_ ? 255 : 205,
                                       index == selected_palette_ ? 228 : 211,
                                       index == selected_palette_ ? 120 : 222, 255);
                SDL_RenderDebugText(renderer_, 34.0F, y + 6.0F,
                                    palette[index].name);
            }
        } else if (mode_ == EditMode::Placements) {
            const auto &templates = PlacementTemplates();
            for (std::size_t index = 0; index < templates.size(); ++index) {
                const float y = kContentTop - sidebar_scroll_ +
                    static_cast<float>(index) * 58.0F;
                SDL_FRect row{8.0F, y, kSidebarWidth - 16.0F, 50.0F};
                if (index == selected_template_) {
                    SDL_SetRenderDrawColor(renderer_, 58, 63, 78, 255);
                    SDL_RenderFillRect(renderer_, &row);
                }
                DrawPlacementPreview(index, y);
                SDL_SetRenderDrawColor(renderer_, index == selected_template_ ? 255 : 205,
                                       index == selected_template_ ? 228 : 211,
                                       index == selected_template_ ? 120 : 222, 255);
                SDL_RenderDebugText(renderer_, 64.0F, y + 20.0F,
                                    templates[index].name);
            }
            const float help_y = kContentTop - sidebar_scroll_ +
                static_cast<float>(templates.size()) * 58.0F + 14.0F;
            SDL_SetRenderDrawColor(renderer_, 170, 180, 198, 255);
            SDL_RenderDebugText(renderer_, 10.0F, help_y, "CLICK: ADD / SELECT");
            SDL_RenderDebugText(renderer_, 10.0F, help_y + 14.0F, "DRAG: MOVE   DEL: DELETE");
            if (selected_placement_) {
                const tou::level::Placement &placement =
                    level_.placements[*selected_placement_];
                SDL_SetRenderDrawColor(renderer_, 255, 228, 120, 255);
                SDL_RenderDebugText(renderer_, 10.0F, help_y + 44.0F,
                                    "SELECTED PROPERTIES");
                std::size_t visible_parameter = 0;
                for (std::size_t parameter = 0; parameter < 5; ++parameter) {
                    const int limit = ParameterLimit(placement, parameter);
                    if (limit == 0 && parameter != 0) continue;
                    const float y = help_y + 66.0F +
                        static_cast<float>(visible_parameter++) * 24.0F;
                    const std::string label = std::to_string(parameter + 1U) + " " +
                        ParameterName(placement, parameter) + ": " +
                        ParameterValue(placement, parameter);
                    SDL_SetRenderDrawColor(renderer_,
                        parameter == selected_parameter_ ? 255 : 205,
                        parameter == selected_parameter_ ? 228 : 211,
                        parameter == selected_parameter_ ? 120 : 222, 255);
                    SDL_RenderDebugText(renderer_, 14.0F, y, label.c_str());
                }
                SDL_SetRenderDrawColor(renderer_, 170, 180, 198, 255);
                SDL_RenderDebugText(renderer_, 10.0F,
                                    help_y + 74.0F + visible_parameter * 24.0F,
                                    "CLICK/1-5 SELECT, +/- CHANGE");
            }
        } else if (mode_ == EditMode::LevelRules) {
            const auto &names = RuleNames();
            for (std::size_t index = 0; index < names.size(); ++index) {
                const float y = kContentTop - sidebar_scroll_ +
                    static_cast<float>(index) * 40.0F;
                SDL_FRect row{8.0F, y, kSidebarWidth - 16.0F, 35.0F};
                if (index == selected_rule_) {
                    SDL_SetRenderDrawColor(renderer_, 58, 63, 78, 255);
                    SDL_RenderFillRect(renderer_, &row);
                }
                SDL_SetRenderDrawColor(renderer_, index == selected_rule_ ? 255 : 205,
                                       index == selected_rule_ ? 228 : 211,
                                       index == selected_rule_ ? 120 : 222, 255);
                SDL_RenderDebugText(renderer_, 12.0F, y + 5.0F, names[index]);
                const std::string value = Truncated(RuleValue(index), 34U);
                SDL_RenderDebugText(renderer_, 12.0F, y + 18.0F, value.c_str());
            }
            SDL_SetRenderDrawColor(renderer_, 170, 180, 198, 255);
            SDL_RenderDebugText(renderer_, 10.0F,
                                kContentTop - sidebar_scroll_ +
                                    static_cast<float>(names.size()) * 40.0F + 8.0F,
                                "+/- OR UP/DOWN: CHANGE");
        } else {
            SDL_SetRenderDrawColor(renderer_, 145, 205, 220, 255);
            SDL_RenderDebugText(renderer_, 12.0F, kContentTop - sidebar_scroll_,
                                "RUNTIME PROCEDURAL TERRAIN");
            SDL_SetRenderDrawColor(renderer_, 170, 180, 198, 255);
            SDL_RenderDebugText(renderer_, 12.0F, kContentTop + 16.0F - sidebar_scroll_,
                                "CANVAS = CONSTRAINT MAP");
            for (std::size_t index = 0; index < GroundGenerationRuleCount(); ++index) {
                const float y = kContentTop + 46.0F - sidebar_scroll_ +
                    static_cast<float>(index) * 48.0F;
                SDL_FRect row{8.0F, y, kSidebarWidth - 16.0F, 42.0F};
                if (index == selected_gg_rule_) {
                    SDL_SetRenderDrawColor(renderer_, 58, 63, 78, 255);
                    SDL_RenderFillRect(renderer_, &row);
                }
                SDL_SetRenderDrawColor(renderer_, index == selected_gg_rule_ ? 255 : 205,
                                       index == selected_gg_rule_ ? 228 : 211,
                                       index == selected_gg_rule_ ? 120 : 222, 255);
                const std::string name = GroundGenerationName(index);
                SDL_RenderDebugText(renderer_, 12.0F, y + 6.0F, name.c_str());
                const std::string value = Truncated(GroundGenerationValue(index), 34U);
                SDL_RenderDebugText(renderer_, 12.0F, y + 23.0F, value.c_str());
            }
            SDL_SetRenderDrawColor(renderer_, 170, 180, 198, 255);
            SDL_RenderDebugText(renderer_, 10.0F,
                                kContentTop + 46.0F - sidebar_scroll_ +
                                    static_cast<float>(GroundGenerationRuleCount()) * 48.0F + 8.0F,
                                "CTRL+N: NEW THEME  P: PREVIEW");
        }

        SDL_SetRenderClipRect(renderer_, nullptr);

        SDL_Rect clip{static_cast<int>(kSidebarWidth), static_cast<int>(kToolbarHeight),
                      std::max(0, width - static_cast<int>(kSidebarWidth)),
                      std::max(0, height - static_cast<int>(kToolbarHeight))};
        SDL_SetRenderClipRect(renderer_, &clip);
        const SDL_FRect canvas = CanvasRect();
        SDL_Texture *canvas_visual = visual_texture_;
        if (level_.config.mode == tou::level::LevelMode::GroundGenerated &&
            gg_preview_texture_ != nullptr) {
            canvas_visual = gg_preview_texture_;
        }
        if (show_visual_ && canvas_visual != nullptr) {
            SDL_RenderTexture(renderer_, canvas_visual, nullptr, &canvas);
        }
        if (show_attributes_) {
            SDL_RenderTexture(renderer_, attribute_texture_, nullptr, &canvas);
        }
        for (std::size_t placement_index = 0;
             placement_index < level_.placements.size(); ++placement_index) {
            const tou::level::Placement &placement =
                level_.placements[placement_index];
            const float size = std::max(4.0F, zoom_ * 3.0F);
            const float anchor_x = canvas.x + placement.x * zoom_;
            const float anchor_y = canvas.y + placement.y * zoom_;
            if (placement.type == tou::level::PlacementType::Turret) {
                const std::size_t style = std::min<std::size_t>(
                    placement.parameters[0], kTurretSpriteBases.size() - 1U);
                const std::size_t team = std::min<std::size_t>(
                    placement.parameters[2], 3U);
                const std::size_t frame = placement.parameters[3] == 0
                    ? 0U : std::min<std::size_t>(placement.parameters[3] - 1U, 31U);
                const SpritePreview &preview = turret_previews_[style][team][frame];
                if (preview.texture != nullptr) {
                    SDL_FRect sprite{
                        anchor_x - preview.width * zoom_ / 2.0F,
                        anchor_y - preview.height * zoom_ / 2.0F,
                        preview.width * zoom_, preview.height * zoom_,
                    };
                    SDL_RenderTexture(renderer_, preview.texture, nullptr, &sprite);
                }
            } else if (placement.type == tou::level::PlacementType::Gate) {
                const std::size_t graphic = placement.parameters[4] == 0 ? 0U : 1U;
                const std::size_t facing = placement.parameters[3] & 3U;
                const SpritePreview &preview = gate_previews_[graphic];
                if (preview.texture != nullptr) {
                    SDL_FRect sprite{
                        anchor_x - preview.width * zoom_ / 2.0F,
                        anchor_y - preview.height * zoom_ / 2.0F,
                        preview.width * zoom_, preview.height * zoom_,
                    };
                    SDL_RenderTextureRotated(renderer_, preview.texture, nullptr,
                                             &sprite, facing * 90.0, nullptr,
                                             SDL_FLIP_NONE);
                }
            }
            SDL_FRect marker{
                anchor_x - size / 2.0F,
                anchor_y - size / 2.0F,
                size,
                size,
            };
            const bool selected = selected_placement_ == placement_index;
            SDL_SetRenderDrawColor(renderer_, selected ? 255 : 255,
                                   selected ? 230 : 40,
                                   selected ? 60 : 210, 255);
            SDL_RenderFillRect(renderer_, &marker);
            if (selected) {
                SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
                SDL_RenderRect(renderer_, &marker);
                if (placement.type == tou::level::PlacementType::Gate) {
                    /* Keep the anchor visible over the sprite: it is the exact
                     * coordinate stored in the level placement record. */
                    SDL_SetRenderDrawColor(renderer_, 70, 225, 240, 255);
                    const bool horizontal =
                        placement.parameters[3] == 0 || placement.parameters[3] == 2;
                    if (horizontal) {
                        SDL_RenderLine(renderer_, anchor_x - 40.0F, anchor_y,
                                       anchor_x + 40.0F, anchor_y);
                    } else {
                        SDL_RenderLine(renderer_, anchor_x, anchor_y - 40.0F,
                                       anchor_x, anchor_y + 40.0F);
                    }
                    SDL_RenderLine(renderer_, anchor_x - 5.0F, anchor_y,
                                   anchor_x + 5.0F, anchor_y);
                    SDL_RenderLine(renderer_, anchor_x, anchor_y - 5.0F,
                                   anchor_x, anchor_y + 5.0F);
                    SDL_RenderDebugText(renderer_, anchor_x + 8.0F,
                                        anchor_y + 8.0F, "GATE ANCHOR");
                }
            }
        }
        SDL_SetRenderDrawColor(renderer_, 110, 120, 140, 255);
        SDL_RenderRect(renderer_, &canvas);
        if (gg_preview_texture_ != nullptr && gg_preview_stale_) {
            SDL_SetRenderDrawColor(renderer_, 255, 210, 80, 255);
            SDL_RenderDebugText(renderer_, canvas.x + 8.0F, canvas.y + 8.0F,
                                "GG PREVIEW STALE - REGENERATE");
        }
        SDL_SetRenderClipRect(renderer_, nullptr);
        DrawToolbar();
        DrawStatusBar(width, height);
        if (text_field_ != TextField::None) {
            SDL_FRect shade{0.0F, 0.0F, static_cast<float>(width), static_cast<float>(height)};
            SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 180);
            SDL_RenderFillRect(renderer_, &shade);
            SDL_FRect dialog{static_cast<float>(width) / 2.0F - 260.0F,
                             static_cast<float>(height) / 2.0F - 64.0F,
                             520.0F, 128.0F};
            SDL_SetRenderDrawColor(renderer_, 28, 31, 40, 255);
            SDL_RenderFillRect(renderer_, &dialog);
            SDL_SetRenderDrawColor(renderer_, 100, 180, 205, 255);
            SDL_RenderRect(renderer_, &dialog);
            const char *prompt = text_field_ == TextField::Maker ? "EDIT MAKER"
                : text_field_ == TextField::Email ? "EDIT EMAIL"
                : text_field_ == TextField::ThemeName ? "NEW GG THEME NAME"
                : text_field_ == TextField::SignFirst ? "EDIT SIGN LINE 1"
                                                      : "EDIT SIGN LINE 2";
            SDL_SetRenderDrawColor(renderer_, 225, 232, 240, 255);
            SDL_RenderDebugText(renderer_, dialog.x + 16.0F, dialog.y + 16.0F, prompt);
            SDL_FRect input{dialog.x + 16.0F, dialog.y + 42.0F,
                            dialog.w - 32.0F, 30.0F};
            SDL_SetRenderDrawColor(renderer_, 12, 14, 19, 255);
            SDL_RenderFillRect(renderer_, &input);
            SDL_SetRenderDrawColor(renderer_, 105, 120, 140, 255);
            SDL_RenderRect(renderer_, &input);
            SDL_SetRenderDrawColor(renderer_, 255, 228, 120, 255);
            const std::string shown = Truncated(text_buffer_, 58U) + "_";
            SDL_RenderDebugText(renderer_, input.x + 8.0F, input.y + 10.0F,
                                shown.c_str());
            SDL_SetRenderDrawColor(renderer_, 170, 180, 198, 255);
            SDL_RenderDebugText(renderer_, dialog.x + 16.0F, dialog.y + 91.0F,
                                "ENTER: ACCEPT    ESC: CANCEL");
        }
        SDL_RenderPresent(renderer_);
    }

    tou::level::Project project_;
    tou::level::LevelData level_;
    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *visual_texture_ = nullptr;
    SDL_Texture *attribute_texture_ = nullptr;
    SDL_Texture *gg_preview_texture_ = nullptr;
    std::array<std::array<std::array<SpritePreview, 32>, 4>, 7> turret_previews_{};
    std::array<SpritePreview, 2> gate_previews_{};
    bool running_ = true;
    bool dirty_ = false;
    bool panning_ = false;
    bool painting_ = false;
    bool dragging_placement_ = false;
    bool drag_edit_started_ = false;
    bool show_visual_ = true;
    bool show_attributes_ = true;
    bool gg_preview_stale_ = true;
    EditMode mode_ = EditMode::Terrain;
    float zoom_ = 1.0F;
    float pan_x_ = 0.0F;
    float pan_y_ = 0.0F;
    float sidebar_scroll_ = 0.0F;
    int brush_size_ = 9;
    int overlay_alpha_ = 144;
    std::uint8_t selected_palette_ = 1;
    std::size_t selected_template_ = 0;
    std::optional<std::size_t> selected_placement_;
    std::optional<std::size_t> pairing_source_;
    std::size_t selected_parameter_ = 0;
    std::size_t selected_rule_ = 0;
    TextField text_field_ = TextField::None;
    std::string text_buffer_;
    std::size_t text_sign_index_ = 0;
    std::vector<EditorSnapshot> undo_;
    std::vector<EditorSnapshot> redo_;
    EditorSnapshot saved_;
    std::vector<std::uint8_t> available_parallax_payload_;
    std::vector<std::string> themes_;
    std::size_t selected_gg_rule_ = 0;
};

}  // namespace

int main(int argc, char **argv) {
    try {
        const std::optional<std::filesystem::path> project = argc > 1
            ? std::optional<std::filesystem::path>(
                  std::filesystem::path(argv[1]))
            : ChooseStartupProject();
        if (!project) return 0;
        Editor editor(*project);
        return editor.Run();
    } catch (const std::exception &error) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "TOU Level Editor",
                                 error.what(), nullptr);
        std::cerr << "TOU Level Editor: " << error.what() << '\n';
        return 1;
    }
}
