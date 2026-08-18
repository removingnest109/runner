#include "config.hpp"
#include <toml++/toml.hpp>
#include <string>
#include <fstream>
#include <sstream>

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
        bool complete = true;
        if (auto v = (*t)["label"].value<std::string>()) a.label = *v;
        else { r.errors.push_back("action #" + std::to_string(idx) + ": missing 'label'"); complete = false; }
        if (auto v = (*t)["cmd"].value<std::string>()) a.cmd = *v;
        else { r.errors.push_back("action #" + std::to_string(idx) + ": missing 'cmd'"); complete = false; }
        if (!complete) continue;   // errors recorded; don't push an incomplete action
        a.desc  = (*t)["desc"].value_or(std::string{});
        a.cwd   = (*t)["cwd"].value_or(std::string{});
        a.group = (*t)["group"].value_or(std::string{});
        r.actions.push_back(std::move(a));
    }
    return r;
}

namespace fs = std::filesystem;

std::optional<fs::path> find_config(fs::path start_dir) {
    std::error_code ec;
    fs::path dir = fs::absolute(start_dir, ec);
    while (true) {
        fs::path candidate = dir / "runner.toml";
        if (fs::is_regular_file(candidate, ec)) return candidate;
        if (dir == dir.root_path()) break;
        dir = dir.parent_path();
    }
    return std::nullopt;
}

ParseResult load_config(const fs::path& config_path) {
    std::ifstream in(config_path);
    if (!in) {
        ParseResult r;
        r.errors.push_back("cannot read config file: " + config_path.string());
        return r;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    ParseResult r = parse_config(ss.str());
    if (!r.errors.empty()) return r;

    fs::path base = config_path.parent_path();
    std::error_code ec;
    for (auto& a : r.actions) {
        fs::path dir;
        if (a.cwd.empty())                 dir = base;
        else if (fs::path(a.cwd).is_absolute()) dir = a.cwd;
        else                               dir = base / a.cwd;

        if (!fs::is_directory(dir, ec)) {
            r.errors.push_back("action '" + a.label +
                               "': cwd is not a directory: " + dir.string());
        } else {
            a.cwd = fs::weakly_canonical(dir, ec).string();
        }
    }
    return r;
}
