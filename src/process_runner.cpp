#include "process_runner.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>

using namespace std::chrono_literals;

ProcessRunner::ProcessRunner(std::function<void()> notify)
    : notify_(std::move(notify)) {}

ProcessRunner::~ProcessRunner() {
    if (state_.load() == RunState::Running) {
        kill_requested_.store(true);
        pid_t pid = child_pid_.load();
        if (pid > 0) ::kill(-pid, SIGKILL);
    }
    // jthreads auto-join once the reader reaches EOF.
}

std::string ProcessRunner::take_output() {
    std::lock_guard<std::mutex> lock(buf_mutex_);
    std::string out = std::move(buffer_);
    buffer_.clear();
    return out;
}

void ProcessRunner::start(const Action& action) {
    if (state_.load() == RunState::Running) return;

    {
        std::lock_guard<std::mutex> lock(buf_mutex_);
        buffer_.clear();
    }
    kill_requested_.store(false);
    exit_code_.store(0);

    int fds[2];
    if (pipe(fds) != 0) {
        std::lock_guard<std::mutex> lock(buf_mutex_);
        buffer_ += std::string("runner: pipe() failed: ") + std::strerror(errno) + "\n";
        state_.store(RunState::Idle);
        notify_();
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]); close(fds[1]);
        std::lock_guard<std::mutex> lock(buf_mutex_);
        buffer_ += std::string("runner: fork() failed: ") + std::strerror(errno) + "\n";
        state_.store(RunState::Idle);
        notify_();
        return;
    }

    if (pid == 0) {
        // Child: own process group, merge stdout+stderr into the pipe.
        setpgid(0, 0);
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]);
        close(fds[1]);
        if (!action.cwd.empty()) {
            if (chdir(action.cwd.c_str()) != 0) _exit(127);
        }
        for (const auto& [key, value] : action.env) {
            setenv(key.c_str(), value.c_str(), 1);   // overwrite; inherits parent env
        }
        execl("/bin/sh", "sh", "-c", action.cmd.c_str(), (char*)nullptr);
        _exit(127);  // exec failed
    }

    // Parent.
    setpgid(pid, pid);        // race-safe: set here too
    close(fds[1]);
    child_pid_.store(pid);
    state_.store(RunState::Running);
    notify_();

    // The stop_token is intentionally unused: read() blocks and kill() drives
    // EOF via the signal, so the reader ends naturally when the pipe closes.
    reader_ = std::jthread([this, read_fd = fds[0]](std::stop_token) {
        read_loop(read_fd);
    });
}

void ProcessRunner::read_loop(int read_fd) {
    char buf[4096];
    while (true) {
        ssize_t n = read(read_fd, buf, sizeof(buf));
        if (n > 0) {
            {
                std::lock_guard<std::mutex> lock(buf_mutex_);
                buffer_.append(buf, static_cast<size_t>(n));
            }
            notify_();
            continue;
        }
        if (n < 0 && errno == EINTR) continue;  // interrupted read, retry
        break;                                   // EOF (0) or read error
    }
    close(read_fd);

    int status = 0;
    waitpid(child_pid_.load(), &status, 0);

    // A late kill() (issued just as the child exited on its own) sets
    // kill_requested_, but the child still reaps as WIFEXITED and its SIGTERM
    // was a no-op. Only report Killed when we asked to kill AND the child was
    // actually terminated by a signal; otherwise report the true exit status.
    if (kill_requested_.load() && WIFSIGNALED(status)) {
        state_.store(RunState::Killed);
    } else if (WIFEXITED(status)) {
        exit_code_.store(WEXITSTATUS(status));
        state_.store(RunState::Exited);
    } else if (WIFSIGNALED(status)) {
        exit_code_.store(128 + WTERMSIG(status));
        state_.store(RunState::Exited);
    } else {
        state_.store(RunState::Exited);
    }
    notify_();
}

void ProcessRunner::kill() {
    if (state_.load() != RunState::Running) return;
    kill_requested_.store(true);
    pid_t pid = child_pid_.load();
    if (pid <= 0) return;
    ::kill(-pid, SIGTERM);

    killer_ = std::jthread([this, pid](std::stop_token st) {
        for (int i = 0; i < 20; ++i) {              // ~2s grace
            if (st.stop_requested()) return;
            // Bail if this run ended or a different run has since started.
            if (state_.load() != RunState::Running || child_pid_.load() != pid) return;
            std::this_thread::sleep_for(100ms);
        }
        if (state_.load() == RunState::Running && child_pid_.load() == pid)
            ::kill(-pid, SIGKILL);
    });
}
