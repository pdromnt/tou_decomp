#include "tou_level/level.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace tou::level {
namespace {

using json = nlohmann::json;

constexpr char kMagic[] = "TOU level file v1.4";
constexpr std::size_t kHeaderSize = 0x22;
constexpr std::size_t kMainOffset = kHeaderSize + kConfigSize;

std::vector<std::uint8_t> ReadBytes(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Could not open " + path.string());
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff length = stream.tellg();
    if (length < 0) {
        throw std::runtime_error("Could not determine size of " + path.string());
    }
    stream.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        stream.read(reinterpret_cast<char *>(bytes.data()), length);
    }
    if (!stream) {
        throw std::runtime_error("Could not read " + path.string());
    }
    return bytes;
}

void WriteBytes(const std::filesystem::path &path,
                const std::vector<std::uint8_t> &bytes) {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw std::runtime_error("Could not create " + path.string());
    }
    if (!bytes.empty()) {
        stream.write(reinterpret_cast<const char *>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream) {
        throw std::runtime_error("Could not write " + path.string());
    }
}

std::uint16_t ReadU16(const std::vector<std::uint8_t> &bytes,
                      std::size_t offset) {
    if (offset + 2 > bytes.size()) {
        throw std::runtime_error("Unexpected end of data while reading uint16");
    }
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(bytes[offset + 1] << 8U);
}

std::uint32_t ReadU32(const std::vector<std::uint8_t> &bytes,
                      std::size_t offset) {
    if (offset + 4 > bytes.size()) {
        throw std::runtime_error("Unexpected end of data while reading uint32");
    }
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

void AppendU16(std::vector<std::uint8_t> &bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void AppendU32(std::vector<std::uint8_t> &bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void StoreU32(std::vector<std::uint8_t> &bytes, std::size_t offset,
              std::uint32_t value) {
    if (offset + 4 > bytes.size()) {
        throw std::runtime_error("Internal writer offset is out of range");
    }
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

std::pair<std::uint16_t, std::uint16_t> JpegDimensions(
    const std::vector<std::uint8_t> &bytes) {
    if (bytes.size() < 4 || bytes[0] != 0xFF || bytes[1] != 0xD8) {
        throw std::runtime_error("Asset is not a JPEG file");
    }
    std::size_t position = 2;
    while (position + 4 <= bytes.size()) {
        if (bytes[position] != 0xFF) {
            ++position;
            continue;
        }
        while (position < bytes.size() && bytes[position] == 0xFF) {
            ++position;
        }
        if (position >= bytes.size()) {
            break;
        }
        const std::uint8_t marker = bytes[position++];
        if (marker == 0xD8 || marker == 0xD9 ||
            (marker >= 0xD0 && marker <= 0xD7)) {
            continue;
        }
        if (position + 2 > bytes.size()) {
            break;
        }
        const std::size_t length =
            (static_cast<std::size_t>(bytes[position]) << 8U) |
            bytes[position + 1];
        if (length < 2 || position + length > bytes.size()) {
            break;
        }
        const bool is_sof =
            (marker >= 0xC0 && marker <= 0xC3) ||
            (marker >= 0xC5 && marker <= 0xC7) ||
            (marker >= 0xC9 && marker <= 0xCB) ||
            (marker >= 0xCD && marker <= 0xCF);
        if (is_sof && length >= 7) {
            const std::uint16_t height =
                static_cast<std::uint16_t>((bytes[position + 3] << 8U) |
                                           bytes[position + 4]);
            const std::uint16_t width =
                static_cast<std::uint16_t>((bytes[position + 5] << 8U) |
                                           bytes[position + 6]);
            return {width, height};
        }
        position += length;
    }
    throw std::runtime_error("JPEG dimensions could not be recovered");
}

struct TgaImage {
    std::uint16_t width;
    std::uint16_t height;
    std::vector<Rgb> pixels;
};

TgaImage LoadTga(const std::filesystem::path &path) {
    const std::vector<std::uint8_t> bytes = ReadBytes(path);
    if (bytes.size() < 18 || bytes[1] != 0 || bytes[2] != 2 || bytes[16] != 24) {
        throw std::runtime_error(
            "Attribute map must be an uncompressed 24-bit true-color TGA");
    }
    const std::uint16_t width = ReadU16(bytes, 12);
    const std::uint16_t height = ReadU16(bytes, 14);
    if (width == 0 || height == 0) {
        throw std::runtime_error("Attribute TGA has invalid dimensions");
    }
    const std::size_t pixel_offset = 18U + bytes[0];
    const std::size_t pixel_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (pixel_offset + pixel_count * 3U > bytes.size()) {
        throw std::runtime_error("Attribute TGA pixel data is truncated");
    }

    const bool top_origin = (bytes[17] & 0x20U) != 0;
    const bool right_origin = (bytes[17] & 0x10U) != 0;
    TgaImage image{width, height, std::vector<Rgb>(pixel_count)};
    for (std::uint16_t source_y = 0; source_y < height; ++source_y) {
        for (std::uint16_t source_x = 0; source_x < width; ++source_x) {
            const std::size_t source = pixel_offset +
                (static_cast<std::size_t>(source_y) * width + source_x) * 3U;
            const std::uint16_t x = right_origin
                ? static_cast<std::uint16_t>(width - 1U - source_x)
                : source_x;
            const std::uint16_t y = top_origin
                ? source_y
                : static_cast<std::uint16_t>(height - 1U - source_y);
            image.pixels[static_cast<std::size_t>(y) * width + x] =
                Rgb{bytes[source + 2], bytes[source + 1], bytes[source]};
        }
    }
    return image;
}

std::string Utf8ToLatin1(const std::string &value) {
    std::string result;
    for (std::size_t index = 0; index < value.size();) {
        const std::uint8_t first = static_cast<std::uint8_t>(value[index]);
        std::uint32_t codepoint = 0;
        std::size_t count = 1;
        if ((first & 0x80U) == 0) {
            codepoint = first;
        } else if ((first & 0xE0U) == 0xC0U && index + 1 < value.size()) {
            codepoint = static_cast<std::uint32_t>(first & 0x1FU) << 6U;
            codepoint |= static_cast<std::uint8_t>(value[index + 1]) & 0x3FU;
            count = 2;
        } else {
            throw std::runtime_error("Legacy level strings only support Latin-1");
        }
        if (codepoint > 0xFFU) {
            throw std::runtime_error("Legacy level strings only support Latin-1");
        }
        result.push_back(static_cast<char>(codepoint));
        index += count;
    }
    return result;
}

std::string Latin1ToUtf8(const std::uint8_t *bytes, std::size_t length) {
    std::string result;
    for (std::size_t index = 0; index < length && bytes[index] != 0; ++index) {
        const std::uint8_t value = bytes[index];
        if (value < 0x80U) {
            result.push_back(static_cast<char>(value));
        } else {
            result.push_back(static_cast<char>(0xC0U | (value >> 6U)));
            result.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
    }
    return result;
}

void StoreString(std::vector<std::uint8_t> &bytes, std::size_t offset,
                 std::size_t capacity, const std::string &value) {
    const std::string encoded = Utf8ToLatin1(value);
    if (encoded.size() >= capacity) {
        throw std::runtime_error("Level string exceeds its legacy field capacity");
    }
    std::copy(encoded.begin(), encoded.end(), bytes.begin() +
              static_cast<std::ptrdiff_t>(offset));
}

std::vector<std::uint8_t> EncodeConfig(const LevelConfig &config) {
    if (config.sign_texts.size() > 16) {
        throw std::runtime_error("A level can contain at most 16 sign-text records");
    }
    std::vector<std::uint8_t> bytes(kConfigSize, 0);
    StoreString(bytes, 0x000, 0x80, config.maker);
    StoreString(bytes, 0x080, 0x80, config.email);
    bytes[0x100] = config.parallax ? 1 : 0;
    bytes[0x101] = config.civilians;
    bytes[0x102] = config.bombing;
    bytes[0x103] = config.water.r;
    bytes[0x104] = config.water.g;
    bytes[0x105] = config.water.b;
    bytes[0x106] = config.disable_running_water ? 1 : 0;
    bytes[0x107] = config.gravity_tenths;
    bytes[0x108] = config.resistance_tenths;
    bytes[0x109] = config.collision_damage_tenths;
    bytes[0x10A] = config.bouncing_tenths;
    bytes[0x10B] = config.ambient;
    bytes[0x10C] = config.parallax_aftertouch;
    bytes[0x10D] = config.mode == LevelMode::GroundGenerated ? 1 : 0;
    StoreString(bytes, 0x10E, 0x80, config.gg_theme);
    bytes[0x18E] = config.gg_shape ? 1 : 0;
    bytes[0x18F] = config.repair_density;
    bytes[0x190] = config.stuff_density;
    bytes[0x191] = config.sign_density;
    StoreU32(bytes, 0x194, config.random_seed);
    for (std::size_t index = 0; index < config.sign_texts.size(); ++index) {
        const std::size_t offset = 0x198 + index * 32U;
        StoreString(bytes, offset, 16, config.sign_texts[index].first);
        StoreString(bytes, offset + 16, 16, config.sign_texts[index].second);
    }
    bytes[0x398] = static_cast<std::uint8_t>(config.sign_texts.size());
    return bytes;
}

LevelConfig DecodeConfig(const std::vector<std::uint8_t> &bytes,
                         std::size_t offset) {
    if (offset + kConfigSize > bytes.size()) {
        throw std::runtime_error("Level config is truncated");
    }
    const std::uint8_t *config = bytes.data() + offset;
    LevelConfig result;
    result.maker = Latin1ToUtf8(config + 0x000, 0x80);
    result.email = Latin1ToUtf8(config + 0x080, 0x80);
    result.parallax = config[0x100] != 0;
    result.civilians = config[0x101];
    result.bombing = config[0x102];
    result.water = {config[0x103], config[0x104], config[0x105]};
    result.disable_running_water = config[0x106] != 0;
    result.gravity_tenths = config[0x107];
    result.resistance_tenths = config[0x108];
    result.collision_damage_tenths = config[0x109];
    result.bouncing_tenths = config[0x10A];
    result.ambient = config[0x10B];
    result.parallax_aftertouch = config[0x10C];
    result.mode = config[0x10D] != 0
        ? LevelMode::GroundGenerated : LevelMode::Normal;
    result.gg_theme = Latin1ToUtf8(config + 0x10E, 0x80);
    result.gg_shape = config[0x18E] != 0;
    result.repair_density = config[0x18F];
    result.stuff_density = config[0x190];
    result.sign_density = config[0x191];
    result.random_seed = static_cast<std::uint32_t>(config[0x194]) |
        (static_cast<std::uint32_t>(config[0x195]) << 8U) |
        (static_cast<std::uint32_t>(config[0x196]) << 16U) |
        (static_cast<std::uint32_t>(config[0x197]) << 24U);
    const std::size_t sign_count = std::min<std::size_t>(config[0x398], 16);
    for (std::size_t index = 0; index < sign_count; ++index) {
        const std::size_t sign_offset = 0x198 + index * 32U;
        result.sign_texts.push_back({
            Latin1ToUtf8(config + sign_offset, 16),
            Latin1ToUtf8(config + sign_offset + 16, 16),
        });
    }
    return result;
}

int PaletteIndex(const Rgb &rgb) {
    const auto &palette = Palette();
    const auto found = std::find_if(palette.begin(), palette.end(),
        [&rgb](const PaletteEntry &entry) { return entry.rgb == rgb; });
    return found == palette.end() ? -1 : found->index;
}

bool ExtractPlacement(const Rgb &rgb, std::int32_t x, std::int32_t y,
                      Placement &placement, std::uint8_t &replacement) {
    replacement = 0;
    placement.x = x;
    placement.y = y;
    switch (rgb.b) {
    case 230: {
        static constexpr std::array<std::uint8_t, 7> styles{0, 1, 5, 2, 3, 4, 6};
        const std::uint8_t raw_style = std::min<std::uint8_t>(rgb.r & 0x0FU, 6);
        placement.type = PlacementType::Turret;
        placement.parameters = {
            styles[raw_style],
            static_cast<std::uint8_t>(rgb.r >> 4U),
            static_cast<std::uint8_t>(rgb.g & 3U),
            static_cast<std::uint8_t>(rgb.g >> 2U),
            0,
        };
        return true;
    }
    case 120:
        placement.type = PlacementType::Gate;
        placement.parameters = {
            static_cast<std::uint8_t>(rgb.r & 0x7FU),
            static_cast<std::uint8_t>(rgb.r >> 7U),
            static_cast<std::uint8_t>(rgb.g & 3U),
            static_cast<std::uint8_t>((rgb.g >> 2U) & 3U),
            static_cast<std::uint8_t>(rgb.g >> 4U),
        };
        return true;
    case 180:
        placement.type = PlacementType::Object;
        placement.parameters = {
            rgb.r,
            static_cast<std::uint8_t>(rgb.g & 7U),
            static_cast<std::uint8_t>((rgb.g >> 3U) & 3U),
            static_cast<std::uint8_t>(rgb.g >> 5U),
            0,
        };
        if (rgb.r <= 1) {
            replacement = 1;
        }
        return true;
    case 140:
        placement.type = PlacementType::StartingPlace;
        placement.parameters = {rgb.r, 0, 0, 0, 0};
        return true;
    case 190:
        placement.type = PlacementType::Teleport;
        placement.parameters = {
            static_cast<std::uint8_t>(rgb.r & 0x3FU),
            static_cast<std::uint8_t>(rgb.r >> 6U),
            rgb.g,
            0,
            0,
        };
        return true;
    default:
        return false;
    }
}

Rgb EncodePlacementMarker(const Placement &placement) {
    const auto &p = placement.parameters;
    switch (placement.type) {
    case PlacementType::Turret: {
        static constexpr std::array<std::uint8_t, 7> inverse{0, 1, 3, 4, 5, 2, 6};
        const std::uint8_t style = p[0] < inverse.size() ? inverse[p[0]] : 6;
        return {
            static_cast<std::uint8_t>((p[1] << 4U) | style),
            static_cast<std::uint8_t>((p[3] << 2U) | (p[2] & 3U)),
            230,
        };
    }
    case PlacementType::Gate:
        return {
            static_cast<std::uint8_t>((p[1] << 7U) | (p[0] & 0x7FU)),
            static_cast<std::uint8_t>((p[4] << 4U) | ((p[3] & 3U) << 2U) |
                                      (p[2] & 3U)),
            120,
        };
    case PlacementType::Object:
        return {
            p[0],
            static_cast<std::uint8_t>((p[3] << 5U) | ((p[2] & 3U) << 3U) |
                                      (p[1] & 7U)),
            180,
        };
    case PlacementType::StartingPlace:
        return {p[0], 0, 140};
    case PlacementType::Teleport:
        return {
            static_cast<std::uint8_t>((p[1] << 6U) | (p[0] & 0x3FU)),
            p[2],
            190,
        };
    }
    throw std::runtime_error("Unknown placement type");
}

std::vector<std::uint8_t> EncodeRle(const std::vector<std::uint8_t> &attributes) {
    if (attributes.empty()) {
        throw std::runtime_error("Cannot encode an empty attribute map");
    }
    std::vector<std::uint8_t> result;
    std::uint8_t current = attributes.front();
    std::uint16_t run = 0;
    const auto flush = [&result](std::uint8_t index, std::uint16_t length) {
        result.push_back(static_cast<std::uint8_t>((index << 2U) | (length & 3U)));
        result.push_back(static_cast<std::uint8_t>(length >> 2U));
    };
    for (const std::uint8_t index : attributes) {
        if (index > 33) {
            throw std::runtime_error("Attribute palette index is out of range");
        }
        if (index != current || run == 0x3FFU) {
            flush(current, run);
            current = index;
            run = 0;
        }
        ++run;
    }
    flush(current, run);
    result.push_back(0xFF);
    result.push_back(0xFF);
    return result;
}

std::filesystem::path ResolveAsset(const std::filesystem::path &project_path,
                                   const std::string &value) {
    if (value.empty()) {
        return {};
    }
    const std::filesystem::path path = std::filesystem::u8path(value);
    return path.is_absolute() ? path : project_path.parent_path() / path;
}

std::string RelativeAsset(const std::filesystem::path &project_path,
                          const std::filesystem::path &asset_path) {
    if (asset_path.empty()) {
        return {};
    }
    std::error_code error;
    const std::filesystem::path relative =
        std::filesystem::relative(asset_path, project_path.parent_path(), error);
    return (error ? asset_path : relative).generic_u8string();
}

std::uint8_t ByteValue(const json &object, const char *key,
                       std::uint8_t fallback) {
    const int value = object.value(key, static_cast<int>(fallback));
    if (value < 0 || value > 255) {
        throw std::runtime_error(std::string(key) + " must be between 0 and 255");
    }
    return static_cast<std::uint8_t>(value);
}

bool IsIgnoredPlacementPadding(const std::vector<std::uint8_t> &bytes,
                               std::size_t offset) {
    if (bytes.size() < kHeaderSize) {
        return false;
    }
    const std::size_t placement_offset = ReadU32(bytes, 0x1E);
    if (placement_offset + 4 > bytes.size()) {
        return false;
    }
    const std::size_t count = ReadU32(bytes, placement_offset);
    const std::size_t records_start = placement_offset + 4;
    if (offset < records_start || offset >= records_start + count * kPlacementSize) {
        return false;
    }
    return ((offset - records_start) % kPlacementSize) >= 14;
}

void ValidateConfigFields(const LevelConfig &config,
                          std::vector<ValidationMessage> &messages) {
    const auto error = [&messages](const std::string &text) {
        messages.push_back({ValidationMessage::Severity::Error, text});
    };
    const auto check_string = [&error](const char *name,
                                       const std::string &value,
                                       std::size_t capacity) {
        try {
            if (Utf8ToLatin1(value).size() >= capacity) {
                error(std::string(name) + " exceeds its " +
                      std::to_string(capacity - 1U) + " character limit");
            }
        } catch (const std::exception &exception) {
            error(std::string(name) + ": " + exception.what());
        }
    };
    check_string("Maker", config.maker, 0x80);
    check_string("Email", config.email, 0x80);
    check_string("GG theme", config.gg_theme, 0x80);
    if (config.civilians > 100 || config.bombing > 100) {
        error("Civilians and bombing must be between 0 and 100");
    }
    if (config.repair_density > 100 || config.stuff_density > 100 ||
        config.sign_density > 100) {
        error("GG densities must be between 0 and 100");
    }
    if (config.sign_texts.size() > 16) {
        error("A level can contain at most 16 sign-text records");
    }
    for (std::size_t index = 0; index < config.sign_texts.size(); ++index) {
        check_string(("Sign " + std::to_string(index + 1U) + " first line").c_str(),
                     config.sign_texts[index].first, 16);
        check_string(("Sign " + std::to_string(index + 1U) + " second line").c_str(),
                     config.sign_texts[index].second, 16);
    }
}

}  // namespace

bool Rgb::operator==(const Rgb &other) const {
    return r == other.r && g == other.g && b == other.b;
}

const std::array<PaletteEntry, 34> &Palette() {
    static const std::array<PaletteEntry, 34> palette{{
        {0, {0, 0, 0}, "Transparent", false},
        {1, {0, 152, 0}, "Normal land", false},
        {2, {126, 48, 59}, "Brick wall", false},
        {3, {77, 77, 77}, "Indestructible", false},
        {4, {168, 168, 168}, "Repair place", false},
        {5, {160, 0, 0}, "Exploding wall", false},
        {6, {227, 0, 0}, "Huge exploding wall", false},
        {7, {255, 255, 255}, "Snow", false},
        {8, {189, 120, 65}, "Burning", false},
        {9, {133, 84, 45}, "Burning (ashes remain)", false},
        {10, {255, 169, 96}, "Flesh/meat", false},
        {11, {0, 0, 200}, "Water up, power 1", false},
        {12, {0, 0, 255}, "Water up, power 2", false},
        {13, {50, 50, 200}, "Water down, power 1", false},
        {14, {50, 50, 255}, "Water down, power 2", false},
        {15, {100, 100, 200}, "Water left, power 1", false},
        {16, {100, 100, 255}, "Water left, power 2", false},
        {17, {150, 150, 200}, "Water right, power 1", false},
        {18, {150, 150, 255}, "Water right, power 2", false},
        {19, {50, 50, 0}, "Air (transparent)", false},
        {20, {100, 100, 0}, "Air (non-transparent)", false},
        {21, {150, 150, 0}, "Air stream up", false},
        {22, {200, 200, 0}, "Air stream down", false},
        {23, {250, 250, 0}, "Air stream left", false},
        {24, {0, 152, 100}, "Air stream right", false},
        {25, {126, 48, 159}, "Normal land under water", false},
        {26, {160, 0, 100}, "Brick wall under water", false},
        {27, {168, 168, 255}, "Exploding wall under water", false},
        {28, {168, 255, 156}, "Repair for team 1", false},
        {29, {124, 178, 117}, "Repair for team 2", false},
        {30, {86, 122, 81}, "Repair for team 3", false},
        {31, {58, 78, 55}, "Repair for team 4", false},
        {32, {0, 100, 0}, "GG land without objects", false},
        {33, {0, 255, 255}, "GG sign location", true},
    }};
    return palette;
}

Project LoadProject(const std::filesystem::path &path) {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Could not open project " + path.string());
    }
    json root;
    stream >> root;
    if (root.value("format", "") != "tou-level-project" || root.value("version", 0) != 1) {
        throw std::runtime_error("Unsupported TOU level project format/version");
    }

    Project project;
    project.project_path = std::filesystem::absolute(path).lexically_normal();
    const json assets = root.value("assets", json::object());
    project.visual_path = ResolveAsset(project.project_path, assets.value("visual", ""));
    project.attribute_path = ResolveAsset(project.project_path, assets.value("attributes", ""));
    project.parallax_path = ResolveAsset(project.project_path, assets.value("parallax", ""));

    project.config.mode = root.value("mode", "normal") == "gg"
        ? LevelMode::GroundGenerated : LevelMode::Normal;
    const json metadata = root.value("metadata", json::object());
    project.config.maker = metadata.value("maker", project.config.maker);
    project.config.email = metadata.value("email", project.config.email);
    const json rules = root.value("rules", json::object());
    project.config.parallax = rules.value("parallax", !project.parallax_path.empty());
    project.config.civilians = ByteValue(rules, "civilians", project.config.civilians);
    project.config.bombing = ByteValue(rules, "bombing", project.config.bombing);
    const std::vector<int> water = rules.value("waterRgb", std::vector<int>{80, 90, 170});
    if (water.size() != 3 || std::any_of(water.begin(), water.end(),
        [](int value) { return value < 0 || value > 255; })) {
        throw std::runtime_error("waterRgb must contain three byte values");
    }
    project.config.water = {
        static_cast<std::uint8_t>(water[0]),
        static_cast<std::uint8_t>(water[1]),
        static_cast<std::uint8_t>(water[2]),
    };
    project.config.disable_running_water = rules.value("disableRunningWater", false);
    project.config.gravity_tenths = ByteValue(rules, "gravityTenths", 10);
    project.config.resistance_tenths = ByteValue(rules, "resistanceTenths", 10);
    project.config.collision_damage_tenths = ByteValue(rules, "collisionDamageTenths", 10);
    project.config.bouncing_tenths = ByteValue(rules, "bouncingTenths", 10);
    project.config.ambient = ByteValue(rules, "ambient", 0);
    project.config.parallax_aftertouch = ByteValue(rules, "parallaxAftertouch", 0);

    const json gg = root.value("gg", json::object());
    project.config.gg_theme = gg.value("theme", project.config.gg_theme);
    project.config.gg_shape = gg.value("shape", project.config.gg_shape);
    project.config.repair_density = ByteValue(gg, "repairDensity", 20);
    project.config.stuff_density = ByteValue(gg, "stuffDensity", 20);
    project.config.sign_density = ByteValue(gg, "signDensity", 20);
    const std::uint64_t seed = gg.value("randomSeed", UINT64_C(12345));
    if (seed > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("randomSeed exceeds uint32 range");
    }
    project.config.random_seed = static_cast<std::uint32_t>(seed);
    for (const json &entry : gg.value("signTexts", json::array())) {
        project.config.sign_texts.push_back({
            entry.value("first", ""), entry.value("second", ""),
        });
    }
    return project;
}

void SaveProject(const Project &project, const std::filesystem::path &path) {
    json root;
    root["format"] = "tou-level-project";
    root["version"] = 1;
    root["mode"] = project.config.mode == LevelMode::GroundGenerated ? "gg" : "normal";
    root["assets"] = {
        {"visual", RelativeAsset(path, project.visual_path)},
        {"attributes", RelativeAsset(path, project.attribute_path)},
        {"parallax", RelativeAsset(path, project.parallax_path)},
    };
    root["metadata"] = {{"maker", project.config.maker}, {"email", project.config.email}};
    root["rules"] = {
        {"parallax", project.config.parallax},
        {"civilians", project.config.civilians},
        {"bombing", project.config.bombing},
        {"waterRgb", {project.config.water.r, project.config.water.g, project.config.water.b}},
        {"disableRunningWater", project.config.disable_running_water},
        {"gravityTenths", project.config.gravity_tenths},
        {"resistanceTenths", project.config.resistance_tenths},
        {"collisionDamageTenths", project.config.collision_damage_tenths},
        {"bouncingTenths", project.config.bouncing_tenths},
        {"ambient", project.config.ambient},
        {"parallaxAftertouch", project.config.parallax_aftertouch},
    };
    json sign_texts = json::array();
    for (const SignText &entry : project.config.sign_texts) {
        sign_texts.push_back({{"first", entry.first}, {"second", entry.second}});
    }
    root["gg"] = {
        {"theme", project.config.gg_theme},
        {"shape", project.config.gg_shape},
        {"repairDensity", project.config.repair_density},
        {"stuffDensity", project.config.stuff_density},
        {"signDensity", project.config.sign_density},
        {"randomSeed", project.config.random_seed},
        {"signTexts", sign_texts},
    };
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream stream(path, std::ios::trunc);
    if (!stream) {
        throw std::runtime_error("Could not save project " + path.string());
    }
    stream << std::setw(2) << root << '\n';
}

Project CreateNormalProject(const std::filesystem::path &project_path,
                            const std::filesystem::path &visual_path,
                            const std::filesystem::path &parallax_path) {
    const std::filesystem::path absolute_project =
        std::filesystem::absolute(project_path).lexically_normal();
    if (std::filesystem::exists(absolute_project)) {
        throw std::runtime_error("Project already exists: " + absolute_project.string());
    }
    const std::filesystem::path absolute_visual =
        std::filesystem::absolute(visual_path).lexically_normal();
    const std::vector<std::uint8_t> visual = ReadBytes(absolute_visual);
    const auto dimensions = JpegDimensions(visual);

    std::string base = absolute_project.filename().string();
    const std::string suffix = ".toulevel.json";
    if (base.size() >= suffix.size() &&
        base.compare(base.size() - suffix.size(), suffix.size(), suffix) == 0) {
        base.erase(base.size() - suffix.size());
    } else {
        base = absolute_project.stem().string();
    }
    const std::filesystem::path attributes =
        absolute_project.parent_path() / (base + "-attributes.tga");
    if (std::filesystem::exists(attributes)) {
        throw std::runtime_error("Attribute image already exists: " + attributes.string());
    }
    std::filesystem::create_directories(absolute_project.parent_path());

    Project project;
    project.project_path = absolute_project;
    project.visual_path = absolute_visual;
    project.attribute_path = attributes;
    if (!parallax_path.empty()) {
        project.parallax_path =
            std::filesystem::absolute(parallax_path).lexically_normal();
        static_cast<void>(JpegDimensions(ReadBytes(project.parallax_path)));
        project.config.parallax = true;
    }

    LevelData level;
    level.config = project.config;
    level.width = dimensions.first;
    level.height = dimensions.second;
    level.main_payload = visual;
    level.attributes.assign(static_cast<std::size_t>(level.width) * level.height, 0);
    SaveAttributeTga(level, attributes);
    SaveProject(project, absolute_project);
    return project;
}

Project CreateGroundGeneratedProject(const std::filesystem::path &project_path,
                                     std::uint16_t width,
                                     std::uint16_t height,
                                     const std::string &theme) {
    if (width == 0 || height == 0) {
        throw std::runtime_error("GG project dimensions must be non-zero");
    }
    if (theme.empty()) {
        throw std::runtime_error("GG project theme must not be empty");
    }
    const std::filesystem::path absolute_project =
        std::filesystem::absolute(project_path).lexically_normal();
    if (std::filesystem::exists(absolute_project)) {
        throw std::runtime_error("Project already exists: " + absolute_project.string());
    }
    std::string base = absolute_project.filename().string();
    const std::string suffix = ".toulevel.json";
    if (base.size() >= suffix.size() &&
        base.compare(base.size() - suffix.size(), suffix.size(), suffix) == 0) {
        base.erase(base.size() - suffix.size());
    } else {
        base = absolute_project.stem().string();
    }
    const std::filesystem::path attributes =
        absolute_project.parent_path() / (base + "-attributes.tga");
    if (std::filesystem::exists(attributes)) {
        throw std::runtime_error("Attribute image already exists: " + attributes.string());
    }
    std::filesystem::create_directories(absolute_project.parent_path());

    Project project;
    project.project_path = absolute_project;
    project.attribute_path = attributes;
    project.config.mode = LevelMode::GroundGenerated;
    project.config.gg_theme = theme;

    LevelData level;
    level.config = project.config;
    level.width = width;
    level.height = height;
    level.attributes.assign(static_cast<std::size_t>(width) * height, 0);
    SaveAttributeTga(level, attributes);
    SaveProject(project, absolute_project);
    return project;
}

Project ImportLevelProject(const std::filesystem::path &level_path,
                           const std::filesystem::path &project_path) {
    const std::filesystem::path absolute_project =
        std::filesystem::absolute(project_path).lexically_normal();
    if (std::filesystem::exists(absolute_project)) {
        throw std::runtime_error("Project already exists: " + absolute_project.string());
    }
    const LevelData level = ReadLevel(level_path);
    std::string base = absolute_project.filename().string();
    const std::string suffix = ".toulevel.json";
    if (base.size() >= suffix.size() &&
        base.compare(base.size() - suffix.size(), suffix.size(), suffix) == 0) {
        base.erase(base.size() - suffix.size());
    } else {
        base = absolute_project.stem().string();
    }
    const std::filesystem::path directory = absolute_project.parent_path();
    const std::filesystem::path attributes = directory / (base + "-attributes.tga");
    const std::filesystem::path visual = directory / (base + "-visual.jpg");
    const std::filesystem::path parallax = directory / (base + "-parallax.jpg");
    const auto ensure_new = [](const std::filesystem::path &path) {
        if (!path.empty() && std::filesystem::exists(path)) {
            throw std::runtime_error("Imported asset already exists: " + path.string());
        }
    };
    ensure_new(attributes);
    if (level.config.mode == LevelMode::Normal) ensure_new(visual);
    if (!level.parallax_payload.empty()) ensure_new(parallax);
    std::filesystem::create_directories(directory);

    Project project;
    project.project_path = absolute_project;
    project.attribute_path = attributes;
    project.config = level.config;
    if (level.config.mode == LevelMode::Normal) {
        WriteBytes(visual, level.main_payload);
        project.visual_path = visual;
    }
    if (!level.parallax_payload.empty()) {
        WriteBytes(parallax, level.parallax_payload);
        project.parallax_path = parallax;
    }
    SaveAttributeTga(level, attributes);
    SaveProject(project, absolute_project);
    return project;
}

LevelData CompileProject(const Project &project) {
    const std::vector<ValidationMessage> messages = ValidateProject(project);
    const auto error = std::find_if(messages.begin(), messages.end(),
        [](const ValidationMessage &message) {
            return message.severity == ValidationMessage::Severity::Error;
        });
    if (error != messages.end()) {
        throw std::runtime_error(error->text);
    }

    TgaImage attribute = LoadTga(project.attribute_path);
    LevelData level;
    level.config = project.config;
    level.width = attribute.width;
    level.height = attribute.height;
    level.attributes.resize(attribute.pixels.size());
    for (std::uint16_t y = 0; y < attribute.height; ++y) {
        for (std::uint16_t x = 0; x < attribute.width; ++x) {
            const std::size_t offset = static_cast<std::size_t>(y) * attribute.width + x;
            Placement placement;
            std::uint8_t replacement = 0;
            if (ExtractPlacement(attribute.pixels[offset], x, y, placement, replacement)) {
                if (level.placements.size() >= kMaxPlacements) {
                    throw std::runtime_error("Attribute map contains more than 1024 placements");
                }
                level.placements.push_back(placement);
                level.attributes[offset] = replacement;
                continue;
            }
            const int index = PaletteIndex(attribute.pixels[offset]);
            if (index < 0) {
                std::ostringstream text;
                text << "Illegal attribute color at (" << x << ", " << y << "): rgb("
                     << static_cast<int>(attribute.pixels[offset].r) << ", "
                     << static_cast<int>(attribute.pixels[offset].g) << ", "
                     << static_cast<int>(attribute.pixels[offset].b) << ')';
                throw std::runtime_error(text.str());
            }
            level.attributes[offset] = static_cast<std::uint8_t>(index);
        }
    }

    if (project.config.mode == LevelMode::Normal) {
        level.main_payload = ReadBytes(project.visual_path);
        const auto dimensions = JpegDimensions(level.main_payload);
        if (dimensions.first != level.width || dimensions.second != level.height) {
            throw std::runtime_error("Visual JPEG and attribute TGA dimensions differ");
        }
    }
    if (project.config.parallax) {
        level.parallax_payload = ReadBytes(project.parallax_path);
        const auto dimensions = JpegDimensions(level.parallax_payload);
        if (dimensions.first > level.width || dimensions.second > level.height) {
            throw std::runtime_error(
                "Parallax JPEG cannot be larger than the level in either dimension");
        }
    }
    return level;
}

LevelData ReadLevel(const std::filesystem::path &path) {
    const std::vector<std::uint8_t> bytes = ReadBytes(path);
    if (bytes.size() < kMainOffset ||
        std::memcmp(bytes.data(), kMagic, sizeof(kMagic) - 1U) != 0 ||
        bytes[0x13] != 0x0D || bytes[0x14] != 0x0A || bytes[0x15] != 0x1A) {
        throw std::runtime_error("Not a TOU v1.4 level");
    }
    const std::size_t main_offset = ReadU32(bytes, 0x16);
    const std::size_t parallax_offset = ReadU32(bytes, 0x1A);
    const std::size_t placement_offset = ReadU32(bytes, 0x1E);
    if (main_offset != kMainOffset || main_offset > parallax_offset ||
        parallax_offset > placement_offset || placement_offset + 4 > bytes.size()) {
        throw std::runtime_error("Level contains invalid section offsets");
    }

    LevelData level;
    level.config = DecodeConfig(bytes, kHeaderSize);
    if (level.config.mode == LevelMode::GroundGenerated) {
        if (parallax_offset - main_offset != 4) {
            throw std::runtime_error("GG level dimensions payload is malformed");
        }
        level.width = ReadU16(bytes, main_offset);
        level.height = ReadU16(bytes, main_offset + 2);
    } else {
        level.main_payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(main_offset),
                                  bytes.begin() + static_cast<std::ptrdiff_t>(parallax_offset));
        const auto dimensions = JpegDimensions(level.main_payload);
        level.width = dimensions.first;
        level.height = dimensions.second;
    }
    level.parallax_payload.assign(
        bytes.begin() + static_cast<std::ptrdiff_t>(parallax_offset),
        bytes.begin() + static_cast<std::ptrdiff_t>(placement_offset));

    const std::size_t count = ReadU32(bytes, placement_offset);
    if (count > kMaxPlacements || placement_offset + 4 + count * kPlacementSize > bytes.size()) {
        throw std::runtime_error("Level placement section is malformed");
    }
    std::size_t position = placement_offset + 4;
    for (std::size_t index = 0; index < count; ++index) {
        Placement placement;
        placement.x = static_cast<std::int32_t>(ReadU32(bytes, position));
        placement.y = static_cast<std::int32_t>(ReadU32(bytes, position + 4));
        placement.type = static_cast<PlacementType>(bytes[position + 8]);
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(position + 9), 5,
                    placement.parameters.begin());
        level.placements.push_back(placement);
        position += kPlacementSize;
    }

    const std::size_t expected =
        static_cast<std::size_t>(level.width) * level.height;
    level.attributes.reserve(expected);
    while (position + 2 <= bytes.size()) {
        const std::uint8_t first = bytes[position++];
        const std::uint8_t second = bytes[position++];
        if (first == 0xFF) {
            break;
        }
        const std::uint8_t palette_index = first >> 2U;
        if (palette_index > 33) {
            throw std::runtime_error("Level RLE contains an invalid palette index");
        }
        const std::size_t run = (first & 3U) + static_cast<std::size_t>(second) * 4U;
        if (run == 0 || level.attributes.size() + run > expected) {
            throw std::runtime_error("Level RLE run exceeds attribute dimensions");
        }
        level.attributes.insert(level.attributes.end(), run, palette_index);
    }
    if (level.attributes.size() != expected) {
        throw std::runtime_error("Level RLE does not fill its declared dimensions");
    }
    return level;
}

void WriteLevel(const LevelData &level, const std::filesystem::path &path) {
    const std::vector<ValidationMessage> messages = ValidateLevel(level);
    const auto error = std::find_if(messages.begin(), messages.end(),
        [](const ValidationMessage &message) {
            return message.severity == ValidationMessage::Severity::Error;
        });
    if (error != messages.end()) {
        throw std::runtime_error(error->text);
    }

    std::vector<std::uint8_t> bytes(kHeaderSize, 0);
    std::copy_n(reinterpret_cast<const std::uint8_t *>(kMagic), sizeof(kMagic) - 1U,
                bytes.begin());
    bytes[0x13] = 0x0D;
    bytes[0x14] = 0x0A;
    bytes[0x15] = 0x1A;
    const std::vector<std::uint8_t> config = EncodeConfig(level.config);
    bytes.insert(bytes.end(), config.begin(), config.end());

    StoreU32(bytes, 0x16, static_cast<std::uint32_t>(bytes.size()));
    if (level.config.mode == LevelMode::GroundGenerated) {
        AppendU16(bytes, level.width);
        AppendU16(bytes, level.height);
    } else {
        bytes.insert(bytes.end(), level.main_payload.begin(), level.main_payload.end());
    }
    StoreU32(bytes, 0x1A, static_cast<std::uint32_t>(bytes.size()));
    bytes.insert(bytes.end(), level.parallax_payload.begin(), level.parallax_payload.end());
    StoreU32(bytes, 0x1E, static_cast<std::uint32_t>(bytes.size()));

    AppendU32(bytes, static_cast<std::uint32_t>(level.placements.size()));
    for (const Placement &placement : level.placements) {
        AppendU32(bytes, static_cast<std::uint32_t>(placement.x));
        AppendU32(bytes, static_cast<std::uint32_t>(placement.y));
        bytes.push_back(static_cast<std::uint8_t>(placement.type));
        bytes.insert(bytes.end(), placement.parameters.begin(), placement.parameters.end());
        bytes.insert(bytes.end(), 6, 0);
    }
    const std::vector<std::uint8_t> rle = EncodeRle(level.attributes);
    bytes.insert(bytes.end(), rle.begin(), rle.end());
    WriteBytes(path, bytes);
}

std::vector<ValidationMessage> ValidateProject(const Project &project) {
    std::vector<ValidationMessage> messages;
    const auto error = [&messages](const std::string &text) {
        messages.push_back({ValidationMessage::Severity::Error, text});
    };
    if (project.attribute_path.empty() || !std::filesystem::is_regular_file(project.attribute_path)) {
        error("Attribute TGA does not exist: " + project.attribute_path.string());
    }
    if (project.config.mode == LevelMode::Normal &&
        (project.visual_path.empty() || !std::filesystem::is_regular_file(project.visual_path))) {
        error("Visual JPEG does not exist: " + project.visual_path.string());
    }
    if (project.config.parallax &&
        (project.parallax_path.empty() || !std::filesystem::is_regular_file(project.parallax_path))) {
        error("Parallax is enabled but its JPEG does not exist");
    }
    if (!project.config.parallax && !project.parallax_path.empty()) {
        messages.push_back({ValidationMessage::Severity::Warning,
                            "Parallax asset is set but parallax is disabled"});
    }
    if (project.config.mode == LevelMode::GroundGenerated && project.config.parallax) {
        error("Custom parallax in GG levels is intentionally unsupported");
    }
    ValidateConfigFields(project.config, messages);
    return messages;
}

std::vector<ValidationMessage> ValidateLevel(const LevelData &level) {
    std::vector<ValidationMessage> messages;
    const auto error = [&messages](const std::string &text) {
        messages.push_back({ValidationMessage::Severity::Error, text});
    };
    const auto warning = [&messages](const std::string &text) {
        messages.push_back({ValidationMessage::Severity::Warning, text});
    };
    ValidateConfigFields(level.config, messages);
    if (level.width == 0 || level.height == 0) {
        error("Level dimensions must be non-zero");
    }
    const std::size_t expected = static_cast<std::size_t>(level.width) * level.height;
    if (level.attributes.size() != expected) {
        error("Attribute count does not match level dimensions");
    }
    if (level.placements.size() > kMaxPlacements) {
        error("Level contains more than 1024 placements");
    }
    if (level.config.mode == LevelMode::Normal && level.main_payload.empty()) {
        error("Normal level is missing its visual JPEG payload");
    }
    if (level.config.parallax != !level.parallax_payload.empty()) {
        error("Parallax config and payload disagree");
    }
    if (level.config.mode == LevelMode::GroundGenerated && level.config.parallax) {
        error("Custom parallax in GG levels is intentionally unsupported");
    }
    std::size_t wall_segments = 0;
    std::vector<std::pair<std::int32_t, std::int32_t>> occupied;
    std::array<bool, 64> teleport_numbers{};
    for (const Placement &placement : level.placements) {
        if (placement.x < 0 || placement.y < 0 ||
            placement.x >= level.width || placement.y >= level.height) {
            error("Placement lies outside the attribute map");
            continue;
        }
        if (std::find(occupied.begin(), occupied.end(),
                      std::make_pair(placement.x, placement.y)) != occupied.end()) {
            error("Multiple placements overlap at (" +
                  std::to_string(placement.x) + ", " +
                  std::to_string(placement.y) + ")");
        } else {
            occupied.emplace_back(placement.x, placement.y);
        }
        const auto &p = placement.parameters;
        switch (placement.type) {
        case PlacementType::Turret:
            if (p[0] > 6 || p[1] > 15 || p[2] > 3 || p[3] > 32) {
                error("Turret placement has an out-of-range style, armor, team, or direction");
            }
            break;
        case PlacementType::Gate:
            if (p[0] > 4 || p[1] > 1 || p[2] > 3 || p[3] > 3 || p[4] > 1) {
                error("Gate placement has an out-of-range style, pair, team, facing, or mirror value");
            }
            wall_segments += p[1] == 0 ? 1U : 2U;
            break;
        case PlacementType::Object:
            if (p[0] > 4 ||
                (p[0] == 0 && (p[1] > 7 || p[2] > 3 || p[3] > 3)) ||
                (p[0] == 2 && p[1] > 7) ||
                (p[0] == 3 && p[1] > 3)) {
                error("Object placement has an out-of-range type-specific value");
            }
            break;
        case PlacementType::StartingPlace:
            if (p[0] > 3) error("Starting place has an invalid team");
            break;
        case PlacementType::Teleport:
            if (p[0] > 63 || p[1] > 3) {
                error("Teleport has an invalid number or team");
            } else if (teleport_numbers[p[0]]) {
                error("Teleport number " + std::to_string(p[0] + 1U) +
                      " is used more than once");
            } else {
                teleport_numbers[p[0]] = true;
            }
            break;
        default:
            error("Placement has an unknown runtime type");
            break;
        }
    }
    if (wall_segments > 16) {
        error("Gates create " + std::to_string(wall_segments) +
              " wall segments, but the runtime limit is 16");
    }
    for (const Placement &placement : level.placements) {
        if (placement.type == PlacementType::Teleport &&
            (placement.parameters[2] > 63 ||
             !teleport_numbers[placement.parameters[2]])) {
            error("Teleport " + std::to_string(placement.parameters[0] + 1U) +
                  " targets missing teleport " +
                  std::to_string(placement.parameters[2] + 1U));
        }
    }
    if (std::none_of(level.placements.begin(), level.placements.end(),
                     [](const Placement &placement) {
                         return placement.type == PlacementType::StartingPlace;
                     })) {
        warning("Level contains no authored starting places");
    }
    return messages;
}

Comparison CompareLevels(const std::filesystem::path &left,
                         const std::filesystem::path &right) {
    const std::vector<std::uint8_t> a = ReadBytes(left);
    const std::vector<std::uint8_t> b = ReadBytes(right);
    Comparison comparison;
    comparison.left_size = a.size();
    comparison.right_size = b.size();
    const std::size_t common = std::min(a.size(), b.size());
    for (std::size_t offset = 0; offset < common; ++offset) {
        if (a[offset] == b[offset]) {
            continue;
        }
        ++comparison.raw_differences;
        if (comparison.first_raw_offsets.size() < 32) {
            comparison.first_raw_offsets.push_back(offset);
        }
        if (!IsIgnoredPlacementPadding(a, offset) &&
            !IsIgnoredPlacementPadding(b, offset)) {
            ++comparison.meaningful_differences;
            if (comparison.first_meaningful_offsets.size() < 32) {
                comparison.first_meaningful_offsets.push_back(offset);
            }
        }
    }
    const std::size_t extra = a.size() > b.size() ? a.size() - b.size() : b.size() - a.size();
    comparison.raw_differences += extra;
    comparison.meaningful_differences += extra;
    return comparison;
}

void SaveAttributeTga(const LevelData &level, const std::filesystem::path &path) {
    const std::size_t expected = static_cast<std::size_t>(level.width) * level.height;
    if (level.attributes.size() != expected) {
        throw std::runtime_error("Cannot save a TGA with mismatched dimensions");
    }
    std::vector<Rgb> pixels(expected);
    const auto &palette = Palette();
    for (std::size_t index = 0; index < expected; ++index) {
        if (level.attributes[index] > 33) {
            throw std::runtime_error("Attribute palette index is out of range");
        }
        pixels[index] = palette[level.attributes[index]].rgb;
    }
    for (const Placement &placement : level.placements) {
        if (placement.x < 0 || placement.y < 0 ||
            placement.x >= level.width || placement.y >= level.height) {
            throw std::runtime_error("Cannot save an out-of-bounds placement");
        }
        pixels[static_cast<std::size_t>(placement.y) * level.width + placement.x] =
            EncodePlacementMarker(placement);
    }

    std::vector<std::uint8_t> bytes(18, 0);
    bytes[2] = 2;
    bytes[12] = static_cast<std::uint8_t>(level.width & 0xFFU);
    bytes[13] = static_cast<std::uint8_t>(level.width >> 8U);
    bytes[14] = static_cast<std::uint8_t>(level.height & 0xFFU);
    bytes[15] = static_cast<std::uint8_t>(level.height >> 8U);
    bytes[16] = 24;
    bytes[17] = 0x20;
    bytes.reserve(18 + expected * 3U);
    for (const Rgb &pixel : pixels) {
        bytes.push_back(pixel.b);
        bytes.push_back(pixel.g);
        bytes.push_back(pixel.r);
    }
    WriteBytes(path, bytes);
}

std::string ToJson(const LevelData &level) {
    json root;
    root["mode"] = level.config.mode == LevelMode::GroundGenerated ? "gg" : "normal";
    root["dimensions"] = {level.width, level.height};
    root["maker"] = level.config.maker;
    root["email"] = level.config.email;
    root["mainPayloadBytes"] = level.main_payload.size();
    root["parallaxBytes"] = level.parallax_payload.size();
    root["placements"] = level.placements.size();
    root["attributePixels"] = level.attributes.size();
    std::array<std::size_t, 34> counts{};
    for (const std::uint8_t index : level.attributes) {
        if (index <= 33) {
            ++counts[index];
        }
    }
    json palette_counts = json::object();
    for (const PaletteEntry &entry : Palette()) {
        if (counts[entry.index] != 0) {
            palette_counts[entry.name] = counts[entry.index];
        }
    }
    root["paletteCounts"] = palette_counts;
    return root.dump(2);
}

}  // namespace tou::level
