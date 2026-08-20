# runner

A TUI project script runner. `runner` reads a `runner.toml` describing a set of
actions, shows them in an interactive terminal list, and runs the selected one
in a child shell while streaming its output — including 256-color and truecolor
ANSI — into a scrolling pane.

Features: per-project action lists with groups, `${VAR}` expansion and per-action
environment injection, upward config search, a `--generate-config` starter, and
mouse selection / clipboard copy of the output pane.

## Install

<details>
<summary><strong>Debian / Ubuntu</strong></summary>

A prebuilt `amd64` `.deb` is attached to every
[release](https://github.com/removingnest109/runner/releases/latest). Download it
and install with `dpkg`:

```sh
sudo dpkg -i runner_*_amd64.deb
```

The package is built against **FTXUI 6+**, so it needs Debian 14 or
Ubuntu 26.10 or newer. If `dpkg` reports unmet
dependencies, pull them in with `sudo apt-get install -f`. To build the `.deb`
yourself, see [`packaging/debian/`](packaging/debian/).

</details>

<details>
<summary><strong>Arch (AUR)</strong></summary>

```sh
yay -S ftxui                       # FTXUI is not in the official repos
cd packaging/aur && makepkg -si    # builds (runs tests) and installs
```

See [`packaging/aur/`](packaging/aur/) for details.

</details>

<details>
<summary><strong>Homebrew</strong></summary>

```sh
brew install --build-from-source ./packaging/homebrew/runner.rb
```

See [`packaging/homebrew/`](packaging/homebrew/), which also covers publishing via
a tap.

</details>

<details>
<summary><strong>Nix</strong></summary>

```sh
nix run github:removingnest109/runner        # run without installing
```

See [`packaging/nix/`](packaging/nix/) for installing into a profile or nixpkgs.

</details>

<details>
<summary><strong>Void</strong></summary>

Build from the `xbps-src` template at
[`packaging/void/`](packaging/void/) (FTXUI is statically linked from its release
tarball, so the build is fully offline).

</details>

<details>
<summary><strong>Building from source</strong></summary>

Requires a C++20 compiler and CMake ≥ 3.20.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/runner --version
```

Dependencies are resolved **system-first, source-fallback**: if
[FTXUI][ftxui] (>= 6.0.0; 6.x or 7.x), [tomlplusplus][toml] 3.4.0, and (for tests)
[doctest][doctest] 2.4.11 are installed, CMake uses them via `find_package`;
otherwise it fetches and builds them with `FetchContent` (needs network at
configure time, FTXUI pinned to v6.1.9). Pass `-DRUNNER_BUILD_TESTS=OFF` to skip
the test suite (and its doctest dependency).

FTXUI **6.0+** is required for the output pane's text selection.

Run the tests:

```sh
ctest --test-dir build --output-on-failure
```

Install:

```sh
sudo cmake --install build --prefix /usr/local
```

Installs the `runner` binary, the `runner.1` man page, and
`share/doc/runner/runner.toml.example`.

</details>

## Usage

```sh
runner                    # search upward for runner.toml and open the UI
runner -c path/to.toml    # use a specific config
runner --generate-config  # write a starter runner.toml here
runner --help
```

### Keys

| Key | Action |
|-----|--------|
| `↑`/`↓` or `k`/`j` | Select action |
| `Enter` | Run the selected action |
| `PgUp`/`PgDn`, `End`, mouse wheel | Scroll the output pane |
| drag with the mouse | Select output text |
| `y` | Copy the current selection to the clipboard |
| `Ctrl+Y` | Copy the full output of the last command |
| `Ctrl+C` | Kill the running child |
| `Ctrl+D` | Quit |

Copy uses OSC 52 (works over SSH/tmux; honored by e.g. alacritty) and, when
available, an external clipboard tool (`wl-copy`, `xclip`/`xsel`, or `pbcopy`).

See `man 1 runner` for the full `runner.toml` schema.

## Packaging

Packaging for five platforms (Arch, Homebrew, Nix, Debian/Ubuntu, Void) lives
under [`packaging/`](packaging/), all building the **same upstream release** and
each wired up as a `runner.toml` action in the **Packaging** group. See
[`packaging/README.md`](packaging/README.md) for the FTXUI-source matrix and
per-platform maintainer notes.

## License

[MIT](LICENSE)

[ftxui]: https://github.com/ArthurSonzogni/FTXUI
[toml]: https://github.com/marzer/tomlplusplus
[doctest]: https://github.com/doctest/doctest
