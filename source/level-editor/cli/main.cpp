#include "tou_level/level.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using tou::level::ValidationMessage;

void Usage() {
    std::cout
        << "TOU level compiler\n\n"
        << "Usage:\n"
        << "  tou-level-compiler new <project.toulevel.json> <visual.jpg> [parallax.jpg]\n"
        << "  tou-level-compiler new-gg <project.toulevel.json> <width> <height> <theme>\n"
        << "  tou-level-compiler new-theme <ggstuff-directory> <theme-name>\n"
        << "  tou-level-compiler import <level.lev> <project.toulevel.json>\n"
        << "  tou-level-compiler inspect <level.lev>\n"
        << "  tou-level-compiler validate <project.toulevel.json>\n"
        << "  tou-level-compiler build <project.toulevel.json> <output.lev>\n"
        << "  tou-level-compiler compare <left.lev> <right.lev>\n";
}

int PrintMessages(const std::vector<ValidationMessage> &messages) {
    bool failed = false;
    for (const ValidationMessage &message : messages) {
        const bool error = message.severity == ValidationMessage::Severity::Error;
        std::cout << (error ? "error: " : "warning: ") << message.text << '\n';
        failed = failed || error;
    }
    if (messages.empty()) {
        std::cout << "valid\n";
    }
    return failed ? 1 : 0;
}

}  // namespace

int main(int argc, char **argv) {
    try {
        if (argc < 3) {
            Usage();
            return 2;
        }
        const std::string command = argv[1];
        if (command == "new" && (argc == 4 || argc == 5)) {
            const tou::level::Project project = tou::level::CreateNormalProject(
                argv[2], argv[3], argc == 5 ? std::filesystem::path(argv[4])
                                             : std::filesystem::path{});
            const tou::level::LevelData level = tou::level::CompileProject(project);
            std::cout << "created " << project.project_path.string() << " ("
                      << level.width << 'x' << level.height << ")\n";
            return 0;
        }
        if (command == "new-gg" && argc == 6) {
            const int width = std::stoi(argv[3]);
            const int height = std::stoi(argv[4]);
            if (width < 1 || width > 65535 || height < 1 || height > 65535) {
                throw std::runtime_error("GG dimensions must be between 1 and 65535");
            }
            const tou::level::Project project =
                tou::level::CreateGroundGeneratedProject(
                    argv[2], static_cast<std::uint16_t>(width),
                    static_cast<std::uint16_t>(height), argv[5]);
            std::cout << "created GG project " << project.project_path.string()
                      << " (" << width << 'x' << height << ", theme "
                      << project.config.gg_theme << ")\n";
            return 0;
        }
        if (command == "new-theme" && argc == 4) {
            const std::string name = argv[3];
            if (name.empty() || name == "." || name == ".." ||
                name.find('/') != std::string::npos ||
                name.find('\\') != std::string::npos) {
                throw std::runtime_error("Theme name must be one directory name");
            }
            const std::filesystem::path directory =
                std::filesystem::path(argv[2]) / name;
            if (std::filesystem::exists(directory)) {
                throw std::runtime_error("Theme directory already exists: " +
                                         directory.string());
            }
            std::filesystem::create_directories(directory);
            std::ofstream info(directory / "info.txt", std::ios::binary);
            info << "/MAKER\nAnonymous\n/EMAIL\n\n/TEXDARK\n80\n/PARDARK\n96\n"
                    "/TEXATTR\n1\n/GRASSATTR\n1\n/PICATTR\n1\n/SIGNATTR\n2\n"
                    "/SIGNCOLOR\n1\n/SIGNTEXT\n10,22\n/FIXEDSIZE\n0,0\n"
                    "/WATERLEVEL\n0\n/WATERC\n50,50,200\n/BEACHSTYLE\nno\n";
            if (!info) throw std::runtime_error("Could not create theme info.txt");
            std::cout << "created GG theme scaffold " << directory.string()
                      << " (add s1.tga and t1.jpg)\n";
            return 0;
        }
        if (command == "import" && argc == 4) {
            const tou::level::Project project =
                tou::level::ImportLevelProject(argv[2], argv[3]);
            const tou::level::LevelData level = tou::level::CompileProject(project);
            std::cout << "imported " << std::filesystem::path(argv[2]).string()
                      << " into " << project.project_path.string() << " ("
                      << level.width << 'x' << level.height << ")\n";
            return 0;
        }
        if (command == "inspect" && argc == 3) {
            const tou::level::LevelData level = tou::level::ReadLevel(argv[2]);
            std::cout << tou::level::ToJson(level) << '\n';
            return 0;
        }
        if (command == "validate" && argc == 3) {
            const tou::level::Project project = tou::level::LoadProject(argv[2]);
            std::vector<ValidationMessage> messages = tou::level::ValidateProject(project);
            if (PrintMessages(messages) != 0) {
                return 1;
            }
            const tou::level::LevelData level = tou::level::CompileProject(project);
            return PrintMessages(tou::level::ValidateLevel(level));
        }
        if (command == "build" && argc == 4) {
            const tou::level::Project project = tou::level::LoadProject(argv[2]);
            const tou::level::LevelData level = tou::level::CompileProject(project);
            tou::level::WriteLevel(level, argv[3]);
            std::cout << "built " << std::filesystem::path(argv[3]).string()
                      << " (" << level.width << 'x' << level.height << ", "
                      << level.placements.size() << " placements)\n";
            return 0;
        }
        if (command == "compare" && argc == 4) {
            const tou::level::Comparison comparison =
                tou::level::CompareLevels(argv[2], argv[3]);
            std::cout << "left bytes: " << comparison.left_size << '\n'
                      << "right bytes: " << comparison.right_size << '\n'
                      << "raw differences: " << comparison.raw_differences << '\n'
                      << "meaningful differences: "
                      << comparison.meaningful_differences << '\n';
            if (!comparison.first_meaningful_offsets.empty()) {
                std::cout << "first meaningful offsets:";
                for (const std::size_t offset : comparison.first_meaningful_offsets) {
                    std::cout << " 0x" << std::hex << offset << std::dec;
                }
                std::cout << '\n';
            }
            return comparison.meaningful_differences == 0 ? 0 : 1;
        }
        Usage();
        return 2;
    } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
