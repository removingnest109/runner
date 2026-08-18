#pragma once
#include "action.hpp"
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

struct ParseResult {
    std::vector<Action> actions;
    std::vector<std::string> errors;  // non-empty => invalid config
};

// Structural parse only; no filesystem access. cwd stored raw.
ParseResult parse_config(const std::string& toml_content);

// Expand ${VAR} references in `in`. `lookup(name)` returns the value or nullopt
// when undefined; each undefined reference is reported via `on_undefined(name)`
// and expands to "". A '$' not followed by '{' is literal; an unterminated "${"
// is left verbatim. Pure: no getenv, no filesystem — directly unit-testable.
std::string expand_vars(
    const std::string& in,
    const std::function<std::optional<std::string>(std::string_view)>& lookup,
    const std::function<void(std::string_view)>& on_undefined);

// Walk up from start_dir to the filesystem root; first runner.toml wins.
std::optional<std::filesystem::path> find_config(std::filesystem::path start_dir);

// Read + parse the file, then resolve each action's cwd to an absolute path
// (empty => config dir, relative => config dir / cwd, absolute => as-is),
// erroring if a resolved cwd is not an existing directory.
ParseResult load_config(const std::filesystem::path& config_path);
