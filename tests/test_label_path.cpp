#include <doctest/doctest.h>
#include "label_path.hpp"

using V = std::vector<std::string>;

TEST_CASE("split_label splits on slash") {
    CHECK(split_label("Dev/Build") == V{"Dev", "Build"});
    CHECK(split_label("Packaging/Arch/build") == V{"Packaging", "Arch", "build"});
}

TEST_CASE("split_label trims surrounding whitespace per segment") {
    CHECK(split_label(" Packaging / Arch ") == V{"Packaging", "Arch"});
    CHECK(split_label("\tDev\t/\tTest\t") == V{"Dev", "Test"});
}

TEST_CASE("split_label preserves internal whitespace") {
    CHECK(split_label("Packaging/Arch/build package") ==
          V{"Packaging", "Arch", "build package"});
}

TEST_CASE("split_label of a plain name yields one segment") {
    CHECK(split_label("Build") == V{"Build"});
}

TEST_CASE("split_label keeps empty segments for malformed input") {
    CHECK(split_label("") == V{""});
    CHECK(split_label("a//b") == V{"a", "", "b"});
    CHECK(split_label("/b") == V{"", "b"});
    CHECK(split_label("a/") == V{"a", ""});
    CHECK(split_label("   ") == V{""});  // whitespace-only trims to empty
}
