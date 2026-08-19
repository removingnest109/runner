# Nix packaging for `runner`

The Nix packaging is two files:

- [`../../flake.nix`](../../flake.nix) — the flake at the repository root, so
  `nix run github:removingnest109/runner` works.
- [`package.nix`](package.nix) — the actual derivation, written `callPackage`-style
  so it can be dropped into nixpkgs (`pkgs/by-name/ru/runner/package.nix`) with
  only the `src` hash to fill in.

## How it works

- **Source**: the flake builds this repository's tree (`src = self`), i.e. the
  same source as the `v0.1.0` release. `package.nix` also carries a
  `fetchFromGitHub` `src` for a future nixpkgs submission (its hash is a
  `lib.fakeHash` placeholder — the flake overrides `src`, so that hash is never
  evaluated during flake builds).
- **Dependencies come from nixpkgs**, never fetched or vendored:
  - `ftxui` and `tomlplusplus` are `buildInputs`, found by CMake's
    `find_package`. FTXUI links into the binary (shared in nixpkgs → part of the
    runtime closure); tomlplusplus is header-only (build-time only).
  - `doctest` is a `nativeCheckInput` used only to compile and run the test
    suite. The test target is gated on `doCheck` via `cmakeFlags`, so doctest
    **never enters the runtime closure**.
- **nixpkgs pin**: `flake.nix` pins `nixpkgs` to **`nixos-25.05`**, whose
  `ftxui` (6.x, 6.1.9), `tomlplusplus` (3.4.0) and `doctest` (2.4.11) satisfy the
  project's `find_package` calls. `runner` requires the FTXUI **6.0+** API (text
  selection) and builds against both 6.x and 7.x — its CMake queries
  `find_package(ftxui)` unversioned with a 6.0 floor — so any channel whose
  `ftxui` is >= 6 works.
- **Tests run in `checkPhase`** (`doCheck = true`) via CMake/CTest.
- **CMake FetchContent is never triggered**: with all three deps present,
  `find_package` succeeds for each, so the sandboxed (network-free) Nix build
  needs no downloads.

## Use it

```sh
# Run without installing:
nix run github:removingnest109/runner

# Install into your profile:
nix profile install github:removingnest109/runner

# Build locally from a checkout:
nix build            # result/bin/runner
nix run .            # build and run
nix develop          # dev shell with cmake + ftxui + tomlplusplus + doctest
```

Supported systems: `x86_64-linux`, `aarch64-linux`.

## Reproducibility / lock file

This repo does not commit a `flake.lock` (it cannot be generated without Nix
present). Generate and commit one to pin the exact nixpkgs revision:

```sh
nix flake lock          # writes flake.lock pinning nixos-25.05's current commit
git add flake.lock
```

Without a committed lock, `nix run github:…` resolves `nixos-25.05` to its tip at
run time, which still yields ftxui 6.x / tomlplusplus 3.4.0 / doctest 2.4.11.

## Toward nixpkgs submission

`package.nix` is already in nixpkgs form. To submit:

1. Copy it to `pkgs/by-name/ru/runner/package.nix` in a nixpkgs checkout.
2. Replace the `src` hash: `nix-prefetch-url --unpack \
   https://github.com/removingnest109/runner/archive/refs/tags/v0.1.0.tar.gz`
   (or `nix flake prefetch`), and drop the `lib.fakeHash`.
3. `nix-build -A runner` and `nix run nixpkgs#nixpkgs-review` as usual.

Note: `runner` builds against nixpkgs' current `ftxui` (6.x/7.x) directly — the
`find_package(ftxui)` call is unversioned with a 6.0 floor, so no channel-specific
pin is needed for a nixpkgs submission.
