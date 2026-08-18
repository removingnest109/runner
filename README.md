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

## Packages

Packaging for four distributions lives under [`packaging/`](packaging/), all
building the **same `v0.1.0` upstream release**. Each has its own README with
build/publish/install commands, and every step is also wired up as a
`runner.toml` action in the **Packaging** group (so you can build a package from
inside `runner` itself).

| Distro | Files | FTXUI source | Notes |
|--------|-------|--------------|-------|
| **Arch (AUR)** | [`packaging/aur/`](packaging/aur/) — `PKGBUILD`, `.SRCINFO` | AUR [`ftxui5`](https://aur.archlinux.org/packages/ftxui5) (5.0.0, build-only) | `tomlplusplus`/`doctest` from `extra` |
| **Nix** | [`flake.nix`](flake.nix) + [`packaging/nix/package.nix`](packaging/nix/package.nix) | nixpkgs `ftxui` (pinned `nixos-24.11` = 5.0.0) | `nix run github:removingnest109/runner` |
| **Debian / Ubuntu** | [`packaging/debian/`](packaging/debian/) | `libftxui-dev` (5.x, runtime) | Debian 13+/Ubuntu 25.04+ |
| **Void** | [`packaging/void/template`](packaging/void/template) | FetchContent tarball (static, offline) | unchanged |

The AUR, Nix, and Debian packages use their distro's **system** FTXUI and toml++
instead of CMake's `FetchContent` fallback (which needs network); doctest is
build/test-only in every case and never a runtime dependency. `runner` requires
the FTXUI **5.x** API (it does not build against 7.x), so each package targets a
FTXUI 5.x provider.

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

See `man 1 runner` for the full `runner.toml` schema.

## License

[MIT](LICENSE)

[ftxui]: https://github.com/ArthurSonzogni/FTXUI
[toml]: https://github.com/marzer/tomlplusplus
[doctest]: https://github.com/doctest/doctest
