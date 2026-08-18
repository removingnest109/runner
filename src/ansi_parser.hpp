#pragma once
#include <cstddef>
#include <deque>
#include <string>
#include <vector>

struct StyledSpan {
    std::string text;
    int  fg   = -1;   // -1 default, else 16-color index 0..15
    int  bg   = -1;
    bool bold = false;
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
    void clear();

private:
    void apply_sgr(const std::string& params);  // params between '[' and 'm'
    void push_span_if_any();                     // flush cur_span_ into cur_line_
    void flush_line();                           // finish current line

    std::deque<StyledLine> lines_;
    std::size_t max_lines_;
    StyledLine  cur_line_;
    StyledSpan  cur_span_;     // style applied lazily on first char
    int  fg_ = -1, bg_ = -1;
    bool bold_ = false;

    bool in_escape_ = false;   // seen ESC, collecting the sequence
    std::string esc_buf_;      // sequence bytes after ESC (starts with '[')
};
