#include "clipboard.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

std::string base64_encode(const std::string& in) {
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    std::size_t i = 0;
    for (; i + 3 <= in.size(); i += 3) {
        std::uint32_t n = (static_cast<unsigned char>(in[i]) << 16) |
                          (static_cast<unsigned char>(in[i + 1]) << 8) |
                          static_cast<unsigned char>(in[i + 2]);
        out += tbl[(n >> 18) & 0x3F];
        out += tbl[(n >> 12) & 0x3F];
        out += tbl[(n >> 6) & 0x3F];
        out += tbl[n & 0x3F];
    }
    if (std::size_t rem = in.size() - i; rem > 0) {
        std::uint32_t n = static_cast<unsigned char>(in[i]) << 16;
        if (rem == 2) n |= static_cast<unsigned char>(in[i + 1]) << 8;
        out += tbl[(n >> 18) & 0x3F];
        out += tbl[(n >> 12) & 0x3F];
        out += (rem == 2) ? tbl[(n >> 6) & 0x3F] : '=';
        out += '=';
    }
    return out;
}

// Feed `text` to an external command's stdin. Returns true if the command was
// spawned and exited successfully. Best-effort: never throws.
bool pipe_to_command(const std::vector<std::string>& argv, const std::string& text) {
    if (argv.empty()) return false;

    int fds[2];
    if (pipe(fds) != 0) return false;

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return false;
    }
    if (pid == 0) {
        // Child: stdin from pipe, silence stdout/stderr, then exec.
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);
        close(fds[1]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        std::vector<char*> c_argv;
        c_argv.reserve(argv.size() + 1);
        for (const auto& a : argv) c_argv.push_back(const_cast<char*>(a.c_str()));
        c_argv.push_back(nullptr);
        execvp(c_argv[0], c_argv.data());
        _exit(127);  // exec failed (tool not installed)
    }

    // Parent: write the payload, close, reap.
    close(fds[0]);
    std::size_t off = 0;
    while (off < text.size()) {
        ssize_t n = write(fds[1], text.data() + off, text.size() - off);
        if (n <= 0) break;
        off += static_cast<std::size_t>(n);
    }
    close(fds[1]);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

std::string env_or_empty(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

}  // namespace

std::string osc52_sequence(const std::string& text) {
    // ESC ] 52 ; c ; <base64> BEL  — 'c' targets the clipboard selection.
    return "\033]52;c;" + base64_encode(text) + "\a";
}

std::vector<std::string> clipboard_tool_command(const std::string& wayland_display,
                                                const std::string& x11_display,
                                                bool is_macos) {
    if (is_macos) return {"pbcopy"};
    if (!wayland_display.empty()) return {"wl-copy"};
    if (!x11_display.empty()) return {"xclip", "-selection", "clipboard"};
    return {};
}

void copy_to_clipboard(const std::string& text) {
#ifdef __APPLE__
    constexpr bool kMacos = true;
#else
    constexpr bool kMacos = false;
#endif

    // Transport 1: OSC 52 straight to the controlling terminal.
    if (int tty = open("/dev/tty", O_WRONLY); tty >= 0) {
        std::string seq = osc52_sequence(text);
        std::size_t off = 0;
        while (off < seq.size()) {
            ssize_t n = write(tty, seq.data() + off, seq.size() - off);
            if (n <= 0) break;
            off += static_cast<std::size_t>(n);
        }
        close(tty);
    }

    // Transport 2: external tool. On X11, fall back xclip -> xsel if the
    // primary choice isn't installed.
    std::vector<std::string> argv =
        clipboard_tool_command(env_or_empty("WAYLAND_DISPLAY"),
                               env_or_empty("DISPLAY"), kMacos);
    if (!pipe_to_command(argv, text) && !kMacos &&
        env_or_empty("WAYLAND_DISPLAY").empty() && !env_or_empty("DISPLAY").empty()) {
        pipe_to_command({"xsel", "--clipboard", "--input"}, text);
    }
}
