#include "app.hpp"
#include "ansi_parser.hpp"
#include "clipboard.hpp"
#include "process_runner.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include <termios.h>
#include <unistd.h>

// Targeted using-declarations rather than `using namespace ftxui`: FTXUI 7
// introduces ftxui::App, which would collide with this project's ::App class
// under a blanket directive. Colors are still qualified as ftxui::Color in
// to_ftxui() below (::Color is our own type).
using ftxui::CatchEvent;
using ftxui::Element;
using ftxui::Elements;
using ftxui::Event;
using ftxui::Mouse;
using ftxui::Renderer;
using ftxui::ScreenInteractive;
using ftxui::bgcolor;
using ftxui::bold;
using ftxui::color;
using ftxui::EQUAL;
using ftxui::flex;
using ftxui::focus;
using ftxui::frame;
using ftxui::hbox;
using ftxui::inverted;
using ftxui::separator;
using ftxui::size;
using ftxui::text;
using ftxui::vbox;
using ftxui::WIDTH;
using ftxui::yframe;

namespace {

// Restores the terminal's original line discipline on scope exit — even if
// screen.Loop() throws. FTXUI's own uninstall only restores to the state it
// saw at Install() time (our ISIG-cleared copy), not the true original, so we
// must guarantee our own restore runs on every exit path.
class TermiosGuard {
public:
    TermiosGuard() { valid_ = (tcgetattr(STDIN_FILENO, &saved_) == 0); }
    ~TermiosGuard() { if (valid_) tcsetattr(STDIN_FILENO, TCSANOW, &saved_); }
    bool valid() const { return valid_; }
    const termios& saved() const { return saved_; }
    TermiosGuard(const TermiosGuard&) = delete;
    TermiosGuard& operator=(const TermiosGuard&) = delete;
private:
    termios saved_{};
    bool valid_ = false;
};

// Map our neutral color to an FTXUI color at render time — the only place FTXUI
// colors are constructed. Palette16 keeps the named colors (theme-aware).
ftxui::Color to_ftxui(const ::Color& c) {
    using K = ::Color::Kind;
    switch (c.kind) {
        case K::Palette16: {
            static const ftxui::Color table[16] = {
                ftxui::Color::Black,     ftxui::Color::Red,          ftxui::Color::Green,
                ftxui::Color::Yellow,    ftxui::Color::Blue,         ftxui::Color::Magenta,
                ftxui::Color::Cyan,      ftxui::Color::GrayLight,    ftxui::Color::GrayDark,
                ftxui::Color::RedLight,  ftxui::Color::GreenLight,   ftxui::Color::YellowLight,
                ftxui::Color::BlueLight, ftxui::Color::MagentaLight, ftxui::Color::CyanLight,
                ftxui::Color::White,
            };
            return table[c.index <= 15 ? c.index : 0];
        }
        case K::Palette256:
            return ftxui::Color(static_cast<ftxui::Color::Palette256>(c.index));
        case K::Rgb:
            return ftxui::Color::RGB(c.r, c.g, c.b);
        case K::Default:
        default:
            return ftxui::Color::Default;
    }
}

Element span_to_element(const StyledSpan& s) {
    Element e = text(s.text);
    if (s.fg.kind != ::Color::Kind::Default) e = e | color(to_ftxui(s.fg));
    if (s.bg.kind != ::Color::Kind::Default) e = e | bgcolor(to_ftxui(s.bg));
    if (s.bold) e = e | bold;
    return e;
}

Element line_to_element(const StyledLine& line) {
    if (line.empty()) return text("");
    Elements spans;
    for (const auto& s : line) spans.push_back(span_to_element(s));
    return hbox(std::move(spans));
}

// Display model: actions ordered by group first-appearance, in-group file order.
struct DisplayModel {
    std::vector<Action> ordered;
    std::vector<std::string> group_of;  // group name per ordered action ("" ok)
};

DisplayModel build_display(const std::vector<Action>& actions) {
    DisplayModel m;
    std::vector<std::string> group_order;
    for (const auto& a : actions) {
        std::string g = a.group;  // "" allowed (Ungrouped)
        if (std::find(group_order.begin(), group_order.end(), g) == group_order.end())
            group_order.push_back(g);
    }
    for (const auto& g : group_order) {
        for (const auto& a : actions) {
            if (a.group == g) { m.ordered.push_back(a); m.group_of.push_back(g); }
        }
    }
    return m;
}

std::string state_label(RunState st, int code) {
    switch (st) {
        case RunState::Idle:    return "idle";
        case RunState::Running: return "● running";
        case RunState::Killed:  return "✕ killed";
        case RunState::Exited:  return (code == 0 ? "✓ exited 0"
                                                  : "✗ exited " + std::to_string(code));
    }
    return "";
}

}  // namespace

App::App(std::vector<Action> actions) : actions_(std::move(actions)) {}

void App::run() {
    auto screen = ScreenInteractive::Fullscreen();

    // FTXUI 6+ runs its own Ctrl-C / Ctrl-Z handlers by default even when a
    // component catches the event (see screen_interactive.hpp). That would quit
    // the app on Ctrl-C, overriding our "Ctrl+C kills the running child" below.
    // Force FTXUI to leave these to our CatchEvent handler.
    screen.ForceHandleCtrlC(false);
    screen.ForceHandleCtrlZ(false);

    // FTXUI v5.0.0's raw-mode setup (ScreenInteractive::Install) clears
    // ICANON/ECHO but leaves ISIG enabled, and installs its own SIGINT
    // handler that treats Ctrl+C as "quit the whole app" (see
    // screen_interactive.cpp: RecordSignal/Signal(SIGABRT) -> OnExit()).
    // That fires before the raw 0x03 byte would ever reach our CatchEvent
    // handler below, so plain "kill just the running child" Ctrl+C could
    // never work.
    //
    // Let raw Ctrl+C (0x03) reach FTXUI as a keystroke instead of the tty
    // turning it into SIGINT. Clearing ISIG also disables Ctrl+Z/Ctrl+\ for
    // the app's runtime — an accepted tradeoff for a full-screen TUI. The
    // guard restores the original discipline on every exit path (normal
    // return OR exception out of screen.Loop()).
    TermiosGuard term_guard;
    if (term_guard.valid()) {
        termios raw = term_guard.saved();
        raw.c_lflag &= ~static_cast<tcflag_t>(ISIG);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }

    DisplayModel model = build_display(actions_);

    AnsiParser parser;
    ProcessRunner runner([&] { screen.PostEvent(Event::Custom); });

    int selected = 0;          // index into model.ordered
    bool follow = true;        // auto-scroll output to bottom
    int scroll_line = 0;       // used when not following
    int total_lines = 0;       // output line count from the last render (for wheel)
    std::string copied_msg;    // transient status feedback after a copy

    // FTXUI v5.0.0's Event has no named Event::CtrlC/Event::CtrlD constants;
    // per ftxui/component/event.cpp these map to raw control bytes 3 and 4.
    const Event ctrl_c = Event::Special(std::string(1, static_cast<char>(3)));
    const Event ctrl_d = Event::Special(std::string(1, static_cast<char>(4)));
    const Event ctrl_y = Event::Special(std::string(1, static_cast<char>(25)));

    auto sidebar = Renderer([&] {
        Elements rows;
        std::string last_group = "\x01";  // impossible sentinel
        for (size_t i = 0; i < model.ordered.size(); ++i) {
            const std::string& g = model.group_of[i];
            if (g != last_group) {
                std::string header = g.empty() ? "Ungrouped" : g;
                rows.push_back(text(header) | bold | color(ftxui::Color::GrayLight));
                last_group = g;
            }
            Element row = text((static_cast<int>(i) == selected ? "▶ " : "  ")
                               + model.ordered[i].label);
            if (static_cast<int>(i) == selected) row = row | inverted;
            rows.push_back(row);
        }
        return vbox(std::move(rows)) | frame;
    });

    auto output = Renderer([&] {
        // Drain any new process output on every render pass.
        std::string chunk = runner.take_output();
        if (!chunk.empty()) parser.feed(chunk);

        Elements lines;
        for (const auto& l : parser.lines()) lines.push_back(line_to_element(l));
        StyledLine pend = parser.pending_line();
        if (!pend.empty()) lines.push_back(line_to_element(pend));

        int total = static_cast<int>(lines.size());
        total_lines = total;  // published for the mouse-wheel handler
        if (follow && total > 0) {
            lines.push_back(text("") | focus);  // pin view to bottom
        } else if (total > 0) {
            int idx = std::clamp(scroll_line, 0, total - 1);
            lines[idx] = lines[idx] | focus;
        }
        return vbox(std::move(lines)) | yframe | flex;
    });

    auto layout = ftxui::Container::Horizontal({sidebar, output});

    auto renderer = Renderer(layout, [&] {
        std::string status =
            state_label(runner.state(), runner.exit_code()) +
            "   ↑↓ select · Enter run · Ctrl+C kill · Ctrl+D quit · y copy · Ctrl+Y copy all";
        if (!copied_msg.empty()) status += "   [" + copied_msg + "]";

        std::string desc = model.ordered.empty() ? "" : model.ordered[selected].desc;

        return vbox({
            text(" runner") | bold,
            separator(),
            hbox({
                sidebar->Render() | size(WIDTH, EQUAL, 30),
                separator(),
                output->Render() | flex,
            }) | flex,
            separator(),
            text(desc.empty() ? " " : (" " + model.ordered[selected].label + " — " + desc)),
            separator(),
            text(" " + status),
        });
    });

    auto with_keys = CatchEvent(renderer, [&](Event e) {
        if (e == Event::Custom) return false;  // just triggers a redraw
        copied_msg.clear();                    // any real event dismisses feedback

        // Copy the current mouse selection (drag to select first).
        if (e == Event::Character('y')) {
            std::string sel = screen.GetSelection();
            if (sel.empty()) {
                copied_msg = "nothing selected";
            } else {
                copy_to_clipboard(sel);
                copied_msg = "copied " + std::to_string(sel.size()) + " chars";
            }
            return true;
        }
        // Copy the full output of the last command.
        if (e == ctrl_y) {
            std::string out = parser.plain_text();
            if (out.empty()) {
                copied_msg = "no output to copy";
            } else {
                copy_to_clipboard(out);
                copied_msg = "copied output (" + std::to_string(out.size()) + " chars)";
            }
            return true;
        }

        if (e == Event::ArrowDown || e == Event::Character('j')) {
            if (!model.ordered.empty())
                selected = std::min(selected + 1, (int)model.ordered.size() - 1);
            return true;
        }
        if (e == Event::ArrowUp || e == Event::Character('k')) {
            selected = std::max(selected - 1, 0);
            return true;
        }
        if (e == Event::Return) {
            if (runner.state() != RunState::Running && !model.ordered.empty()) {
                const Action& a = model.ordered[selected];
                parser.clear();
                parser.feed("$ " + a.cmd + "\n");
                follow = true;
                runner.start(a);
            }
            return true;
        }
        if (e == ctrl_c) { runner.kill(); return true; }
        if (e == ctrl_d) {
            if (runner.state() == RunState::Running) runner.kill();
            screen.Exit();
            return true;
        }
        if (e == Event::PageUp)   { follow = false; scroll_line = std::max(scroll_line - 10, 0); return true; }
        if (e == Event::PageDown) { scroll_line += 10; follow = true; return true; }
        if (e == Event::End)      { follow = true; return true; }

        // Mouse wheel scrolls the output. Press/drag/release fall through to
        // FTXUI's built-in selection handler.
        if (e.is_mouse()) {
            if (e.mouse().button == Mouse::WheelUp) {
                if (follow) { follow = false; scroll_line = std::max(total_lines - 1, 0); }
                scroll_line = std::max(scroll_line - 3, 0);
                return true;
            }
            if (e.mouse().button == Mouse::WheelDown) {
                scroll_line += 3;
                follow = (scroll_line >= total_lines - 1);
                return true;
            }
            return false;
        }
        return false;
    });

    screen.Loop(with_keys);
}
