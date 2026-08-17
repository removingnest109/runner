#include "config.hpp"
#include <toml++/toml.hpp>
#include <string>

ParseResult parse_config(const std::string& toml_content) {
    ParseResult r;
    toml::table tbl;
    try {
        tbl = toml::parse(toml_content);
    } catch (const toml::parse_error& e) {
        r.errors.push_back(std::string("TOML parse error: ") + e.description().data());
        return r;
    }

    auto arr = tbl["action"].as_array();
    if (!arr || arr->empty()) {
        r.errors.push_back("no [[action]] entries found");
        return r;
    }

    int idx = 0;
    for (auto& node : *arr) {
        ++idx;
        auto t = node.as_table();
        if (!t) {
            r.errors.push_back("action #" + std::to_string(idx) + ": not a table");
            continue;
        }
        Action a;
        if (auto v = (*t)["label"].value<std::string>()) a.label = *v;
        else r.errors.push_back("action #" + std::to_string(idx) + ": missing 'label'");
        if (auto v = (*t)["cmd"].value<std::string>()) a.cmd = *v;
        else r.errors.push_back("action #" + std::to_string(idx) + ": missing 'cmd'");
        a.desc  = (*t)["desc"].value_or(std::string{});
        a.cwd   = (*t)["cwd"].value_or(std::string{});
        a.group = (*t)["group"].value_or(std::string{});
        r.actions.push_back(std::move(a));
    }
    return r;
}
