#include <doctest/doctest.h>
#include "config.hpp"
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
