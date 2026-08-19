# Void Linux package for `runner`

[`template`](template) is the xbps-src build recipe. It is kept here under
version control; to build a package you copy it into a
[void-packages][void-packages] checkout.

## How it works

- **runner** is built with the `cmake` build style and installed via the CMake
  install rules in the repo root (`bin/runner`, the man page, and
  `runner.toml.example`). `vlicense` installs the MIT `LICENSE`.
- **tomlplusplus** and **doctest** come from Void's `tomlplusplus-devel` and
  `doctest-devel` (exact pinned versions), resolved by CMake's `find_package`.
- **FTXUI** is *not* in Void, so its v6.1.9 release tarball is a second
  `distfiles` entry. `create_wrksrc` extracts both tarballs as siblings;
  `FETCHCONTENT_SOURCE_DIR_FTXUI` + `FETCHCONTENT_FULLY_DISCONNECTED=ON` make
  FetchContent build FTXUI from that local tree with **no network**, and it is
  statically linked into `runner`.
- The doctest test suite is built as part of the package build; run it by
  building with checks enabled (`-Q`, see below).

## Build and install locally

```sh
# 1. Get a void-packages checkout (once) and bootstrap the build environment.
git clone --depth 1 https://github.com/void-linux/void-packages.git
cd void-packages
./xbps-src binary-bootstrap

# 2. Drop the template in (checksums are already filled in).
mkdir -p srcpkgs/runner
cp /path/to/runner/packaging/void/template srcpkgs/runner/template

# 3. Build (add -Q to also compile+run the doctest suite during the build).
#    This needs the v0.1.0 git tag pushed to GitHub so the source tarball exists.
./xbps-src -Q pkg runner

# 4. Install the freshly built package from the local repo.
sudo xbps-install -R hostdir/binpkgs runner
```

If you bump `version=` later, regenerate the checksums with
`./xbps-src fetch runner && xgensum -i srcpkgs/runner/template`.

Verify: `runner --version` prints `runner 0.1.0`; `man 1 runner` shows the page.

## Notes

- The template pins FTXUI to v6.1.9 to match the repo's `FetchContent` tag; bump
  both together.
- If FTXUI is ever packaged in Void, drop the second distfile and the two
  `FETCHCONTENT_*` args and add `ftxui-devel` to `makedepends` — the CMake
  `find_package(ftxui)` path already supports that with no code change.
- `checksum` placeholders (`@@..@@`) must be replaced before the package will
  build; `xgensum -i` does this for you.

[void-packages]: https://github.com/void-linux/void-packages
