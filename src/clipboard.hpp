#pragma once
#include <string>
#include <vector>

// Clipboard integration. Two transports, used together for robustness:
//   1. OSC 52 escape sequence written to the tty — terminal-native, works over
//      ssh/tmux, honored by alacritty (and others) but not by st/plain xterm.
//   2. An external CLI tool (wl-copy / xclip / pbcopy) chosen from the
//      environment — covers terminals that ignore OSC 52.
// Both fire on copy; they write the same content, so last-writer-wins is fine.

// The full OSC 52 clipboard sequence (base64-encoded body) for `text`. Pure.
std::string osc52_sequence(const std::string& text);

// The external clipboard-tool argv chosen from environment hints, or empty if
// none applies. `wayland_display` / `x11_display` are the raw env values ("" if
// unset). macOS (pbcopy) takes precedence when `is_macos`. Pure.
std::vector<std::string> clipboard_tool_command(const std::string& wayland_display,
                                                const std::string& x11_display,
                                                bool is_macos);

// Copy `text` to the system clipboard via both transports. Best-effort: any
// failure (no tty, tool missing) is silent.
void copy_to_clipboard(const std::string& text);
