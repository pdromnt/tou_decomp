#include "tou_level/level.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "stb_image.h"

#ifndef TOU_DEFAULT_LEVEL_PROJECT
#define TOU_DEFAULT_LEVEL_PROJECT ""
#endif

namespace {

constexpr float kSidebarWidth = 224.0F;
constexpr float kPaletteTop = 32.0F;
constexpr float kPaletteRowHeight = 22.0F;

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
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (renderer_ == nullptr) {
            throw std::runtime_error(SDL_GetError());
        }
        SDL_SetRenderVSync(renderer_, 1);
        LoadVisualTexture();
        CreateAttributeTexture();
        FitCanvas();
        UpdateTitle("ready");
    }

    ~Editor() {
        if (attribute_texture_ != nullptr) SDL_DestroyTexture(attribute_texture_);
        if (visual_texture_ != nullptr) SDL_DestroyTexture(visual_texture_);
        if (renderer_ != nullptr) SDL_DestroyRenderer(renderer_);
        if (window_ != nullptr) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    int Run() {
        while (running_) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                HandleEvent(event);
            }
            Render();
        }
        return 0;
    }

private:
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
        const float available_height = std::max(1.0F, height - 24.0F);
        zoom_ = std::min(available_width / level_.width,
                         available_height / level_.height);
        zoom_ = std::clamp(zoom_, 0.05F, 32.0F);
        pan_x_ = 12.0F;
        pan_y_ = 12.0F;
    }

    SDL_FRect CanvasRect() const {
        return {
            kSidebarWidth + pan_x_,
            pan_y_,
            level_.width * zoom_,
            level_.height * zoom_,
        };
    }

    bool MapPoint(float screen_x, float screen_y, int &map_x, int &map_y) const {
        const SDL_FRect canvas = CanvasRect();
        map_x = static_cast<int>(std::floor((screen_x - canvas.x) / zoom_));
        map_y = static_cast<int>(std::floor((screen_y - canvas.y) / zoom_));
        return map_x >= 0 && map_y >= 0 &&
               map_x < level_.width && map_y < level_.height;
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
        if (x < 8.0F || x >= kSidebarWidth - 8.0F || y < kPaletteTop) {
            return;
        }
        const int index = static_cast<int>((y - kPaletteTop) / kPaletteRowHeight);
        if (index >= 0 && index < 34) {
            selected_palette_ = static_cast<std::uint8_t>(index);
            UpdateTitle(dirty_ ? "modified" : "ready");
        }
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

    void Save() {
        tou::level::SaveAttributeTga(level_, project_.attribute_path);
        tou::level::SaveProject(project_, project_.project_path);
        dirty_ = false;
        UpdateTitle("saved");
    }

    void Export() {
        tou::level::WriteLevel(level_, ExportPath());
        UpdateTitle("exported " + ExportPath().filename().string());
    }

    void UpdateTitle(const std::string &status) {
        const tou::level::PaletteEntry &entry = tou::level::Palette()[selected_palette_];
        const std::string title = "TOU Level Editor - " +
            project_.project_path.filename().string() + " - " + entry.name +
            " - brush " + std::to_string(brush_size_) + " - " + status +
            " | LMB paint, RMB pick, MMB pan, wheel zoom, Ctrl+S save, Ctrl+E export";
        SDL_SetWindowTitle(window_, title.c_str());
    }

    void HandleEvent(const SDL_Event &event) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            running_ = false;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (event.button.x < kSidebarWidth) {
                    SelectPalette(event.button.x, event.button.y);
                } else {
                    Paint(event.button.x, event.button.y);
                }
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                Pick(event.button.x, event.button.y);
            } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                panning_ = true;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_MIDDLE) {
                panning_ = false;
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (panning_) {
                pan_x_ += event.motion.xrel;
                pan_y_ += event.motion.yrel;
            } else if ((event.motion.state & SDL_BUTTON_LMASK) != 0 &&
                       event.motion.x >= kSidebarWidth) {
                Paint(event.motion.x, event.motion.y);
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL: {
            const float old_zoom = zoom_;
            zoom_ = std::clamp(zoom_ * (event.wheel.y > 0 ? 1.2F : 1.0F / 1.2F),
                               0.05F, 32.0F);
            float mouse_x = 0.0F;
            float mouse_y = 0.0F;
            SDL_GetMouseState(&mouse_x, &mouse_y);
            pan_x_ = mouse_x - kSidebarWidth -
                (mouse_x - kSidebarWidth - pan_x_) * (zoom_ / old_zoom);
            pan_y_ = mouse_y - (mouse_y - pan_y_) * (zoom_ / old_zoom);
            break;
        }
        case SDL_EVENT_KEY_DOWN: {
            const bool control = (event.key.mod & SDL_KMOD_CTRL) != 0;
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) running_ = false;
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
                overlay_alpha_ = std::max(16, overlay_alpha_ - 16);
                UpdateAttributeTexture();
            }
            if (event.key.scancode == SDL_SCANCODE_EQUALS) {
                overlay_alpha_ = std::min(255, overlay_alpha_ + 16);
                UpdateAttributeTexture();
            }
            if (control && event.key.scancode == SDL_SCANCODE_S) Save();
            if (control && event.key.scancode == SDL_SCANCODE_E) Export();
            break;
        }
        default:
            break;
        }
    }

    void Render() {
        int width = 0;
        int height = 0;
        SDL_GetRenderOutputSize(renderer_, &width, &height);
        SDL_SetRenderDrawColor(renderer_, 16, 18, 24, 255);
        SDL_RenderClear(renderer_);

        SDL_FRect sidebar{0.0F, 0.0F, kSidebarWidth, static_cast<float>(height)};
        SDL_SetRenderDrawColor(renderer_, 28, 31, 40, 255);
        SDL_RenderFillRect(renderer_, &sidebar);
        SDL_SetRenderDrawColor(renderer_, 210, 218, 232, 255);
        SDL_RenderDebugText(renderer_, 10.0F, 10.0F, "TERRAIN / ATTRIBUTES");
        const auto &palette = tou::level::Palette();
        for (std::size_t index = 0; index < palette.size(); ++index) {
            const float y = kPaletteTop + static_cast<float>(index) * kPaletteRowHeight;
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

        SDL_Rect clip{static_cast<int>(kSidebarWidth), 0,
                      std::max(0, width - static_cast<int>(kSidebarWidth)), height};
        SDL_SetRenderClipRect(renderer_, &clip);
        const SDL_FRect canvas = CanvasRect();
        if (show_visual_ && visual_texture_ != nullptr) {
            SDL_RenderTexture(renderer_, visual_texture_, nullptr, &canvas);
        }
        if (show_attributes_) {
            SDL_RenderTexture(renderer_, attribute_texture_, nullptr, &canvas);
        }
        for (const tou::level::Placement &placement : level_.placements) {
            const float size = std::max(4.0F, zoom_ * 3.0F);
            SDL_FRect marker{
                canvas.x + placement.x * zoom_ - size / 2.0F,
                canvas.y + placement.y * zoom_ - size / 2.0F,
                size,
                size,
            };
            SDL_SetRenderDrawColor(renderer_, 255, 40, 210, 255);
            SDL_RenderFillRect(renderer_, &marker);
        }
        SDL_SetRenderDrawColor(renderer_, 110, 120, 140, 255);
        SDL_RenderRect(renderer_, &canvas);
        SDL_SetRenderClipRect(renderer_, nullptr);
        SDL_RenderPresent(renderer_);
    }

    tou::level::Project project_;
    tou::level::LevelData level_;
    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *visual_texture_ = nullptr;
    SDL_Texture *attribute_texture_ = nullptr;
    bool running_ = true;
    bool dirty_ = false;
    bool panning_ = false;
    bool show_visual_ = true;
    bool show_attributes_ = true;
    float zoom_ = 1.0F;
    float pan_x_ = 0.0F;
    float pan_y_ = 0.0F;
    int brush_size_ = 1;
    int overlay_alpha_ = 144;
    std::uint8_t selected_palette_ = 1;
};

}  // namespace

int main(int argc, char **argv) {
    try {
        const std::filesystem::path project = argc > 1
            ? std::filesystem::path(argv[1])
            : std::filesystem::path(TOU_DEFAULT_LEVEL_PROJECT);
        if (project.empty()) {
            throw std::runtime_error("Pass a .toulevel.json project path");
        }
        Editor editor(project);
        return editor.Run();
    } catch (const std::exception &error) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "TOU Level Editor",
                                 error.what(), nullptr);
        std::cerr << "TOU Level Editor: " << error.what() << '\n';
        return 1;
    }
}
