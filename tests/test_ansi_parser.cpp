#include <doctest/doctest.h>
#include "ansi_parser.hpp"

TEST_CASE("plain text without escapes becomes one span per line") {
    AnsiParser p;
    p.feed("hello\nworld\n");
    REQUIRE(p.lines().size() == 2);
    REQUIRE(p.lines()[0].size() == 1);
    CHECK(p.lines()[0][0].text == "hello");
    CHECK(p.lines()[0][0].fg == -1);
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
    CHECK(line[0].fg == 1);            // 31 -> index 1
    CHECK(line[1].text == "normal");
    CHECK(line[1].fg == -1);           // reset
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
    CHECK(p.lines()[0][0].fg == 10);
}

TEST_CASE("background color is tracked separately") {
    AnsiParser p;
    p.feed("\x1b[44mB\x1b[49mn\n");  // 44 -> bg 4 ; 49 -> reset bg
    const StyledLine& line = p.lines()[0];
    CHECK(line[0].bg == 4);
    CHECK(line[1].bg == -1);
}

TEST_CASE("an escape split across two feeds is handled") {
    AnsiParser p;
    p.feed("\x1b[3");
    p.feed("1mred\n");
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 1);
    CHECK(line[0].text == "red");
    CHECK(line[0].fg == 1);
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
    CHECK(line[0].fg == 1);
    CHECK(line[1].text == "plain");
    CHECK(line[1].fg == -1);
}

TEST_CASE("empty SGR parameter is treated as 0 (reset)") {
    AnsiParser p;
    p.feed("\x1b[1m\x1b[;31mX\n");   // ";31" -> [0,31]: reset clears bold, then red
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 1);
    CHECK(line[0].fg == 1);
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
    CHECK(p.lines()[0].back().fg == 1);   // oversized param ignored, red preserved (not reset)
}

TEST_CASE("256-color foreground is consumed and ignored") {
    AnsiParser p;
    p.feed("\x1b[38;5;31mX\n");   // the 31 must NOT leak as red
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 1);
    CHECK(line[0].text == "X");
    CHECK(line[0].fg == -1);
}

TEST_CASE("truecolor foreground is consumed and ignored") {
    AnsiParser p;
    p.feed("\x1b[38;2;0;255;0mY\x1b[0m\n");
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 1);
    CHECK(line[0].text == "Y");
    CHECK(line[0].fg == -1);
    CHECK(line[0].bg == -1);
}

TEST_CASE("256-color background is consumed and ignored") {
    AnsiParser p;
    p.feed("\x1b[48;5;40mZ\n");
    const StyledLine& line = p.lines()[0];
    REQUIRE(line.size() == 1);
    CHECK(line[0].bg == -1);
}

TEST_CASE("AnsiParser caps retained scrollback to max_lines") {
    AnsiParser p(3);
    p.feed("a\nb\nc\nd\ne\n");
    REQUIRE(p.lines().size() == 3);       // capped to 3
    CHECK(p.lines()[0][0].text == "c");   // oldest (a,b) evicted
    CHECK(p.lines()[2][0].text == "e");   // newest retained
}
