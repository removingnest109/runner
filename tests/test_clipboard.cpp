#include <doctest/doctest.h>

#include "clipboard.hpp"

TEST_CASE("osc52_sequence wraps base64 in the clipboard escape") {
    // "hello" -> base64 "aGVsbG8="
    CHECK(osc52_sequence("hello") == "\033]52;c;aGVsbG8=\a");
}

TEST_CASE("osc52_sequence base64 handles all padding cases") {
    CHECK(osc52_sequence("") == "\033]52;c;\a");        // empty
    CHECK(osc52_sequence("f") == "\033]52;c;Zg==\a");   // 1 byte -> 2 pad
    CHECK(osc52_sequence("fo") == "\033]52;c;Zm8=\a");  // 2 bytes -> 1 pad
    CHECK(osc52_sequence("foo") == "\033]52;c;Zm9v\a"); // 3 bytes -> 0 pad
}

TEST_CASE("clipboard_tool_command picks the tool from the environment") {
    SUBCASE("wayland") {
        CHECK(clipboard_tool_command("wayland-0", "", false) ==
              std::vector<std::string>{"wl-copy"});
    }
    SUBCASE("x11") {
        CHECK(clipboard_tool_command("", ":0", false) ==
              std::vector<std::string>{"xclip", "-selection", "clipboard"});
    }
    SUBCASE("macos wins over any display") {
        CHECK(clipboard_tool_command("wayland-0", ":0", true) ==
              std::vector<std::string>{"pbcopy"});
    }
    SUBCASE("wayland preferred over x11 when both set") {
        CHECK(clipboard_tool_command("wayland-0", ":0", false) ==
              std::vector<std::string>{"wl-copy"});
    }
    SUBCASE("headless: nothing applies") {
        CHECK(clipboard_tool_command("", "", false).empty());
    }
}
