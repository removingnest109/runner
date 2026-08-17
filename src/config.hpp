#pragma once
#include "action.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct ParseResult {
    std::vector<Action> actions;
    std::vector<std::string> errors;  // non-empty => invalid config
};

// Structural parse only; no filesystem access. cwd stored raw.
ParseResult parse_config(const std::string& toml_content);

// Walk up from start_dir to the filesystem root; first runner.toml wins.
std::optional<std::filesystem::path> find_config(std::filesystem::path start_dir);

// Read + parse the file, then resolve each action's cwd to an absolute path
// (empty => config dir, relative => config dir / cwd, absolute => as-is),
// erroring if a resolved cwd is not an existing directory.
ParseResult load_config(const std::filesystem::path& config_path);
