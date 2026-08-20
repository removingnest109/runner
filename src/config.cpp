#include "config.hpp"
#include "label_path.hpp"
#include "plan.hpp"
#include <toml++/toml.hpp>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>

namespace {

// Canonicalize a path-style label or reference: split on '/', trim each segment,
// rejoin with '/'. Sets `ok = false` (and returns the raw input) if the value is
// empty or has any empty segment ("a//b", "/b", "a/", ""), which the caller
// reports as a load error. Keeps identity and references consistent regardless
// of incidental whitespace, so plan.cpp can match them by exact string.
std::string canonical_path(const std::string& raw, bool& ok) {
    std::vector<std::string> segs = split_label(raw);
    std::string out;
    for (std::size_t i = 0; i < segs.size(); ++i) {
        if (segs[i].empty()) { ok = false; return raw; }
        if (i) out += '/';
        out += segs[i];
    }
    return out;
}

}  // namespace

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
        if (auto v = (*t)["label"].value<std::string>()) {
            // The label carries the full nesting path ("Packaging/Arch/build");
            // canonicalize it and reject empty path segments.
            bool ok = true;
            std::string canon = canonical_path(*v, ok);
            if (!ok) {
                r.errors.push_back("action #" + std::to_string(idx) +
                    ": invalid 'label' '" + *v + "' (empty path segment)");
                complete = false;
            } else {
                a.label = canon;
            }
        } else { r.errors.push_back("action #" + std::to_string(idx) + ": missing 'label'"); complete = false; }

        bool has_cmd = false;
        if (auto v = (*t)["cmd"].value<std::string>()) { a.cmd = *v; has_cmd = true; }

        bool has_seq = false;
        if (auto seq = (*t)["sequence"].as_array()) {
            has_seq = true;
            for (auto& n : *seq) {
                if (auto s = n.value<std::string>()) {
                    bool ok = true;
                    std::string canon = canonical_path(*s, ok);
                    if (!ok) { r.errors.push_back("action #" + std::to_string(idx) +
                        ": invalid 'sequence' reference '" + *s + "' (empty path segment)");
                        complete = false; }
                    else a.sequence.push_back(canon);
                }
                else { r.errors.push_back("action #" + std::to_string(idx) +
                       ": 'sequence' entries must be strings"); complete = false; }
            }
        }

        // A command action has 'cmd'; a composite has 'sequence'. Exactly one.
        if (has_cmd && has_seq) {
            r.errors.push_back("action #" + std::to_string(idx) +
                               ": 'cmd' and 'sequence' are mutually exclusive");
            complete = false;
        } else if (!has_cmd && !has_seq) {
            r.errors.push_back("action #" + std::to_string(idx) +
                               ": missing 'cmd' or 'sequence'");
            complete = false;
        }

        if (!complete) continue;   // errors recorded; don't push an incomplete action
        a.desc  = (*t)["desc"].value_or(std::string{});
        a.cwd   = (*t)["cwd"].value_or(std::string{});
        a.only_if_cmd = (*t)["only_if_cmd"].value_or(std::string{});
        a.hidden = (*t)["hidden"].value_or(false);
        if (a.is_composite() && !a.only_if_cmd.empty())
            r.errors.push_back("action '" + a.label +
                "': 'only_if_cmd' is not allowed on a composite (it owns no command "
                "to gate); put the gate on the member actions instead");
        if (auto deps = (*t)["depends_on"].as_array()) {
            for (auto& n : *deps) {
                if (auto s = n.value<std::string>()) {
                    bool ok = true;
                    std::string canon = canonical_path(*s, ok);
                    if (!ok) r.errors.push_back("action '" + a.label +
                        "': invalid depends_on reference '" + *s + "' (empty path segment)");
                    else a.depends_on.push_back(canon);
                }
                else r.errors.push_back("action '" + a.label +
                     "': depends_on entries must be strings");
            }
        }
        if (auto envtbl = (*t)["env"].as_table()) {
            for (auto&& [k, v] : *envtbl) {
                if (auto s = v.value<std::string>()) {
                    a.env.emplace_back(std::string(k.str()), *s);
                } else {
                    r.errors.push_back("action '" + a.label + "': env var '" +
                                       std::string(k.str()) + "' must be a string");
                }
            }
        }
        r.actions.push_back(std::move(a));
    }

    // Cross-action validation (unique labels, resolvable references, no cycles).
    // Only run when parsing was otherwise clean: a dropped/incomplete action
    // would otherwise produce spurious "unknown reference" noise.
    if (r.errors.empty()) {
        auto graph_errors = validate_graph(r.actions);
        r.errors.insert(r.errors.end(), graph_errors.begin(), graph_errors.end());
    }
    return r;
}

std::string expand_vars(
    const std::string& in,
    const std::function<std::optional<std::string>(std::string_view)>& lookup,
    const std::function<void(std::string_view)>& on_undefined) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size();) {
        if (in[i] == '$' && i + 1 < in.size() && in[i + 1] == '{') {
            std::size_t close = in.find('}', i + 2);
            if (close == std::string::npos) {
                out.append(in, i, std::string::npos);   // unterminated: verbatim
                break;
            }
            std::string_view name(in.data() + i + 2, close - (i + 2));
            if (auto val = lookup(name)) out += *val;
            else                         on_undefined(name);   // expands to ""
            i = close + 1;
        } else {
            out += in[i++];
        }
    }
    return out;
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

    auto env_lookup = [](std::string_view name) -> std::optional<std::string> {
        std::string key(name);
        if (const char* v = std::getenv(key.c_str())) return std::string(v);
        return std::nullopt;
    };

    for (auto& a : r.actions) {
        // Expand ${VAR} in env values.
        for (auto& [k, v] : a.env) {
            v = expand_vars(v, env_lookup, [&](std::string_view name) {
                r.errors.push_back("action '" + a.label +
                    "': undefined variable in env '" + k + "': " + std::string(name));
            });
        }

        // Expand ${VAR} in cwd before resolving it.
        bool cwd_ok = true;
        a.cwd = expand_vars(a.cwd, env_lookup, [&](std::string_view name) {
            r.errors.push_back("action '" + a.label +
                "': undefined variable in cwd: " + std::string(name));
            cwd_ok = false;
        });
        if (!cwd_ok) continue;   // don't resolve a cwd built from an undefined var

        fs::path dir;
        if (a.cwd.empty())                      dir = base;
        else if (fs::path(a.cwd).is_absolute()) dir = a.cwd;
        else                                    dir = base / a.cwd;

        if (!fs::is_directory(dir, ec)) {
            r.errors.push_back("action '" + a.label +
                               "': cwd is not a directory: " + dir.string());
        } else {
            a.cwd = fs::weakly_canonical(dir, ec).string();
        }
    }
    return r;
}
