# runner

A TUI project script runner. `runner` reads a `runner.toml` describing a set of
actions, shows them in an interactive terminal list, and runs the selected one
in a child shell while streaming its output — including 256-color and truecolor
ANSI — into a scrolling pane.

Features: per-project action lists with groups, `${VAR}` expansion and per-action
environment injection, upward config search, a `--generate-config` starter, and
mouse selection / clipboard copy of the output pane.

## Building from source

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

## Installing

```sh
sudo cmake --install build --prefix /usr/local
```

Installs the `runner` binary, the `runner.1` man page, and
`share/doc/runner/runner.toml.example`.

## Packages

Packaging for five platforms lives under [`packaging/`](packaging/), all
building the **same upstream release**. Each has its own README with
build/publish/install commands, and every step is also wired up as a
`runner.toml` action in the **Packaging** group (so you can build a package from
inside `runner` itself).

| Distro | Files | FTXUI source | Notes |
|--------|-------|--------------|-------|
| **Arch (AUR)** | [`packaging/aur/`](packaging/aur/) — `PKGBUILD`, `.SRCINFO` | AUR [`ftxui`](https://aur.archlinux.org/packages/ftxui) (7.x, build-only) | `tomlplusplus`/`doctest` from `extra` |
| **Homebrew** | [`packaging/homebrew/runner.rb`](packaging/homebrew/runner.rb) | `ftxui` formula (7.x) | `brew install --build-from-source ./runner.rb` |
| **Nix** | [`flake.nix`](flake.nix) + [`packaging/nix/package.nix`](packaging/nix/package.nix) | nixpkgs `ftxui` (pinned `nixos-25.05` = 6.x) | `nix run github:removingnest109/runner` |
| **Debian / Ubuntu** | [`packaging/debian/`](packaging/debian/) | `libftxui-dev` (6.x/7.x, runtime) | Debian 14+/Ubuntu with libftxui-dev >= 6 |
| **Void** | [`packaging/void/template`](packaging/void/template) | FetchContent tarball (static, offline) | FTXUI v6.1.9 |

The AUR, Homebrew, Nix, and Debian packages use their platform's **system** FTXUI
and toml++ instead of CMake's `FetchContent` fallback (which needs network);
doctest is build/test-only in every case and never a runtime dependency. `runner`
requires the FTXUI **6.0+** API and builds against both 6.x and 7.x, so each
package targets a FTXUI 6.x-or-newer provider.

### Debian / Ubuntu

A prebuilt `amd64` `.deb` is attached to every
[release](https://github.com/removingnest109/runner/releases/latest). Download the
`.deb` from the latest release and install it with `dpkg`:

```sh
sudo dpkg -i runner_*_amd64.deb
```

The package is built against **FTXUI 6+**, so it needs Debian 14 ("forky")/sid or
an Ubuntu recent enough to ship `libftxui-dev >= 6`. If `dpkg` reports unmet
dependencies, pull them in with `sudo apt-get install -f`.

### Void Linux

A ready-to-use xbps-src template lives at [`packaging/void/template`](packaging/void/template).
To build and install it locally:

Setup void-packages if you have not already:

```sh
git clone --depth 1 https://github.com/void-linux/void-packages.git
cd void-packages
./xbps-src binary-bootstrap
```
Then copy the template from the repo, build and install:

```sh
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

## License

[MIT](LICENSE)

[ftxui]: https://github.com/ArthurSonzogni/FTXUI
[toml]: https://github.com/marzer/tomlplusplus
[doctest]: https://github.com/doctest/doctest
