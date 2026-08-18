#include "ansi_parser.hpp"
#include <charconv>
#include <sstream>

AnsiParser::AnsiParser(std::size_t max_lines) : max_lines_(max_lines) {}

void AnsiParser::push_span_if_any() {
    if (!cur_span_.text.empty()) {
        cur_line_.push_back(cur_span_);
        cur_span_.text.clear();
    }
}

void AnsiParser::flush_line() {
    push_span_if_any();
    lines_.push_back(cur_line_);
    cur_line_.clear();
    while (lines_.size() > max_lines_) lines_.pop_front();
}

void AnsiParser::apply_sgr(const std::string& params) {
    // Any style change starts a new span.
    push_span_if_any();

    std::vector<int> codes;
    std::stringstream ss(params);
    std::string tok;
    while (std::getline(ss, tok, ';')) {
        if (tok.empty()) { codes.push_back(0); continue; }  // empty param == 0
        int val = 0;
        auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), val);
        if (ec == std::errc()) codes.push_back(val);
        // Non-numeric / out-of-range params are ignored (never crash).
    }
    if (params.empty()) codes.push_back(0);  // bare ESC[m == reset; non-empty-but-unparseable params are ignored (no-op)

    for (size_t i = 0; i < codes.size(); ++i) {
        int c = codes[i];
        // 256-color / truecolor are out of scope for v1: consume the extended
        // sub-parameters and apply no style, so they don't leak as 16-color codes.
        if (c == 38 || c == 48) {
            if (i + 1 < codes.size()) {
                int mode = codes[i + 1];
                if (mode == 5)      i += 2;  // 38;5;N
                else if (mode == 2) i += 4;  // 38;2;R;G;B
                else                i += 1;  // unknown extended mode
            }
            continue;
        }
        if (c == 0)                    { fg_ = Color{}; bg_ = Color{}; bold_ = false; }
        else if (c == 1)               { bold_ = true; }
        else if (c == 22)              { bold_ = false; }
        else if (c >= 30 && c <= 37)   { fg_ = Color::palette16(static_cast<std::uint8_t>(c - 30)); }
        else if (c >= 90 && c <= 97)   { fg_ = Color::palette16(static_cast<std::uint8_t>(8 + (c - 90))); }
        else if (c == 39)              { fg_ = Color{}; }
        else if (c >= 40 && c <= 47)   { bg_ = Color::palette16(static_cast<std::uint8_t>(c - 40)); }
        else if (c >= 100 && c <= 107) { bg_ = Color::palette16(static_cast<std::uint8_t>(8 + (c - 100))); }
        else if (c == 49)              { bg_ = Color{}; }
        // other codes ignored
    }
}

void AnsiParser::feed(const std::string& bytes) {
    for (char c : bytes) {
        if (in_escape_) {
            esc_buf_ += c;
            if (esc_buf_.size() == 1) {
                // Only CSI (ESC '[') is supported; drop any other escape type.
                if (c != '[') { in_escape_ = false; esc_buf_.clear(); }
                continue;
            }
            // CSI final byte is in range 0x40..0x7E.
            if (c >= 0x40 && c <= 0x7E) {
                if (c == 'm') {
                    // params are between '[' and the final 'm'
                    apply_sgr(esc_buf_.substr(1, esc_buf_.size() - 2));
                }
                in_escape_ = false;
                esc_buf_.clear();
            }
            continue;
        }

        if (c == 0x1B) { in_escape_ = true; esc_buf_.clear(); continue; }
        if (c == '\r') continue;
        if (c == '\n') { flush_line(); continue; }

        // Lazily stamp the current style onto a fresh span.
        if (cur_span_.text.empty()) {
            cur_span_.fg = fg_; cur_span_.bg = bg_; cur_span_.bold = bold_;
        }
        cur_span_.text += c;
    }
}

StyledLine AnsiParser::pending_line() const {
    StyledLine line = cur_line_;
    if (!cur_span_.text.empty()) line.push_back(cur_span_);
    return line;
}

void AnsiParser::clear() {
    lines_.clear();
    cur_line_.clear();
    cur_span_ = StyledSpan{};
    fg_ = Color{}; bg_ = Color{}; bold_ = false;
    in_escape_ = false; esc_buf_.clear();
}
