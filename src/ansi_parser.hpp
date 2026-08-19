#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

struct Color {
    enum class Kind : std::uint8_t { Default, Palette16, Palette256, Rgb };
    Kind         kind  = Kind::Default;
    std::uint8_t index = 0;              // Palette16: 0..15   Palette256: 0..255
    std::uint8_t r = 0, g = 0, b = 0;    // Rgb

    static Color palette16(std::uint8_t i)  { return Color{Kind::Palette16, i, 0, 0, 0}; }
    static Color palette256(std::uint8_t i) { return Color{Kind::Palette256, i, 0, 0, 0}; }
    static Color rgb(std::uint8_t rr, std::uint8_t gg, std::uint8_t bb) {
        return Color{Kind::Rgb, 0, rr, gg, bb};
    }

    friend bool operator==(const Color&, const Color&) = default;
};

struct StyledSpan {
    std::string text;
    Color fg{};      // default-constructed => Kind::Default
    Color bg{};
    bool  bold = false;
};

using StyledLine = std::vector<StyledSpan>;

// Parses a byte stream (possibly split mid-escape / mid-line) into styled lines.
// Supports 16-color SGR foreground/background, bold, and reset only.
class AnsiParser {
public:
    explicit AnsiParser(std::size_t max_lines = 10000);

    void feed(const std::string& bytes);
    const std::deque<StyledLine>& lines() const { return lines_; }
    StyledLine pending_line() const;
    // All buffered output as plain text (styling stripped): each completed line
    // followed by '\n', then the in-progress line (no trailing newline).
    std::string plain_text() const;
    void clear();

private:
    void apply_sgr(const std::string& params);  // params between '[' and 'm'
    void push_span_if_any();                     // flush cur_span_ into cur_line_
    void flush_line();                           // finish current line

    std::deque<StyledLine> lines_;
    std::size_t max_lines_;
    StyledLine  cur_line_;
    StyledSpan  cur_span_;     // style applied lazily on first char
    Color fg_{}, bg_{};
    bool bold_ = false;

    bool in_escape_ = false;   // seen ESC, collecting the sequence
    std::string esc_buf_;      // sequence bytes after ESC (starts with '[')
};
