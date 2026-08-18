#include <doctest/doctest.h>
#include "cli.hpp"
#include <filesystem>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;

static CliOptions run_cli(std::vector<const char*> args) {
    // args includes argv[0].
    return parse_cli(static_cast<int>(args.size()),
                     const_cast<char**>(args.data()));
}

TEST_CASE("no args yields no flags") {
    CliOptions o = run_cli({"runner"});
    CHECK_FALSE(o.error);
    CHECK_FALSE(o.show_help);
    CHECK_FALSE(o.generate_config);
    CHECK_FALSE(o.config_path.has_value());
}

TEST_CASE("--help sets show_help") {
    CHECK(run_cli({"runner", "--help"}).show_help);
    CHECK(run_cli({"runner", "-h"}).show_help);
}

TEST_CASE("--version sets show_version") {
    CHECK(run_cli({"runner", "--version"}).show_version);
    CHECK(run_cli({"runner", "-v"}).show_version);
}

TEST_CASE("version_text names runner and is non-empty") {
    std::string v = version_text();
    CHECK(v.rfind("runner ", 0) == 0);  // starts with "runner "
    CHECK(v.back() == '\n');
    CHECK(v.size() > std::string("runner \n").size());  // has a version body
}

TEST_CASE("--generate-config sets the flag") {
    CHECK(run_cli({"runner", "--generate-config"}).generate_config);
}

TEST_CASE("--config captures a path") {
    CliOptions o = run_cli({"runner", "--config", "/tmp/x.toml"});
    REQUIRE(o.config_path.has_value());
    CHECK(o.config_path->string() == "/tmp/x.toml");
}

TEST_CASE("-c captures a path") {
    CliOptions o = run_cli({"runner", "-c", "a.toml"});
    REQUIRE(o.config_path.has_value());
    CHECK(o.config_path->string() == "a.toml");
}

TEST_CASE("--config without a value is an error") {
    CHECK(run_cli({"runner", "--config"}).error);
}

TEST_CASE("unknown flag is an error") {
    CHECK(run_cli({"runner", "--nope"}).error);
}

TEST_CASE("generate_config_file writes a scaffold and refuses to overwrite") {
    fs::path dir = fs::temp_directory_path() /
        ("runner_cli_" + std::to_string(::getpid()));
    fs::create_directories(dir);
    fs::remove(dir / "runner.toml");

    std::string msg;
    CHECK(generate_config_file(dir, msg) == 0);
    CHECK(fs::exists(dir / "runner.toml"));

    // second call must refuse
    std::string msg2;
    CHECK(generate_config_file(dir, msg2) != 0);

    fs::remove_all(dir);
}
