# runner

A TUI project script runner. `runner` reads a `runner.toml` describing a set of
actions, shows them in an interactive terminal list, and runs the selected one
in a child shell while streaming its output — including 256-color and truecolor
ANSI — into a scrolling pane.

Features: per-project action lists with groups, `${VAR}` expansion and per-action
environment injection, upward config search, and a `--generate-config` starter.

## Building from source

Requires a C++20 compiler and CMake ≥ 3.20.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/runner --version
```

Dependencies are resolved **system-first, source-fallback**: if
[FTXUI][ftxui] 5.0.0, [tomlplusplus][toml] 3.4.0, and (for tests)
[doctest][doctest] 2.4.11 are installed, CMake uses them via `find_package`;
otherwise it fetches and builds them with `FetchContent` (needs network at
configure time). Pass `-DRUNNER_BUILD_TESTS=OFF` to skip the test suite (and its
doctest dependency).

Run the tests:

```sh
ctest --test-dir build --output-on-failure
```

## Installing

```sh
sudo cmake --install build --prefix /usr/local
```

Installs the `runner` binary, the `runner.1` man page, and
`share/doc/runner/runner.toml.example`.

## Void Linux package

A ready-to-use xbps-src template lives at [`packaging/void/template`](packaging/void/template).
To build and install it locally:

```sh
git clone --depth 1 https://github.com/void-linux/void-packages.git
cd void-packages
./xbps-src binary-bootstrap
mkdir -p srcpkgs/runner
cp /path/to/runner/packaging/void/template srcpkgs/runner/template
./xbps-src pkg runner
sudo xbps-install -R hostdir/binpkgs runner
```

The template pulls FTXUI's release tarball as a second distfile and builds it
offline (static-linked); tomlplusplus and doctest come from Void's
`tomlplusplus-devel` / `doctest-devel`.

## Usage

```sh
runner                    # search upward for runner.toml and open the UI
runner -c path/to.toml    # use a specific config
runner --generate-config  # write a starter runner.toml here
runner --help
```

See `man 1 runner` for the full `runner.toml` schema.

## License

[MIT](LICENSE)

[ftxui]: https://github.com/ArthurSonzogni/FTXUI
[toml]: https://github.com/marzer/tomlplusplus
[doctest]: https://github.com/doctest/doctest
