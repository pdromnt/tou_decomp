#include "tou_level/level.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#ifndef TOU_SOURCE_DIR
#error TOU_SOURCE_DIR must be defined
#endif

namespace {

void Require(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    try {
        const std::filesystem::path root = TOU_SOURCE_DIR;
        const std::filesystem::path project_path =
            root / "makelev/fixtures/jungle/jungle.toulevel.json";
        const std::filesystem::path shipped = root / "levels/jungle.lev";
        const std::filesystem::path temporary =
            std::filesystem::temp_directory_path() / "tou-level-tests";
        std::filesystem::create_directories(temporary);
        const std::filesystem::path generated = temporary / "jungle.lev";
        const std::filesystem::path attribute_roundtrip = temporary / "jungle.tga";

        const tou::level::Project project = tou::level::LoadProject(project_path);
        const tou::level::LevelData compiled = tou::level::CompileProject(project);
        Require(compiled.width == 1000 && compiled.height == 1091,
                "Jungle dimensions differ");
        Require(compiled.placements.size() == 1, "Jungle placement count differs");
        Require(compiled.attributes.size() == 1091000, "Jungle attribute count differs");

        tou::level::WriteLevel(compiled, generated);
        const tou::level::Comparison comparison =
            tou::level::CompareLevels(generated, shipped);
        Require(comparison.meaningful_differences == 0,
                "Generated Jungle differs meaningfully from shipped Jungle");
        Require(comparison.raw_differences == 0,
                "Deterministic writer should exactly match shipped Jungle");

        const tou::level::LevelData decoded = tou::level::ReadLevel(generated);
        Require(decoded.attributes == compiled.attributes,
                "Attribute RLE round trip differs");
        Require(decoded.placements.size() == compiled.placements.size(),
                "Placement round trip differs");

        tou::level::SaveAttributeTga(decoded, attribute_roundtrip);
        tou::level::Project roundtrip_project = project;
        roundtrip_project.attribute_path = attribute_roundtrip;
        const tou::level::LevelData recompiled =
            tou::level::CompileProject(roundtrip_project);
        Require(recompiled.attributes == compiled.attributes,
                "TGA terrain round trip differs");
        Require(recompiled.placements.size() == compiled.placements.size(),
                "TGA placement round trip differs");

        std::filesystem::remove(generated);
        std::filesystem::remove(attribute_roundtrip);
        std::filesystem::remove(temporary);
        std::cout << "TOU level golden tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "TOU level test failure: " << error.what() << '\n';
        return 1;
    }
}
