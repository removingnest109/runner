#include <doctest/doctest.h>
#include "display_order.hpp"

namespace {

// Build a command action with a path-style label.
Action act(std::string label, bool hidden = false) {
    Action a;
    a.label = std::move(label);
    a.cmd = "run-" + a.label;
    a.hidden = hidden;
    return a;
}

using Path = std::vector<std::string>;

// The rows' display names (leaves) in display order.
std::vector<std::string> leaves(const std::vector<DisplayRow>& rows) {
    std::vector<std::string> out;
    for (const auto& r : rows) out.push_back(r.leaf);
    return out;
}

}  // namespace

// ---- single-level groups ---------------------------------------------------

TEST_CASE("single-level groups keep first-appearance order and file order within") {
    std::vector<Action> as = {
        act("Dev/A"),
        act("Build/B"),
        act("Dev/C"),
    };
    auto rows = build_display_order(as);
    // Dev appears first, so all Dev actions (file order), then Build.
    CHECK(leaves(rows) == std::vector<std::string>{"A", "C", "B"});
    CHECK(rows[0].path == Path{"Dev"});
    CHECK(rows[1].path == Path{"Dev"});
    CHECK(rows[2].path == Path{"Build"});
}

// ---- root (Ungrouped) ------------------------------------------------------

TEST_CASE("a label with no slash is a root action under Ungrouped") {
    std::vector<Action> as = {act("A"), act("B")};
    auto rows = build_display_order(as);
    CHECK(leaves(rows) == std::vector<std::string>{"A", "B"});
    CHECK(rows[0].path == Path{"Ungrouped"});
    CHECK(rows[1].path == Path{"Ungrouped"});
}

// ---- two-level nesting -----------------------------------------------------

TEST_CASE("two-level nesting splits the path off the leaf") {
    std::vector<Action> as = {
        act("Packaging/Debian/deb"),
        act("Packaging/Arch/arch"),
    };
    auto rows = build_display_order(as);
    CHECK(leaves(rows) == std::vector<std::string>{"deb", "arch"});
    CHECK(rows[0].path == Path{"Packaging", "Debian"});
    CHECK(rows[1].path == Path{"Packaging", "Arch"});
}

// ---- three-level nesting ---------------------------------------------------

TEST_CASE("three-level nesting preserves the full path") {
    std::vector<Action> as = {act("Packaging/Linux/Debian/build")};
    auto rows = build_display_order(as);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].path == Path{"Packaging", "Linux", "Debian"});
    CHECK(rows[0].leaf == "build");
}

// ---- scattered sibling subgroups under one parent --------------------------

TEST_CASE("scattered subgroups under one parent become contiguous") {
    std::vector<Action> as = {
        act("Packaging/Debian/deb"),
        act("Dev/build"),
        act("Packaging/Arch/arch"),
    };
    auto rows = build_display_order(as);
    // Packaging appeared first (via deb), so both its subgroups render before
    // Dev, contiguously: Debian (first seen) then Arch.
    CHECK(leaves(rows) == std::vector<std::string>{"deb", "arch", "build"});
    CHECK(rows[0].path == Path{"Packaging", "Debian"});
    CHECK(rows[1].path == Path{"Packaging", "Arch"});
    CHECK(rows[2].path == Path{"Dev"});
}

// ---- mixed direct-action + subgroup under one parent -----------------------

TEST_CASE("direct actions and subgroups interleave by first appearance") {
    std::vector<Action> as = {
        act("Packaging/pacman"),        // direct child of Packaging
        act("Packaging/Debian/deb"),    // subgroup Debian
        act("Packaging/aur"),           // another direct child
    };
    auto rows = build_display_order(as);
    // First appearance within Packaging: direct pacman, then subgroup Debian,
    // then direct aur. Direct actions and the subgroup interleave by order.
    CHECK(leaves(rows) == std::vector<std::string>{"pacman", "deb", "aur"});
    CHECK(rows[0].path == Path{"Packaging"});
    CHECK(rows[1].path == Path{"Packaging", "Debian"});
    CHECK(rows[2].path == Path{"Packaging"});
}

TEST_CASE("a subgroup seen before a direct action orders the subgroup first") {
    std::vector<Action> as = {
        act("Packaging/Debian/deb"),    // subgroup Debian seen first
        act("Packaging/pacman"),        // direct child seen after
    };
    auto rows = build_display_order(as);
    CHECK(leaves(rows) == std::vector<std::string>{"deb", "pacman"});
    CHECK(rows[0].path == Path{"Packaging", "Debian"});
    CHECK(rows[1].path == Path{"Packaging"});
}

// ---- hidden actions excluded -----------------------------------------------

TEST_CASE("hidden actions are excluded before grouping") {
    std::vector<Action> as = {
        act("Dev/A"),
        act("Dev/Secret", /*hidden=*/true),
        act("Dev/B"),
    };
    auto rows = build_display_order(as);
    CHECK(leaves(rows) == std::vector<std::string>{"A", "B"});
}

TEST_CASE("an all-hidden group produces no rows at all") {
    std::vector<Action> as = {
        act("Ghost/Only", /*hidden=*/true),
        act("Dev/A"),
    };
    auto rows = build_display_order(as);
    CHECK(leaves(rows) == std::vector<std::string>{"A"});
    CHECK(rows[0].path == Path{"Dev"});
}

// ---- combined scenario -----------------------------------------------------

TEST_CASE("nested and flat groups coexist in first-appearance order") {
    std::vector<Action> as = {
        act("Dev/build"),
        act("Packaging/Debian/deb"),
        act("Dev/test"),
        act("Packaging/Arch/arch"),
        act("loose"),
    };
    auto rows = build_display_order(as);
    CHECK(leaves(rows) ==
          std::vector<std::string>{"build", "test", "deb", "arch", "loose"});
    CHECK(rows[0].path == Path{"Dev"});
    CHECK(rows[1].path == Path{"Dev"});
    CHECK(rows[2].path == Path{"Packaging", "Debian"});
    CHECK(rows[3].path == Path{"Packaging", "Arch"});
    CHECK(rows[4].path == Path{"Ungrouped"});
}
