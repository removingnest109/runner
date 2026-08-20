#include <doctest/doctest.h>
#include "config.hpp"

TEST_CASE("parse_config reads a valid action with all fields") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Dev/Build"
cmd   = "make"
desc  = "build it"
cwd   = "firmware"
)");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 1);
    CHECK(r.actions[0].label == "Dev/Build");
    CHECK(r.actions[0].cmd == "make");
    CHECK(r.actions[0].desc == "build it");
    CHECK(r.actions[0].cwd == "firmware");
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
}

TEST_CASE("parse_config reports missing required fields") {
    ParseResult r = parse_config(R"(
[[action]]
desc = "no label or cmd"
)");
    CHECK_FALSE(r.errors.empty());
    CHECK(r.actions.empty());
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

TEST_CASE("parse_config reads an inline env table into Action.env") {
    ParseResult r = parse_config(R"(
[[action]]
label = "E"
cmd = "true"
env = { FOO = "bar", BAZ = "qux" }
)");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 1);
    REQUIRE(r.actions[0].env.size() == 2);
    bool foo = false, baz = false;   // toml++ table order is not relied upon
    for (const auto& [k, v] : r.actions[0].env) {
        if (k == "FOO" && v == "bar") foo = true;
        if (k == "BAZ" && v == "qux") baz = true;
    }
    CHECK(foo);
    CHECK(baz);
}

TEST_CASE("parse_config accepts the [action.env] sub-table form") {
    ParseResult r = parse_config(R"(
[[action]]
label = "E"
cmd = "true"
[action.env]
TARGET = "prod"
)");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 1);
    REQUIRE(r.actions[0].env.size() == 1);
    CHECK(r.actions[0].env[0].first == "TARGET");
    CHECK(r.actions[0].env[0].second == "prod");
}

TEST_CASE("parse_config leaves env empty when unset") {
    ParseResult r = parse_config("[[action]]\nlabel=\"E\"\ncmd=\"true\"\n");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 1);
    CHECK(r.actions[0].env.empty());
}

TEST_CASE("parse_config reports a non-string env value") {
    ParseResult r = parse_config(R"(
[[action]]
label = "E"
cmd = "true"
env = { PORT = 8080 }
)");
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("parse_config reads depends_on into Action.depends_on") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Build"
cmd = "make"
[[action]]
label = "Test"
cmd = "ctest"
depends_on = ["Build"]
)");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 2);
    REQUIRE(r.actions[1].depends_on.size() == 1);
    CHECK(r.actions[1].depends_on[0] == "Build");
}

TEST_CASE("parse_config reads a composite (sequence, no cmd)") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Fmt"
cmd = "fmt"
[[action]]
label = "Lint"
cmd = "lint"
[[action]]
label = "Checks"
sequence = ["Fmt", "Lint"]
)");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 3);
    CHECK(r.actions[2].cmd.empty());
    CHECK(r.actions[2].is_composite());
    REQUIRE(r.actions[2].sequence.size() == 2);
    CHECK(r.actions[2].sequence[0] == "Fmt");
    CHECK(r.actions[2].sequence[1] == "Lint");
}

TEST_CASE("parse_config reads only_if_cmd") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Lint"
cmd = "lint"
only_if_cmd = "! git diff --quiet -- src/"
)");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 1);
    CHECK(r.actions[0].only_if_cmd == "! git diff --quiet -- src/");
}

TEST_CASE("parse_config rejects both cmd and sequence on one action") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Both"
cmd = "make"
sequence = ["X"]
)");
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("parse_config rejects an action with neither cmd nor sequence") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Empty"
desc = "no cmd, no sequence"
)");
    CHECK_FALSE(r.errors.empty());
    CHECK(r.actions.empty());
}

TEST_CASE("parse_config reports a non-string depends_on entry") {
    ParseResult r = parse_config(R"(
[[action]]
label = "A"
cmd = "a"
depends_on = [42]
)");
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("parse_config reads the hidden flag (default false)") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Visible"
cmd = "v"
[[action]]
label = "Secret"
cmd = "s"
hidden = true
)");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 2);
    CHECK_FALSE(r.actions[0].hidden);
    CHECK(r.actions[1].hidden);
}

TEST_CASE("parse_config rejects duplicate labels") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Build"
cmd = "make"
[[action]]
label = "Build"
cmd = "make again"
)");
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("parse_config rejects an unknown depends_on reference") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Test"
cmd = "ctest"
depends_on = ["Nope"]
)");
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("parse_config rejects only_if_cmd on a composite") {
    // A composite expands to its members, so it owns no command to gate; a gate
    // here would be silently dropped. Reject it at load instead.
    ParseResult r = parse_config(R"(
[[action]]
label = "X"
cmd = "x"
[[action]]
label = "Checks"
sequence = ["X"]
only_if_cmd = "true"
)");
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("parse_config allows only_if_cmd on a command action") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Lint"
cmd = "lint"
only_if_cmd = "true"
)");
    CHECK(r.errors.empty());
}

TEST_CASE("parse_config rejects a dependency cycle") {
    ParseResult r = parse_config(R"(
[[action]]
label = "A"
cmd = "a"
depends_on = ["B"]
[[action]]
label = "B"
cmd = "b"
depends_on = ["A"]
)");
    CHECK_FALSE(r.errors.empty());
}

// ---- path-label canonicalization + validation (folded group) --------------

TEST_CASE("parse_config canonicalizes whitespace around path segments") {
    ParseResult r = parse_config(R"(
[[action]]
label = " Packaging / Arch / build package "
cmd = "makepkg"
)");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 1);
    // Surrounding whitespace per segment is trimmed; internal space is kept.
    CHECK(r.actions[0].label == "Packaging/Arch/build package");
}

TEST_CASE("parse_config canonicalizes references so they resolve") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Dev/Build"
cmd = "make"
[[action]]
label = "Dev/Check"
sequence = [" Dev / Build "]
)");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 2);
    CHECK(r.actions[1].sequence[0] == "Dev/Build");
}

TEST_CASE("parse_config rejects an empty path segment in a label") {
    for (const char* bad : {"a//b", "/b", "a/", ""}) {
        ParseResult r = parse_config(std::string("[[action]]\nlabel = \"") +
                                     bad + "\"\ncmd = \"x\"\n");
        CHECK_FALSE(r.errors.empty());
    }
}

TEST_CASE("parse_config rejects an empty path segment in a reference") {
    ParseResult r = parse_config(R"(
[[action]]
label = "A"
cmd = "a"
[[action]]
label = "B"
cmd = "b"
depends_on = ["bad//ref"]
)");
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("parse_config allows the same leaf under different groups") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Packaging/Arch/build"
cmd = "makepkg"
[[action]]
label = "Packaging/Debian/build"
cmd = "dpkg-buildpackage"
)");
    CHECK(r.errors.empty());
    CHECK(r.actions.size() == 2);
}

TEST_CASE("parse_config still rejects two identical full-path labels") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Packaging/Arch/build"
cmd = "makepkg"
[[action]]
label = "Packaging/Arch/build"
cmd = "makepkg -f"
)");
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("parse_config resolves a bare (root) label reference") {
    ParseResult r = parse_config(R"(
[[action]]
label = "Build"
cmd = "make"
[[action]]
label = "Packaging/Arch/build"
cmd = "makepkg"
depends_on = ["Build"]
)");
    CHECK(r.errors.empty());
    CHECK(r.actions.size() == 2);
}
