#include "app.hpp"
#include "cli.hpp"
#include "config.hpp"

#include <exception>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  try {
    CliOptions opts = parse_cli(argc, argv);

    if (opts.error) {
        std::cerr << "runner: " << opts.message << "\n\n" << help_text();
        return 2;
    }
    if (opts.show_version) {
        std::cout << version_text();
        return 0;
    }
    if (opts.show_help) {
        std::cout << help_text();
        return 0;
    }
    if (opts.generate_config) {
        std::string message;
        int rc = generate_config_file(fs::current_path(), message);
        (rc == 0 ? std::cout : std::cerr) << "runner: " << message << "\n";
        return rc;
    }

    fs::path config_path;
    if (opts.config_path) {
        config_path = *opts.config_path;
        if (!fs::is_regular_file(config_path)) {
            std::cerr << "runner: config not found: " << config_path.string() << "\n";
            return 1;
        }
    } else {
        auto found = find_config(fs::current_path());
        if (!found) {
            std::cerr << "runner: no runner.toml found in this directory or any "
                         "parent.\nRun `runner --generate-config` to create one.\n";
            return 1;
        }
        config_path = *found;
    }

    ParseResult result = load_config(config_path);
    if (!result.errors.empty()) {
        std::cerr << "runner: problems in " << config_path.string() << ":\n";
        for (const auto& e : result.errors) std::cerr << "  - " << e << "\n";
        return 1;
    }

    App app(std::move(result.actions));
    app.run();
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "runner: " << e.what() << "\n";
    return 1;
  }
}
