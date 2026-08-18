#include <doctest/doctest.h>
#include "ansi_parser.hpp"

TEST_CASE("plain text without escapes becomes one span per line") {
    AnsiParser p;
    p.feed("hello\nworld\n");
    REQUIRE(p.lines().size() == 2);
    REQUIRE(p.lines()[0].size() == 1);
    CHECK(p.lines()[0][0].text == "hello");
    CHECK(p.lines()[0][0].fg == Color{});
    CHECK(p.lines()[1][0].text == "world");
}

TEST_CASE("a partial line is exposed via pending_line") {
    AnsiParser p;
    p.feed("partial");
    CHECK(p.lines().empty());
    REQUIRE(p.pending_line().size() == 1);
    CHECK(p.pending_line()[0].text == "partial");
}

TEST_CASE("SGR sets foreground color for following text") {
    AnsiParser p;
    p.feed("\x1b[31mred\x1b[0mnormal\n");
    REQUIRE(p.lines().size() == 1);
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 2);
    CHECK(line[0].text == "red");
    CHECK(line[0].fg == Color::palette16(1));   // 31 -> index 1
    CHECK(line[1].text == "normal");
    CHECK(line[1].fg == Color{});               // reset
}

TEST_CASE("bold flag is tracked and cleared") {
    AnsiParser p;
    p.feed("\x1b[1mB\x1b[22mn\n");
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 2);
    CHECK(line[0].bold == true);
    CHECK(line[1].bold == false);
}

TEST_CASE("bright foreground colors map to indices 8..15") {
    AnsiParser p;
    p.feed("\x1b[92mX\n");   // 92 -> 8 + (92-90) = 10
    CHECK(p.lines()[0][0].fg == Color::palette16(10));
}

TEST_CASE("background color is tracked separately") {
    AnsiParser p;
    p.feed("\x1b[44mB\x1b[49mn\n");  // 44 -> bg 4 ; 49 -> reset bg
    const StyledLine& line = p.lines()[0];
    CHECK(line[0].bg == Color::palette16(4));
    CHECK(line[1].bg == Color{});
}

TEST_CASE("an escape split across two feeds is handled") {
    AnsiParser p;
    p.feed("\x1b[3");
    p.feed("1mred\n");
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 1);
    CHECK(line[0].text == "red");
    CHECK(line[0].fg == Color::palette16(1));
}

TEST_CASE("carriage returns are stripped") {
    AnsiParser p;
    p.feed("ab\r\n");
    REQUIRE(p.lines().size() == 1);
    CHECK(p.lines()[0][0].text == "ab");
}

TEST_CASE("unsupported CSI sequences are consumed and ignored") {
    AnsiParser p;
    p.feed("\x1b[2Kclean\n");  // erase-line CSI, not SGR
    REQUIRE(p.lines().size() == 1);
    REQUIRE(p.lines()[0].size() == 1);
    CHECK(p.lines()[0][0].text == "clean");
}

TEST_CASE("clear resets everything") {
    AnsiParser p;
    p.feed("\x1b[31mred\n");
    p.clear();
    CHECK(p.lines().empty());
    CHECK(p.pending_line().empty());
}

TEST_CASE("bare CSI m resets styles") {
    AnsiParser p;
    p.feed("\x1b[31mred\x1b[mplain\n");
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 2);
    CHECK(line[0].fg == Color::palette16(1));
    CHECK(line[1].text == "plain");
    CHECK(line[1].fg == Color{});
}

TEST_CASE("empty SGR parameter is treated as 0 (reset)") {
    AnsiParser p;
    p.feed("\x1b[1m\x1b[;31mX\n");   // ";31" -> [0,31]: reset clears bold, then red
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 1);
    CHECK(line[0].fg == Color::palette16(1));
    CHECK(line[0].bold == false);
}

TEST_CASE("malformed SGR parameters do not crash and text is preserved") {
    AnsiParser p;
    p.feed("\x1b[31mred\x1b[Xmstill\x1b[999999999999mmore\n");
    REQUIRE(p.lines().size() == 1);
    std::string all;
    for (const auto& s : p.lines()[0]) all += s.text;
    // \x1b[X is a consumed unsupported CSI (m becomes literal); the oversized
    // numeric param is ignored (no crash, style preserved), never applied.
    CHECK(all == "redmstillmore");
    CHECK(p.lines()[0].back().fg == Color::palette16(1));  // oversized param ignored, red preserved
}

TEST_CASE("256-color foreground sets a Palette256 color") {
    AnsiParser p;
    p.feed("\x1b[38;5;196mX\n");
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 1);
    CHECK(line[0].text == "X");
    CHECK(line[0].fg == Color::palette256(196));
}

TEST_CASE("truecolor foreground sets an RGB color") {
    AnsiParser p;
    p.feed("\x1b[38;2;255;128;0mY\x1b[0m\n");
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 1);
    CHECK(line[0].text == "Y");
    CHECK(line[0].fg == Color::rgb(255, 128, 0));
}

TEST_CASE("256-color background sets a Palette256 background") {
    AnsiParser p;
    p.feed("\x1b[48;5;21mZ\n");
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 1);
    CHECK(line[0].bg == Color::palette256(21));
}

TEST_CASE("AnsiParser caps retained scrollback to max_lines") {
    AnsiParser p(3);
    p.feed("a\nb\nc\nd\ne\n");
    REQUIRE(p.lines().size() == 3);       // capped to 3
    CHECK(p.lines()[0][0].text == "c");   // oldest (a,b) evicted
    CHECK(p.lines()[2][0].text == "e");   // newest retained
}

TEST_CASE("a style code after a consumed 256-color payload still applies") {
    AnsiParser p;
    p.feed("\x1b[38;5;1;1mX\n");   // 38;5;1 sets fg; trailing 1 = bold
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 1);
    CHECK(line[0].text == "X");
    CHECK(line[0].fg == Color::palette256(1));
    CHECK(line[0].bold == true);
}

TEST_CASE("unknown extended color mode is consumed without leaking") {
    AnsiParser p;
    p.feed("\x1b[38;9mX\n");        // mode 9 unknown: consume the mode token, apply nothing
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 1);
    CHECK(line[0].text == "X");
    CHECK(line[0].fg == Color{});
}

TEST_CASE("truncated 38;5 (missing index) applies no color") {
    AnsiParser p;
    p.feed("\x1b[38;5mX\n");
    CHECK(p.lines()[0][0].fg == Color{});
}

TEST_CASE("bare 38 (missing mode) applies no color") {
    AnsiParser p;
    p.feed("\x1b[38mX\n");
    CHECK(p.lines()[0][0].fg == Color{});
}

TEST_CASE("out-of-range 256 index applies no color") {
    AnsiParser p;
    p.feed("\x1b[38;5;300mX\n");   // 300 > 255
    CHECK(p.lines()[0][0].fg == Color{});
}

TEST_CASE("short RGB (fewer than three components) applies no color") {
    AnsiParser p;
    p.feed("\x1b[38;2;1;2mX\n");
    CHECK(p.lines()[0][0].fg == Color{});
}

TEST_CASE("out-of-range RGB components are clamped to 0..255") {
    AnsiParser p;
    p.feed("\x1b[38;2;999;0;0mX\n");
    CHECK(p.lines()[0][0].fg == Color::rgb(255, 0, 0));
}
