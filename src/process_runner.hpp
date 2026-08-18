#pragma once
#include "action.hpp"
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

enum class RunState { Idle, Running, Exited, Killed };

class ProcessRunner {
public:
    explicit ProcessRunner(std::function<void()> notify);
    ~ProcessRunner();

    ProcessRunner(const ProcessRunner&) = delete;
    ProcessRunner& operator=(const ProcessRunner&) = delete;

    // Not thread-safe against concurrent callers: call start()/kill() from a single (UI/event-loop) thread.
    void start(const Action& action);  // no-op while Running
    void kill();                       // no-op unless Running

    RunState state() const { return state_.load(); }
    int exit_code() const { return exit_code_.load(); }
    std::string take_output();         // returns & clears newly-read bytes

private:
    void read_loop(int read_fd);

    std::function<void()> notify_;
    std::atomic<RunState> state_{RunState::Idle};
    std::atomic<int> exit_code_{0};
    std::atomic<pid_t> child_pid_{-1};
    std::atomic<bool> kill_requested_{false};

    std::mutex buf_mutex_;
    std::string buffer_;               // guarded output not yet taken

    std::jthread reader_;
    std::jthread killer_;
};
