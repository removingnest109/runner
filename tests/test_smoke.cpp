#include <doctest/doctest.h>
#include "action.hpp"

TEST_CASE("Action default-constructs with empty fields") {
    Action a;
    CHECK(a.label.empty());
    CHECK(a.cmd.empty());
    CHECK(a.group.empty());
}
