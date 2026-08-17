#pragma once
#include <string>

// One runnable entry from runner.toml. Empty string means the field was unset.
struct Action {
    std::string label;  // shown in the sidebar (required in config)
    std::string cmd;    // passed to /bin/sh -c (required in config)
    std::string desc;   // optional description
    std::string cwd;    // optional working dir; resolved to absolute at load time
    std::string group;  // optional group label; "" renders under "Ungrouped"
};
