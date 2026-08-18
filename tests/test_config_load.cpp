#include <doctest/doctest.h>
#include "config.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;

static fs::path make_temp_dir() {
    fs::path base = fs::temp_directory_path() /
        ("runner_test_" + std::to_string(::getpid()) + "_" +
         std::to_string(reinterpret_cast<uintptr_t>(&base)));
    fs::create_directories(base);
    return base;
}

TEST_CASE("find_config walks up to a parent directory") {
    fs::path root = make_temp_dir();
    fs::create_directories(root / "a" / "b");
    std::ofstream(root / "runner.toml") << "# cfg\n";

    auto found = find_config(root / "a" / "b");
    REQUIRE(found.has_value());
    CHECK(fs::equivalent(*found, root / "runner.toml"));
    fs::remove_all(root);
}

TEST_CASE("find_config finds nothing within a subtree that has no runner.toml") {
    fs::path root = make_temp_dir();
    fs::create_directories(root / "x" / "y");
    // No runner.toml is created anywhere in this temp subtree.
    auto found = find_config(root / "x" / "y");
    // find_config walks up to the filesystem root, so an unrelated runner.toml
    // in a real ancestor (e.g. above the temp dir) could exist; what must hold
    // is that it never reports a file inside our freshly-created subtree.
    if (found.has_value()) {
        CHECK(!found->string().starts_with(root.string()));  // not under our temp root
    }
    CHECK_FALSE(fs::exists(root / "runner.toml"));  // sanity: none at our root
    fs::remove_all(root);
}

TEST_CASE("load_config resolves empty cwd to the config directory") {
    fs::path root = make_temp_dir();
    std::ofstream(root / "runner.toml") <<
        "[[action]]\nlabel=\"A\"\ncmd=\"echo hi\"\n";
    ParseResult r = load_config(root / "runner.toml");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 1);
    CHECK(fs::path(r.actions[0].cwd) == fs::weakly_canonical(root));
    fs::remove_all(root);
}

TEST_CASE("load_config resolves a relative cwd against the config directory") {
    fs::path root = make_temp_dir();
    fs::create_directories(root / "firmware");
    std::ofstream(root / "runner.toml") <<
        "[[action]]\nlabel=\"A\"\ncmd=\"make\"\ncwd=\"firmware\"\n";
    ParseResult r = load_config(root / "runner.toml");
    REQUIRE(r.errors.empty());
    CHECK(fs::path(r.actions[0].cwd) == fs::weakly_canonical(root / "firmware"));
    fs::remove_all(root);
}

TEST_CASE("load_config errors when cwd does not exist") {
    fs::path root = make_temp_dir();
    std::ofstream(root / "runner.toml") <<
        "[[action]]\nlabel=\"A\"\ncmd=\"make\"\ncwd=\"nope\"\n";
    ParseResult r = load_config(root / "runner.toml");
    CHECK_FALSE(r.errors.empty());
    fs::remove_all(root);
}

TEST_CASE("load_config passes an absolute cwd through unchanged") {
    fs::path root = make_temp_dir();
    fs::path abs = root / "sub";
    fs::create_directories(abs);
    std::ofstream(root / "runner.toml") <<
        "[[action]]\nlabel=\"A\"\ncmd=\"make\"\ncwd=\"" << abs.string() << "\"\n";
    ParseResult r = load_config(root / "runner.toml");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions.size() == 1);
    CHECK(fs::path(r.actions[0].cwd) == fs::weakly_canonical(abs));
    fs::remove_all(root);
}

TEST_CASE("expand_vars substitutes a defined variable") {
    auto lookup = [](std::string_view n) -> std::optional<std::string> {
        if (n == "FOO") return std::string("bar");
        return std::nullopt;
    };
    bool undef = false;
    std::string out = expand_vars("x=${FOO}!", lookup, [&](std::string_view){ undef = true; });
    CHECK(out == "x=bar!");
    CHECK_FALSE(undef);
}

TEST_CASE("expand_vars reports an undefined variable and expands to empty") {
    auto lookup = [](std::string_view) -> std::optional<std::string> { return std::nullopt; };
    std::string seen;
    std::string out = expand_vars("a${MISSING}b", lookup,
                                  [&](std::string_view n){ seen = std::string(n); });
    CHECK(out == "ab");
    CHECK(seen == "MISSING");
}

TEST_CASE("expand_vars leaves a literal dollar untouched") {
    auto lookup = [](std::string_view) -> std::optional<std::string> { return std::nullopt; };
    std::string out = expand_vars("cost $5 and $x", lookup, [](std::string_view){});
    CHECK(out == "cost $5 and $x");
}

TEST_CASE("expand_vars leaves an unterminated brace verbatim") {
    auto lookup = [](std::string_view) -> std::optional<std::string> { return std::string("v"); };
    std::string out = expand_vars("a${UNCLOSED", lookup, [](std::string_view){});
    CHECK(out == "a${UNCLOSED");
}

TEST_CASE("load_config expands ${VAR} in cwd before resolving it") {
    fs::path root = make_temp_dir();
    fs::create_directories(root / "sub");
    setenv("RUNNER_TEST_SUB", "sub", 1);
    std::ofstream(root / "runner.toml") <<
        "[[action]]\nlabel=\"A\"\ncmd=\"true\"\ncwd=\"${RUNNER_TEST_SUB}\"\n";
    ParseResult r = load_config(root / "runner.toml");
    REQUIRE(r.errors.empty());
    CHECK(fs::path(r.actions[0].cwd) == fs::weakly_canonical(root / "sub"));
    unsetenv("RUNNER_TEST_SUB");
    fs::remove_all(root);
}

TEST_CASE("load_config expands ${VAR} in env values") {
    fs::path root = make_temp_dir();
    setenv("RUNNER_TEST_PREFIX", "/opt/x", 1);
    std::ofstream(root / "runner.toml") <<
        "[[action]]\nlabel=\"A\"\ncmd=\"true\"\nenv = { P = \"${RUNNER_TEST_PREFIX}/bin\" }\n";
    ParseResult r = load_config(root / "runner.toml");
    REQUIRE(r.errors.empty());
    REQUIRE(r.actions[0].env.size() == 1);
    CHECK(r.actions[0].env[0].second == "/opt/x/bin");
    unsetenv("RUNNER_TEST_PREFIX");
    fs::remove_all(root);
}

TEST_CASE("load_config errors on an undefined variable in cwd") {
    fs::path root = make_temp_dir();
    unsetenv("RUNNER_DEFINITELY_UNSET");
    std::ofstream(root / "runner.toml") <<
        "[[action]]\nlabel=\"A\"\ncmd=\"true\"\ncwd=\"${RUNNER_DEFINITELY_UNSET}\"\n";
    ParseResult r = load_config(root / "runner.toml");
    CHECK_FALSE(r.errors.empty());
    fs::remove_all(root);
}
