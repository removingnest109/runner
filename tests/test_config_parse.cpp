#include <doctest/doctest.h>
#include "config.hpp"

TEST_CASE("parse_config reads a valid action with all fields") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Build"
cmd   = "make"
desc  = "build it"
cwd   = "firmware"
group = "Dev"
)");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 1);
    CHECK(r.actions[0].label == "Build");
    CHECK(r.actions[0].cmd == "make");
    CHECK(r.actions[0].desc == "build it");
    CHECK(r.actions[0].cwd == "firmware");
    CHECK(r.actions[0].group == "Dev");
}

TEST_CASE("parse_config leaves optional fields empty when unset") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Test"
cmd   = "ctest"
)");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 1);
    CHECK(r.actions[0].desc.empty());
    CHECK(r.actions[0].cwd.empty());
    CHECK(r.actions[0].group.empty());
}

TEST_CASE("parse_config reports missing required fields") {
    ParseResult r = parse_config(R"(
[[action]]
desc = "no label or cmd"
)");
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("parse_config reports malformed TOML") {
    ParseResult r = parse_config("this is = = not toml");
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("parse_config reports when no actions present") {
    ParseResult r = parse_config("# just a comment\n");
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("parse_config preserves file order across multiple actions") {
    ParseResult r = parse_config(R"(
[[action]]
label = "A"
cmd = "a"
[[action]]
label = "B"
cmd = "b"
)");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 2);
    CHECK(r.actions[0].label == "A");
    CHECK(r.actions[1].label == "B");
}
