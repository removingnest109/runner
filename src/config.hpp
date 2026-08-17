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
