#include "cli.hpp"
#include "version.hpp"
#include <fstream>
#include <string>

namespace fs = std::filesystem;

std::string version_text() {
    return "runner " RUNNER_VERSION "\n";
}

std::string help_text() {
    return
        "runner " RUNNER_VERSION " - TUI project script runner\n"
        "\n"
        "Usage: runner [options]\n"
        "\n"
        "Options:\n"
        "  -c, --config PATH   Use PATH instead of searching for runner.toml\n"
        "  --generate-config   Write a starter runner.toml in the current dir\n"
        "  -h, --help          Show this help\n"
        "  -v, --version       Print the version and exit\n"
        "\n"
        "With no options, runner searches upward from the current directory\n"
        "for runner.toml and opens the interactive UI.\n";
}

CliOptions parse_cli(int argc, char** argv) {
    CliOptions o;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            o.show_help = true;
        } else if (arg == "-v" || arg == "--version") {
            o.show_version = true;
        } else if (arg == "--generate-config") {
            o.generate_config = true;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 >= argc) {
                o.error = true;
                o.message = "option " + arg + " requires a PATH argument";
                return o;
            }
            o.config_path = fs::path(argv[++i]);
        } else {
            o.error = true;
            o.message = "unknown option: " + arg;
            return o;
        }
    }
    return o;
}

int generate_config_file(const fs::path& dir, std::string& message) {
    fs::path target = dir / "runner.toml";
    std::error_code ec;
    if (fs::exists(target, ec)) {
        message = "refusing to overwrite existing " + target.string();
        return 1;
    }
    std::ofstream out(target);
    if (!out) {
        message = "cannot write " + target.string();
        return 1;
    }
    out <<
        "# runner.toml - actions for `runner`. Each [[action]] is one runnable command.\n"
        "# A label may nest with '/': \"General/Example\" shows \"Example\" under a\n"
        "# \"General\" heading. depends_on / sequence reference the full label path.\n"
        "[[action]]\n"
        "label = \"General/Example\"\n"
        "cmd   = \"echo hello from runner\"\n"
        "desc  = \"An example action - edit or replace me\"\n";
    message = "wrote " + target.string();
    return 0;
}
