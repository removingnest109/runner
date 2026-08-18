#include <doctest/doctest.h>
#include "process_runner.hpp"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

static bool wait_until_done(ProcessRunner& pr, std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pr.state() != RunState::Running && pr.state() != RunState::Idle)
            return true;
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

TEST_CASE("runs a command, captures stdout, exits 0") {
    ProcessRunner pr([]{});
    pr.start(Action{.label="e", .cmd="echo hello"});
    REQUIRE(wait_until_done(pr, 5s));
    CHECK(pr.state() == RunState::Exited);
    CHECK(pr.exit_code() == 0);
    std::string out = pr.take_output();
    CHECK(out.find("hello") != std::string::npos);
}

TEST_CASE("captures the child's exit code") {
    ProcessRunner pr([]{});
    pr.start(Action{.label="x", .cmd="exit 3"});
    REQUIRE(wait_until_done(pr, 5s));
    CHECK(pr.state() == RunState::Exited);
    CHECK(pr.exit_code() == 3);
}

TEST_CASE("merges stderr into the same stream") {
    ProcessRunner pr([]{});
    pr.start(Action{.label="e", .cmd="printf oops 1>&2"});
    REQUIRE(wait_until_done(pr, 5s));
    CHECK(pr.take_output().find("oops") != std::string::npos);
}

TEST_CASE("kill terminates a running process") {
    ProcessRunner pr([]{});
    pr.start(Action{.label="s", .cmd="sleep 30"});
    std::this_thread::sleep_for(100ms);
    REQUIRE(pr.state() == RunState::Running);
    pr.kill();
    REQUIRE(wait_until_done(pr, 5s));
    CHECK(pr.state() == RunState::Killed);
}

TEST_CASE("kill after natural completion does not change the reported exit") {
    ProcessRunner pr([]{});
    pr.start(Action{.label="x", .cmd="exit 7"});
    REQUIRE(wait_until_done(pr, 5s));
    REQUIRE(pr.state() == RunState::Exited);
    REQUIRE(pr.exit_code() == 7);
    pr.kill();                       // no-op: not Running anymore
    CHECK(pr.state() == RunState::Exited);
    CHECK(pr.exit_code() == 7);
}

TEST_CASE("start is a no-op while already running") {
    ProcessRunner pr([]{});
    pr.start(Action{.label="s", .cmd="sleep 2"});
    std::this_thread::sleep_for(50ms);
    pr.start(Action{.label="e", .cmd="echo second"});  // ignored
    REQUIRE(wait_until_done(pr, 6s));
    // "second" must never have run.
    CHECK(pr.take_output().find("second") == std::string::npos);
}
