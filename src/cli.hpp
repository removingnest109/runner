#pragma once
#include <filesystem>
#include <optional>
#include <string>

struct CliOptions {
    std::optional<std::filesystem::path> config_path;  // --config / -c
    bool generate_config = false;                      // --generate-config
    bool show_help = false;                            // --help / -h
    bool error = false;                                // parse error
    std::string message;                               // error text (when error)
};

CliOptions parse_cli(int argc, char** argv);
std::string help_text();

// Writes dir/runner.toml scaffold. Returns 0 on success; non-zero if the file
// exists or cannot be written. Sets `message` either way.
int generate_config_file(const std::filesystem::path& dir, std::string& message);
