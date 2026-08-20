#include "display_order.hpp"
#include "label_path.hpp"

#include <string>
#include <vector>

namespace {

// The header path an action nests under: every label segment except the last
// (the leaf). A label with no path segments (a root action) normalizes to
// {"Ungrouped"} so the no-group case is just another single-level group and
// needs no special casing downstream.
std::vector<std::string> header_path(const std::vector<std::string>& segments) {
    std::vector<std::string> path(segments.begin(), segments.end() - 1);
    if (path.empty()) path.push_back("Ungrouped");
    return path;
}

// Recursively flatten the group tree for the actions referenced by `idxs` (in
// file order) whose header paths agree up to `depth`. At this node an action is
// a direct child if its path ends here (path.size() == depth); otherwise it
// belongs to subgroup path[depth]. Children — direct actions and subgroups
// alike — are emitted in first-appearance order, so scattered subgroups of one
// parent stay contiguous under a single header and direct actions interleave
// with subgroups by where they first appear.
void flatten(const std::vector<Action>& actions,
             const std::vector<std::vector<std::string>>& paths,
             const std::vector<std::string>& leaves,
             const std::vector<std::size_t>& idxs, std::size_t depth,
             std::vector<DisplayRow>& out) {
    std::vector<std::string> subgroup_order;

    for (std::size_t k = 0; k < idxs.size(); ++k) {
        std::size_t i = idxs[k];
        if (paths[i].size() == depth) {
            // Direct action of this node: emit immediately in appearance order.
            out.push_back({actions[i], paths[i], leaves[i]});
        } else {
            const std::string& name = paths[i][depth];
            bool known = false;
            for (const auto& s : subgroup_order)
                if (s == name) { known = true; break; }
            if (!known) {
                subgroup_order.push_back(name);
                // Recurse into this subgroup with its members in file order.
                std::vector<std::size_t> child;
                for (std::size_t j = 0; j < idxs.size(); ++j) {
                    std::size_t ij = idxs[j];
                    if (paths[ij].size() > depth && paths[ij][depth] == name)
                        child.push_back(ij);
                }
                flatten(actions, paths, leaves, child, depth + 1, out);
            }
        }
    }
}

}  // namespace

std::vector<DisplayRow> build_display_order(const std::vector<Action>& actions) {
    // Hidden actions stay resolvable as chain members but never appear in the
    // menu, so they're dropped before grouping (an all-hidden group then yields
    // no rows and thus no header).
    std::vector<std::vector<std::string>> paths(actions.size());
    std::vector<std::string> leaves(actions.size());
    std::vector<std::size_t> visible;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        if (actions[i].hidden) continue;
        std::vector<std::string> segments = split_label(actions[i].label);
        leaves[i] = segments.back();      // split_label always yields >= 1 segment
        paths[i] = header_path(segments);
        visible.push_back(i);
    }

    std::vector<DisplayRow> out;
    out.reserve(visible.size());
    flatten(actions, paths, leaves, visible, 0, out);
    return out;
}
