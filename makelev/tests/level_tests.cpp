#include "tou_level/level.hpp"

#include <algorithm>
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
        const std::filesystem::path blank_project = temporary / "blank.toulevel.json";
        const std::filesystem::path blank_attributes = temporary / "blank-attributes.tga";
        const std::filesystem::path gg_project = temporary / "blank-gg.toulevel.json";
        const std::filesystem::path gg_attributes = temporary / "blank-gg-attributes.tga";
        const std::filesystem::path gg_level = temporary / "blank-gg.lev";
        const std::filesystem::path imported_directory = temporary / "imported";
        const std::filesystem::path imported_project =
            imported_directory / "jungle.toulevel.json";
        const std::filesystem::path imported_level = temporary / "imported-jungle.lev";
        std::filesystem::remove(blank_project);
        std::filesystem::remove(blank_attributes);
        std::filesystem::remove(gg_project);
        std::filesystem::remove(gg_attributes);
        std::filesystem::remove(gg_level);
        std::filesystem::remove_all(imported_directory);
        std::filesystem::remove(imported_level);

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

        const tou::level::Project imported =
            tou::level::ImportLevelProject(shipped, imported_project);
        const tou::level::LevelData imported_data =
            tou::level::CompileProject(imported);
        tou::level::WriteLevel(imported_data, imported_level);
        const tou::level::Comparison imported_comparison =
            tou::level::CompareLevels(imported_level, shipped);
        Require(imported_comparison.raw_differences == 0,
                "Imported level does not export identically");

        tou::level::LevelData no_parallax = compiled;
        no_parallax.config.parallax = false;
        no_parallax.parallax_payload.clear();
        const std::filesystem::path no_parallax_file = temporary / "no-parallax.lev";
        tou::level::WriteLevel(no_parallax, no_parallax_file);
        const tou::level::LevelData no_parallax_roundtrip =
            tou::level::ReadLevel(no_parallax_file);
        Require(!no_parallax_roundtrip.config.parallax &&
                    no_parallax_roundtrip.parallax_payload.empty(),
                "Disabled parallax round trip differs");

        tou::level::SaveAttributeTga(decoded, attribute_roundtrip);
        tou::level::Project roundtrip_project = project;
        roundtrip_project.attribute_path = attribute_roundtrip;
        const tou::level::LevelData recompiled =
            tou::level::CompileProject(roundtrip_project);
        Require(recompiled.attributes == compiled.attributes,
                "TGA terrain round trip differs");
        Require(recompiled.placements.size() == compiled.placements.size(),
                "TGA placement round trip differs");

        tou::level::LevelData placement_fixture = compiled;
        placement_fixture.placements = {
            {10, 10, tou::level::PlacementType::Turret, {5, 7, 2, 31, 0}},
            {11, 10, tou::level::PlacementType::Gate, {4, 1, 2, 3, 1}},
            {12, 10, tou::level::PlacementType::Object, {0, 6, 2, 3, 0}},
            {13, 10, tou::level::PlacementType::Object, {1, 0, 0, 0, 0}},
            {14, 10, tou::level::PlacementType::Object, {2, 4, 0, 0, 0}},
            {15, 10, tou::level::PlacementType::Object, {3, 2, 0, 0, 0}},
            {16, 10, tou::level::PlacementType::Object, {4, 0, 0, 0, 0}},
            {17, 10, tou::level::PlacementType::StartingPlace, {2, 0, 0, 0, 0}},
            {18, 10, tou::level::PlacementType::Teleport, {12, 2, 13, 0, 0}},
            {19, 10, tou::level::PlacementType::Teleport, {13, 1, 12, 0, 0}},
        };
        for (const tou::level::Placement &placement : placement_fixture.placements) {
            const std::size_t pixel = static_cast<std::size_t>(placement.y) *
                placement_fixture.width + placement.x;
            placement_fixture.attributes[pixel] =
                placement.type == tou::level::PlacementType::Object &&
                placement.parameters[0] <= 1 ? 1 : 0;
        }
        tou::level::SaveAttributeTga(placement_fixture, attribute_roundtrip);
        const tou::level::LevelData placement_roundtrip =
            tou::level::CompileProject(roundtrip_project);
        Require(placement_roundtrip.placements.size() ==
                    placement_fixture.placements.size(),
                "Placement-family count round trip differs");
        for (std::size_t index = 0; index < placement_fixture.placements.size(); ++index) {
            const tou::level::Placement &expected = placement_fixture.placements[index];
            const tou::level::Placement &actual = placement_roundtrip.placements[index];
            Require(actual.x == expected.x && actual.y == expected.y &&
                        actual.type == expected.type &&
                        actual.parameters == expected.parameters,
                    "Placement-family value round trip differs");
        }
        const auto valid_messages = tou::level::ValidateLevel(placement_fixture);
        Require(std::none_of(valid_messages.begin(), valid_messages.end(),
                             [](const tou::level::ValidationMessage &message) {
                                 return message.severity ==
                                     tou::level::ValidationMessage::Severity::Error;
                             }),
                "Valid placement families failed semantic validation");

        tou::level::LevelData invalid = placement_fixture;
        invalid.placements[1].parameters[0] = 5;
        invalid.placements[2].x = invalid.placements[1].x;
        invalid.placements.back().parameters[2] = 63;
        const auto invalid_messages = tou::level::ValidateLevel(invalid);
        const auto error_count = std::count_if(
            invalid_messages.begin(), invalid_messages.end(),
            [](const tou::level::ValidationMessage &message) {
                return message.severity ==
                    tou::level::ValidationMessage::Severity::Error;
            });
        Require(error_count >= 3,
                "Invalid overlap, gate profile, and teleport target were not rejected");

        const tou::level::Project created = tou::level::CreateNormalProject(
            blank_project, project.visual_path);
        const tou::level::LevelData blank = tou::level::CompileProject(created);
        Require(blank.width == compiled.width && blank.height == compiled.height,
                "New project dimensions differ from its visual JPEG");
        Require(std::all_of(blank.attributes.begin(), blank.attributes.end(),
                            [](std::uint8_t value) { return value == 0; }),
                "New project attribute image is not blank");

        const tou::level::Project created_gg =
            tou::level::CreateGroundGeneratedProject(
                gg_project, 320, 240, "the earth");
        const tou::level::LevelData blank_gg = tou::level::CompileProject(created_gg);
        Require(blank_gg.config.mode == tou::level::LevelMode::GroundGenerated &&
                    blank_gg.width == 320 && blank_gg.height == 240 &&
                    blank_gg.main_payload.empty(),
                "New GG project structure differs");
        tou::level::Project authored_gg = created_gg;
        authored_gg.config.maker = "Editor test";
        authored_gg.config.email = "editor@example.invalid";
        authored_gg.config.sign_texts = {{"FIRST", "SECOND"}, {"THIRD", "FOURTH"}};
        tou::level::SaveProject(authored_gg, authored_gg.project_path);
        const tou::level::Project reopened_gg =
            tou::level::LoadProject(authored_gg.project_path);
        Require(reopened_gg.config.maker == authored_gg.config.maker &&
                    reopened_gg.config.email == authored_gg.config.email &&
                    reopened_gg.config.sign_texts.size() == 2 &&
                    reopened_gg.config.sign_texts[1].second == "FOURTH",
                "GG metadata/sign project save and reopen differs");
        tou::level::LevelData invalid_gg_config = blank_gg;
        invalid_gg_config.config.maker.assign(128, 'X');
        invalid_gg_config.config.sign_texts.resize(17);
        invalid_gg_config.config.parallax = true;
        invalid_gg_config.parallax_payload = {0xFF, 0xD8};
        const auto invalid_config_messages =
            tou::level::ValidateLevel(invalid_gg_config);
        const auto invalid_config_errors = std::count_if(
            invalid_config_messages.begin(), invalid_config_messages.end(),
            [](const tou::level::ValidationMessage &message) {
                return message.severity ==
                    tou::level::ValidationMessage::Severity::Error;
            });
        Require(invalid_config_errors >= 3,
                "Legacy string/sign limits and GG parallax were not rejected");
        tou::level::WriteLevel(blank_gg, gg_level);
        const tou::level::LevelData decoded_gg = tou::level::ReadLevel(gg_level);
        Require(decoded_gg.config.mode == tou::level::LevelMode::GroundGenerated &&
                    decoded_gg.width == 320 && decoded_gg.height == 240,
                "New GG level round trip differs");

        std::filesystem::remove(generated);
        std::filesystem::remove(attribute_roundtrip);
        std::filesystem::remove(no_parallax_file);
        std::filesystem::remove(blank_project);
        std::filesystem::remove(blank_attributes);
        std::filesystem::remove(gg_project);
        std::filesystem::remove(gg_attributes);
        std::filesystem::remove(gg_level);
        std::filesystem::remove_all(imported_directory);
        std::filesystem::remove(imported_level);
        std::filesystem::remove(temporary);
        std::cout << "TOU level golden tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "TOU level test failure: " << error.what() << '\n';
        return 1;
    }
}
