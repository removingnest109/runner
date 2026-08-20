#pragma once
#include <string>
#include <utility>
#include <vector>

// One runnable entry from runner.toml. Empty string means the field was unset.
struct Action {
    std::string label;  // shown in the sidebar; unique key (required in config)
    std::string cmd;    // passed to /bin/sh -c (required unless `sequence` is set)
    std::string desc;   // optional description
    std::string cwd;    // optional working dir; resolved to absolute at load time
    std::string group;  // optional group label; "" renders under "Ungrouped"

    // Environment variables injected into this action's child process.
    // Ordered; empty means none. (toml++ table iteration order; not relied upon.)
    std::vector<std::pair<std::string, std::string>> env;

    // Prerequisite action labels; run (deduped, in order) before this action.
    std::vector<std::string> depends_on;

    // Composite: labels run in order when this action is triggered. Mutually
    // exclusive with `cmd`. An action with a non-empty `sequence` has no command
    // of its own.
    std::vector<std::string> sequence;

    // Optional gate command, run via /bin/sh -c in this action's cwd+env before
    // the action itself. Exit 0 => run the action; non-zero => skip it. Empty
    // means always run.
    std::string only_if_cmd;

    bool is_composite() const { return !sequence.empty(); }
};
