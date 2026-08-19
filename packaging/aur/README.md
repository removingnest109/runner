# AUR package for `runner`

This directory holds the files that make up the Arch User Repository package:
[`PKGBUILD`](PKGBUILD) and its generated [`.SRCINFO`](.SRCINFO). They are kept
under version control here; the AUR repository itself is a separate git repo that
contains only these two files.

## How it works

- **Source**: the GitHub release tarball for the `v0.1.0` tag — the same
  upstream release used by the Void, Debian, and Nix packaging. `sha256sums`
  pins it.
- **FTXUI**: FTXUI is not in the official Arch repositories. `runner` requires
  the FTXUI **6.0+** API (text selection) and builds against both 6.x and 7.x.
  The main AUR [`ftxui`][ftxui] package (currently 7.x) installs its CMake config
  at the standard `/usr/lib/cmake/ftxui`, which `find_package(ftxui)` locates with
  no extra hints. `ftxui` ships **static** libraries, so it is a `makedepends`
  (build-only) and the runner binary does not gain a runtime shared-library
  dependency on it.
- **tomlplusplus** / **doctest**: from the official `extra` packages
  `tomlplusplus` (3.4.0) and `doctest` (>= 2.4.11), resolved by CMake's
  `find_package`. tomlplusplus is header-only and doctest is only used by the
  test suite, so both are `makedepends`, not runtime deps.
- **Tests**: `check()` runs the doctest suite via `ctest` during package build.
- **No downloads during build**: with the three deps installed, CMake finds them
  all with `find_package` and never triggers its `FetchContent` fallback — the
  build works in makepkg's network-isolated environment.
- **Install paths**: `bin/runner`, `share/man/man1/runner.1`,
  `share/doc/runner/runner.toml.example` (from the project's CMake install
  rules), plus the MIT `LICENSE` at `share/licenses/runner/LICENSE`.

## Build and install locally

```sh
# 1. Install the FTXUI dependency from the AUR (any AUR helper works):
yay -S ftxui             # or: paru -S ftxui, or makepkg it by hand

# 2. Build and install runner from this directory:
cd packaging/aur
makepkg -si              # builds (running the test suite) and installs
```

`makepkg -si` pulls `tomlplusplus`/`doctest`/`cmake` from the official repos
automatically; `ftxui` must come from the AUR (step 1), because makepkg itself
does not resolve AUR dependencies.

## Publish to the AUR

```sh
# One-time: clone the (empty) AUR repo for the package name.
git clone ssh://aur@aur.archlinux.org/runner.git aur-runner
cd aur-runner

# Copy the packaging files in.
cp /path/to/runner/packaging/aur/PKGBUILD .
cp /path/to/runner/packaging/aur/.SRCINFO .

# (If you edit PKGBUILD, regenerate .SRCINFO with:  makepkg --printsrcinfo > .SRCINFO)

git add PKGBUILD .SRCINFO
git commit -m "Initial import: runner 0.1.0"
git push
```

## Updating for a new release

1. Bump `pkgver` (and reset `pkgrel=1`) in `PKGBUILD`.
2. Refresh the checksum: `updpkgsums` (or `makepkg -g`).
3. Regenerate metadata: `makepkg --printsrcinfo > .SRCINFO`.
4. Rebuild/validate: `makepkg -f` and `namcap PKGBUILD runner-*.pkg.tar.zst`.
5. Commit and push both files to the AUR repo.

[ftxui]: https://aur.archlinux.org/packages/ftxui
