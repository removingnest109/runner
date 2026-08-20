#pragma once
#include <string>
#include <vector>

// Split a path-style label on '/', trimming surrounding whitespace from each
// segment. The last segment is the action's display name ("leaf"); any earlier
// segments are its nesting path (groups/subgroups). Empty segments are NOT
// dropped — they're returned as empty strings so callers can reject malformed
// labels like "a//b", "/b", or "a/" (an empty label yields {""}). Internal
// whitespace within a segment is preserved ("build package" stays intact).
//
// Examples:
//   "Dev/Build"        -> {"Dev", "Build"}
//   " Packaging / Arch "-> {"Packaging", "Arch"}
//   "Build"            -> {"Build"}
//   "a//b"             -> {"a", "", "b"}
std::vector<std::string> split_label(const std::string& label);
