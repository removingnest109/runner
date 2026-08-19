# Homebrew packaging for `runner`

[`runner.rb`](runner.rb) is a Homebrew formula that builds `runner` from the
`v0.1.0` release tarball — the same source as the AUR, Nix, Debian, and Void
packaging.

## How it works

- **Source**: the GitHub release tarball for the `v0.1.0` tag, pinned by
  `sha256`.
- **FTXUI**: Homebrew's [`ftxui`](https://formulae.brew.sh/formula/ftxui) formula
  (7.x) is a runtime dependency, located by CMake's `find_package(ftxui)`.
  `runner` requires the FTXUI **6.0+** API and builds against both 6.x and 7.x.
- **tomlplusplus** is header-only, so it is a build-only dependency; **cmake** is
  the build driver. The doctest test suite is disabled in the formula
  (`-DRUNNER_BUILD_TESTS=OFF`); the `test do` block runs a smoke check against the
  installed binary instead.

## Build and install locally

```sh
# Build from the formula in this directory (compiles against Homebrew's ftxui):
brew install --build-from-source ./packaging/homebrew/runner.rb

runner --version        # -> runner 0.1.0
```

## Publish via a tap

```sh
# Create/clone a tap repo (e.g. github.com/removingnest109/homebrew-tap), then:
cp packaging/homebrew/runner.rb "$(brew --repository)"/Library/Taps/removingnest109/homebrew-tap/Formula/
# or commit runner.rb into the tap's Formula/ directory and push.

brew install removingnest109/tap/runner
```

## Updating for a new release

1. Bump `url` to the new tag and refresh `sha256`
   (`brew fetch --build-from-source ./runner.rb` prints the hash, or
   `curl -fL <url> | sha256sum`).
2. `brew audit --strict --formula ./runner.rb` and
   `brew test ./runner.rb` before publishing.
