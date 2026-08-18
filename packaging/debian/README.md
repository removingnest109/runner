# Debian / Ubuntu packaging for `runner`

Standard Debian packaging (`debian/control`, `debian/rules`, …) that builds a
`.deb` from the upstream `v0.1.0` release tarball — the same source as the AUR,
Nix, and Void packaging. It is kept here rather than at the repository root so
the packaging stays separate from the application source; [`build.sh`](build.sh)
assembles the `debian/` directory onto the extracted upstream tarball at build
time (the classic non-native model, `3.0 (quilt)`).

## Dependencies and how they are used

| Dependency        | Debian package        | Role in the `.deb`                    |
|-------------------|-----------------------|---------------------------------------|
| FTXUI 5.x         | `libftxui-dev`        | **Runtime** — the binary dynamically links `libftxui-{component,dom,screen}5.0.0`; `${shlibs:Depends}` adds them automatically. |
| toml++ 3.4.0      | `libtomlplusplus-dev` | **Build only** — header-only, not linked. |
| doctest 2.4.11    | `doctest-dev`         | **Build/test only** — compiles the doctest suite run during the build; not linked into `runner`. |

CMake finds all three with `find_package`, so its `FetchContent` fallback is
never triggered — the build needs no network and downloads no FTXUI/toml++/doctest.

## Minimum distro versions (important)

The project needs the FTXUI **5.x** API (it does not build against FTXUI 7.x),
and `find_package(ftxui 5.0.0)` uses `SameMajorVersion` matching. `libftxui-dev`
must therefore be **5.x** — the build-deps require `>= 5.0.0` and `<< 6`:

- **Debian 13 "trixie"** — `libftxui-dev` 5.0.0 ✓  (Debian 12 "bookworm" has no FTXUI → not supported)
- **Ubuntu 25.04 "plucky", 25.10 "questing", 26.04 "resolute"** — `libftxui-dev` 5.0.0 ✓
- Newer releases whose `libftxui-dev` has moved to 6.x/7.x (Debian 14 "forky"/sid,
  Ubuntu 26.10 "stonking") do **not** satisfy the build-dep until upstream
  supports that FTXUI major.

`libtomlplusplus-dev` (3.4.0) and `doctest-dev` (>= 2.4.11) are present on all of
the above.

## Build the .deb

```sh
# 1. Install the build dependencies (Debian 13+ / Ubuntu 25.04+):
sudo apt install build-essential debhelper cmake curl \
     libftxui-dev libtomlplusplus-dev doctest-dev

# 2. Build (downloads the v0.1.0 orig tarball, builds, runs the test suite):
packaging/debian/build.sh          # add -b for a binary-only build

# The .deb is printed at the end, e.g.:
#   /tmp/tmp.XXXX/runner_0.1.0-1_amd64.deb
```

Or, if you prefer to drive it by hand, the equivalent of what `build.sh` does:

```sh
curl -fL -o ../runner_0.1.0.orig.tar.gz \
  https://github.com/removingnest109/runner/archive/refs/tags/v0.1.0.tar.gz
# extract it, copy this debian/ dir into the extracted tree, then from there:
dpkg-buildpackage -us -uc
```

## Install / verify / remove

```sh
sudo apt install ./runner_0.1.0-1_amd64.deb    # resolves the FTXUI runtime dep
# or:  sudo dpkg -i runner_0.1.0-1_amd64.deb && sudo apt -f install

runner --version                                # -> runner 0.1.0
man 1 runner
dpkg -L runner                                  # installed files
dpkg -s runner                                  # metadata + Depends

sudo apt remove runner
```

Installed files (Debian FHS locations):

- `/usr/bin/runner`
- `/usr/share/man/man1/runner.1.gz`
- `/usr/share/doc/runner/runner.toml.example`
- `/usr/share/doc/runner/{changelog.Debian.gz,copyright}`

## Validate the package

```sh
lintian --info --display-info runner_0.1.0-1_amd64.deb
dpkg-deb --info      runner_0.1.0-1_amd64.deb   # control metadata
dpkg-deb --contents  runner_0.1.0-1_amd64.deb   # file list
# Confirm the FTXUI runtime dep was picked up and toml++/doctest were NOT:
dpkg-deb -f runner_0.1.0-1_amd64.deb Depends
```
