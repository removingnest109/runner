#include "plan.hpp"

#include <unordered_map>
#include <unordered_set>

namespace {

// Index actions by label. Assumes labels are unique (validate_graph enforces
// that); on duplicates the last one wins, which is harmless here because callers
// resolve plans only after validation has passed.
std::unordered_map<std::string, const Action*>
index_by_label(const std::vector<Action>& actions) {
    std::unordered_map<std::string, const Action*> by_label;
    for (const auto& a : actions) by_label[a.label] = &a;
    return by_label;
}

}  // namespace

std::vector<std::string> validate_graph(const std::vector<Action>& actions) {
    std::vector<std::string> errors;

    // Duplicate labels. Report each label once, on its second occurrence.
    std::unordered_set<std::string> seen;
    for (const auto& a : actions) {
        if (!seen.insert(a.label).second)
            errors.push_back("duplicate label: '" + a.label + "'");
    }

    // Unknown references (depends_on / sequence entries).
    auto by_label = index_by_label(actions);
    auto check_refs = [&](const Action& a, const std::vector<std::string>& refs,
                          const char* kind) {
        for (const auto& r : refs) {
            if (by_label.find(r) == by_label.end())
                errors.push_back("action '" + a.label + "': " + kind +
                                 " references unknown label '" + r + "'");
        }
    };
    for (const auto& a : actions) {
        check_refs(a, a.depends_on, "depends_on");
        check_refs(a, a.sequence, "sequence");
    }

    // Cycles across the combined depends_on + sequence edge set. Standard
    // 3-colour DFS: 0 = unvisited, 1 = on the current stack, 2 = done.
    std::unordered_map<std::string, int> colour;
    bool cycle_found = false;
    // Recursive lambda via explicit std::function-free self reference.
    struct Dfs {
        const std::unordered_map<std::string, const Action*>& by_label;
        std::unordered_map<std::string, int>& colour;
        bool& cycle_found;
        void operator()(const std::string& label) {
            auto it = by_label.find(label);
            if (it == by_label.end()) return;  // unknown ref already reported
            colour[label] = 1;
            const Action* a = it->second;
            auto walk = [&](const std::vector<std::string>& refs) {
                for (const auto& r : refs) {
                    int c = colour.count(r) ? colour[r] : 0;
                    if (c == 1) { cycle_found = true; }
                    else if (c == 0) (*this)(r);
                }
            };
            walk(a->depends_on);
            walk(a->sequence);
            colour[label] = 2;
        }
    } dfs{by_label, colour, cycle_found};
    for (const auto& a : actions) {
        if ((colour.count(a.label) ? colour[a.label] : 0) == 0) dfs(a.label);
        if (cycle_found) break;
    }
    if (cycle_found)
        errors.push_back("dependency cycle detected");

    return errors;
}

Plan resolve_plan(const std::vector<Action>& actions, const std::string& target_label) {
    Plan plan;
    auto by_label = index_by_label(actions);

    if (by_label.find(target_label) == by_label.end()) {
        plan.errors.push_back("unknown action: '" + target_label + "'");
        return plan;
    }

    std::unordered_set<std::string> on_stack;  // cycle guard
    std::unordered_set<std::string> done;      // fully expanded (dedup)
    std::unordered_set<std::string> emitted;   // labels already in plan.steps

    // Recursive expansion. A composite contributes its members; a command
    // contributes itself. depends_on always run before the node's own body.
    struct Expand {
        const std::unordered_map<std::string, const Action*>& by_label;
        std::unordered_set<std::string>& on_stack;
        std::unordered_set<std::string>& done;
        std::unordered_set<std::string>& emitted;
        Plan& plan;
        void operator()(const std::string& label) {
            if (on_stack.count(label)) {
                plan.errors.push_back("dependency cycle at '" + label + "'");
                return;
            }
            if (done.count(label)) return;
            auto it = by_label.find(label);
            if (it == by_label.end()) {
                plan.errors.push_back("unknown action: '" + label + "'");
                return;
            }
            on_stack.insert(label);
            const Action* a = it->second;
            for (const auto& dep : a->depends_on) (*this)(dep);
            if (a->is_composite()) {
                for (const auto& m : a->sequence) (*this)(m);
            } else if (emitted.insert(label).second) {
                plan.steps.push_back(label);
            }
            on_stack.erase(label);
            done.insert(label);
        }
    } expand{by_label, on_stack, done, emitted, plan};

    expand(target_label);
    return plan;
}
