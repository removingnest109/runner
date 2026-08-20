#pragma once
#include "action.hpp"
#include <string>
#include <vector>

// Whole-graph validation over the depends_on + sequence edges of every action.
// Returns error strings; empty => the graph is well formed. Checks, in order:
//   - duplicate labels (labels are the reference key, so they must be unique),
//   - references (depends_on / sequence entries) to labels that don't exist,
//   - dependency cycles across the combined depends_on + sequence edge set.
// Pure: no filesystem, no process launch. Unit-testable in isolation.
std::vector<std::string> validate_graph(const std::vector<Action>& actions);

struct Plan {
    // Labels of the command actions to execute, in order, deduplicated. A
    // composite contributes its (recursively expanded) members, not itself, so
    // every entry here names an action that owns a `cmd`. The triggered target's
    // own command, when it has one, is last.
    std::vector<std::string> steps;
    std::vector<std::string> errors;  // non-empty => could not resolve
};

// Flatten the execution chain for `target_label`: recursively its depends_on,
// then (if it is a composite) its sequence members, otherwise its own command
// last. Shared subgraphs are expanded once (dedup by label). Order is
// dependency-then-declaration. Guards against cycles defensively even on an
// unvalidated graph. Pure.
Plan resolve_plan(const std::vector<Action>& actions, const std::string& target_label);
