#pragma once
#include "action.hpp"
#include <vector>

class App {
public:
    explicit App(std::vector<Action> actions);
    void run();  // blocks until the user quits (Ctrl+D)

private:
    std::vector<Action> actions_;
};
