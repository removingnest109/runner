# Packaging `runner`

Packaging for five platforms, all building the **same upstream release**. Each
subdirectory has its own README with build/publish/install commands, and every
step is also wired up as a `runner.toml` action in the **Packaging** group (so you
can build a package from inside `runner` itself).

| Distro | Files | FTXUI source | Notes |
|--------|-------|--------------|-------|
| **Arch (AUR)** | [`aur/`](aur/) — `PKGBUILD`, `.SRCINFO` | AUR [`ftxui`](https://aur.archlinux.org/packages/ftxui) (7.x, build-only) | `tomlplusplus`/`doctest` from `extra` |
| **Homebrew** | [`homebrew/runner.rb`](homebrew/runner.rb) | `ftxui` formula (7.x) | `brew install --build-from-source ./runner.rb` |
| **Nix** | [`../flake.nix`](../flake.nix) + [`nix/package.nix`](nix/package.nix) | nixpkgs `ftxui` (pinned `nixos-25.05` = 6.x) | `nix run github:removingnest109/runner` |
| **Debian / Ubuntu** | [`debian/`](debian/) | `libftxui-dev` (6.x/7.x, runtime) | Debian 14+/Ubuntu with libftxui-dev >= 6 |
| **Void** | [`void/template`](void/template) | FetchContent tarball (static, offline) | FTXUI v6.1.9 |

The AUR, Homebrew, Nix, and Debian packages use their platform's **system** FTXUI
and toml++ instead of CMake's `FetchContent` fallback (which needs network);
doctest is build/test-only in every case and never a runtime dependency. `runner`
requires the FTXUI **6.0+** API and builds against both 6.x and 7.x, so each
package targets a FTXUI 6.x-or-newer provider.

For build, publish, and install instructions, see each platform's README:

- [`aur/README.md`](aur/README.md)
- [`homebrew/README.md`](homebrew/README.md)
- [`nix/README.md`](nix/README.md)
- [`debian/README.md`](debian/README.md)
- [`void/README.md`](void/README.md)
