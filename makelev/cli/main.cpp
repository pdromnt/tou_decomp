#include "tou_level/level.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using tou::level::ValidationMessage;

void Usage() {
    std::cout
        << "TOU level compiler\n\n"
        << "Usage:\n"
        << "  tou-level inspect <level.lev>\n"
        << "  tou-level validate <project.toulevel.json>\n"
        << "  tou-level build <project.toulevel.json> <output.lev>\n"
        << "  tou-level compare <left.lev> <right.lev>\n";
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
