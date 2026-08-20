#pragma once
#include "action.hpp"
#include <string>
#include <vector>

// One row of the rendered sidebar: a single action, the group path it nests
// under, and its display name. Both are derived from the action's path-style
// `label` (split on '/'): the last segment is the `leaf` (shown in the row), the
// earlier segments are the `path` (drawn as nested headers). A label with no
// '/' is a root action; its `path` normalizes to {"Ungrouped"}, so `path` is
// never empty. The row list stays FLAT (one entry per visible action, in
// display order) so the sidebar's integer selection index and all navigation
// code are unaffected by nesting — the path only tells the renderer which
// headers to draw and how deep to indent.
struct DisplayRow {
    Action action;                  // the full action; action.label is the path
    std::vector<std::string> path;  // header segments, e.g. {"Packaging","Debian"}
    std::string leaf;               // display name, e.g. "build"
};

// Order the visible actions for the sidebar, grouping hierarchically by their
// label paths.
//
// Hidden actions are excluded (they stay resolvable as chain members but never
// appear in the menu). The remaining actions are bucketed into a group tree by
// the path portion of their label and flattened depth-first. Within every tree
// node the children — each either a direct action (a label ending at this node)
// or a subgroup (a deeper path segment) — are ordered by first appearance in
// file order, interleaving direct actions and subgroups. Actions within a leaf
// keep file order. This makes a parent's scattered subgroups render under a
// single, once-emitted parent header.
std::vector<DisplayRow> build_display_order(const std::vector<Action>& actions);
