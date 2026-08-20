#include <doctest/doctest.h>
#include "plan.hpp"

namespace {

// Build a command action (has a cmd).
Action cmd(std::string label, std::vector<std::string> deps = {}) {
    Action a;
    a.label = std::move(label);
    a.cmd = "run-" + a.label;
    a.depends_on = std::move(deps);
    return a;
}

// Build a composite action (has a sequence, no cmd).
Action seq(std::string label, std::vector<std::string> members,
           std::vector<std::string> deps = {}) {
    Action a;
    a.label = std::move(label);
    a.sequence = std::move(members);
    a.depends_on = std::move(deps);
    return a;
}

}  // namespace

// ---- validate_graph -------------------------------------------------------

TEST_CASE("validate_graph accepts a well-formed graph") {
    std::vector<Action> as = {cmd("Build"), cmd("Test", {"Build"})};
    CHECK(validate_graph(as).empty());
}

TEST_CASE("validate_graph rejects duplicate labels") {
    std::vector<Action> as = {cmd("Build"), cmd("Build")};
    CHECK_FALSE(validate_graph(as).empty());
}

TEST_CASE("validate_graph rejects an unknown depends_on reference") {
    std::vector<Action> as = {cmd("Test", {"Nope"})};
    CHECK_FALSE(validate_graph(as).empty());
}

TEST_CASE("validate_graph rejects an unknown sequence reference") {
    std::vector<Action> as = {seq("Checks", {"Nope"})};
    CHECK_FALSE(validate_graph(as).empty());
}

TEST_CASE("validate_graph rejects a direct cycle") {
    std::vector<Action> as = {cmd("A", {"B"}), cmd("B", {"A"})};
    CHECK_FALSE(validate_graph(as).empty());
}

TEST_CASE("validate_graph rejects an indirect cycle through a composite") {
    std::vector<Action> as = {seq("A", {"B"}), cmd("B", {"A"})};
    CHECK_FALSE(validate_graph(as).empty());
}

TEST_CASE("validate_graph rejects a self-dependency") {
    std::vector<Action> as = {cmd("A", {"A"})};
    CHECK_FALSE(validate_graph(as).empty());
}

// ---- resolve_plan ---------------------------------------------------------

TEST_CASE("resolve_plan of a plain command yields just that command") {
    std::vector<Action> as = {cmd("Build")};
    Plan p = resolve_plan(as, "Build");
    REQUIRE(p.errors.empty());
    CHECK(p.steps == std::vector<std::string>{"Build"});
}

TEST_CASE("resolve_plan runs depends_on before the target") {
    std::vector<Action> as = {cmd("Build"), cmd("Test", {"Build"})};
    Plan p = resolve_plan(as, "Test");
    REQUIRE(p.errors.empty());
    CHECK(p.steps == std::vector<std::string>{"Build", "Test"});
}

TEST_CASE("resolve_plan expands a composite into its members, not itself") {
    std::vector<Action> as = {cmd("Fmt"), cmd("Lint"), seq("Checks", {"Fmt", "Lint"})};
    Plan p = resolve_plan(as, "Checks");
    REQUIRE(p.errors.empty());
    CHECK(p.steps == std::vector<std::string>{"Fmt", "Lint"});
}

TEST_CASE("resolve_plan deduplicates a shared dependency, keeping it first") {
    // Both Lint and Test depend on Build; Checks runs both.
    std::vector<Action> as = {
        cmd("Build"),
        cmd("Lint", {"Build"}),
        cmd("Test", {"Build"}),
        seq("Checks", {"Lint", "Test"}),
    };
    Plan p = resolve_plan(as, "Checks");
    REQUIRE(p.errors.empty());
    CHECK(p.steps == std::vector<std::string>{"Build", "Lint", "Test"});
}

TEST_CASE("resolve_plan expands a composite listed as a dependency") {
    std::vector<Action> as = {
        cmd("X"), cmd("Y"),
        seq("Pre", {"X", "Y"}),
        cmd("Deploy", {"Pre"}),
    };
    Plan p = resolve_plan(as, "Deploy");
    REQUIRE(p.errors.empty());
    CHECK(p.steps == std::vector<std::string>{"X", "Y", "Deploy"});
}

TEST_CASE("resolve_plan reports an unknown target") {
    std::vector<Action> as = {cmd("Build")};
    Plan p = resolve_plan(as, "Ghost");
    CHECK_FALSE(p.errors.empty());
}

TEST_CASE("resolve_plan does not hang on a cycle") {
    std::vector<Action> as = {cmd("A", {"B"}), cmd("B", {"A"})};
    Plan p = resolve_plan(as, "A");
    CHECK_FALSE(p.errors.empty());
}
