#pragma once
#include <string>
#include <utility>
#include <vector>

// One runnable entry from runner.toml. Empty string means the field was unset.
struct Action {
    // Path-style unique key: "Packaging/Arch/build" nests the action under
    // Packaging › Arch and shows "build" in the sidebar. The last '/'-segment is
    // the display name; earlier segments are its groups/subgroups. A label with
    // no '/' is a root action (rendered under "Ungrouped"). References in
    // depends_on / sequence use this full path. Required in config.
    std::string label;
    std::string cmd;    // passed to /bin/sh -c (required unless `sequence` is set)
    std::string desc;   // optional description
    std::string cwd;    // optional working dir; resolved to absolute at load time

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

    // When true, the action is omitted from the menu but stays fully
    // resolvable/runnable as a chain member (depends_on / sequence). Lets a
    // composite's steps be referenced without cluttering the sidebar.
    bool hidden = false;

    bool is_composite() const { return !sequence.empty(); }
};
