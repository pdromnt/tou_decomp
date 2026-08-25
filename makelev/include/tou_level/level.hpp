#ifndef TOU_LEVEL_LEVEL_HPP
#define TOU_LEVEL_LEVEL_HPP

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tou::level {

constexpr std::size_t kConfigSize = 0x39C;
constexpr std::size_t kPlacementSize = 20;
constexpr std::size_t kMaxPlacements = 1024;

struct Rgb {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;

    bool operator==(const Rgb &other) const;
};

struct PaletteEntry {
    std::uint8_t index;
    Rgb rgb;
    const char *name;
    bool metadata;
};

const std::array<PaletteEntry, 34> &Palette();

enum class LevelMode {
    Normal,
    GroundGenerated,
};

enum class PlacementType : std::uint8_t {
    Turret = 0,
    Gate = 1,
    Object = 2,
    StartingPlace = 3,
    Teleport = 4,
};

struct Placement {
    std::int32_t x = 0;
    std::int32_t y = 0;
    PlacementType type = PlacementType::Turret;
    std::array<std::uint8_t, 5> parameters{};
};

struct SignText {
    std::string first;
    std::string second;
};

struct LevelConfig {
    std::string maker = "Anonymous";
    std::string email;
    bool parallax = false;
    std::uint8_t civilians = 10;
    std::uint8_t bombing = 10;
    Rgb water{80, 90, 170};
    bool disable_running_water = false;
    std::uint8_t gravity_tenths = 10;
    std::uint8_t resistance_tenths = 10;
    std::uint8_t collision_damage_tenths = 10;
    std::uint8_t bouncing_tenths = 10;
    std::uint8_t ambient = 0;
    std::uint8_t parallax_aftertouch = 0;
    LevelMode mode = LevelMode::Normal;
    std::string gg_theme = "the earth";
    bool gg_shape = true;
    std::uint8_t repair_density = 20;
    std::uint8_t stuff_density = 20;
    std::uint8_t sign_density = 20;
    std::uint32_t random_seed = 12345;
    std::vector<SignText> sign_texts;
};

struct LevelData {
    LevelConfig config;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint8_t> main_payload;
    std::vector<std::uint8_t> parallax_payload;
    std::vector<Placement> placements;
    std::vector<std::uint8_t> attributes;
};

struct Project {
    std::filesystem::path project_path;
    std::filesystem::path visual_path;
    std::filesystem::path attribute_path;
    std::filesystem::path parallax_path;
    LevelConfig config;
};

struct ValidationMessage {
    enum class Severity { Warning, Error } severity;
    std::string text;
};

struct Comparison {
    std::size_t left_size = 0;
    std::size_t right_size = 0;
    std::size_t raw_differences = 0;
    std::size_t meaningful_differences = 0;
    std::vector<std::size_t> first_raw_offsets;
    std::vector<std::size_t> first_meaningful_offsets;
};

Project LoadProject(const std::filesystem::path &path);
void SaveProject(const Project &project, const std::filesystem::path &path);
Project CreateNormalProject(const std::filesystem::path &project_path,
                            const std::filesystem::path &visual_path,
                            const std::filesystem::path &parallax_path = {});
Project CreateGroundGeneratedProject(const std::filesystem::path &project_path,
                                     std::uint16_t width,
                                     std::uint16_t height,
                                     const std::string &theme);
Project ImportLevelProject(const std::filesystem::path &level_path,
                           const std::filesystem::path &project_path);
LevelData CompileProject(const Project &project);

LevelData ReadLevel(const std::filesystem::path &path);
void WriteLevel(const LevelData &level, const std::filesystem::path &path);

std::vector<ValidationMessage> ValidateProject(const Project &project);
std::vector<ValidationMessage> ValidateLevel(const LevelData &level);
Comparison CompareLevels(const std::filesystem::path &left,
                         const std::filesystem::path &right);

void SaveAttributeTga(const LevelData &level, const std::filesystem::path &path);
std::string ToJson(const LevelData &level);

}  // namespace tou::level

#endif
