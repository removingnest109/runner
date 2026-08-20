# runner

A TUI project script runner. `runner` reads a `runner.toml` describing a set of
actions, shows them in an interactive terminal list, and runs the selected one
in a child shell while streaming its output — including 256-color and truecolor
ANSI — into a scrolling pane.

## Features

- **Per-project action lists** — commands live in a `runner.toml` that runner
  finds by searching upward from the current directory.
- **Nested groups** — an action's `label` is a `/`-separated path, so actions
  organize into collapsible headings and subheadings.
- **Action chaining** — declare prerequisites (`depends_on`), bundle steps into
  composite `sequence`s, and gate work with conditional skips (`only_if_cmd`).
- **Environment control** — per-action environment injection plus load-time
  `${VAR}` expansion in `cwd` and `env`.
- **Rich output pane** — streams merged stdout/stderr with full 256-color and
  truecolor ANSI, mouse-drag text selection, and clipboard copy.
- **Zero-setup start** — `--generate-config` writes a starter file to build from.

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
| `Enter` | Run the selected action (and its resolved chain) |
| `p` | Preview the selected action's plan without running it |
| `PgUp`/`PgDn`, `End`, mouse wheel | Scroll the output pane |
| drag with the mouse | Select output text |
| `y` | Copy the current selection to the clipboard |
| `Ctrl+Y` | Copy the full output of the last command |
| `Ctrl+C` | Kill the running child (aborts the chain) |
| `Ctrl+D` | Quit |

Copy uses OSC 52 (works over SSH/tmux; honored by e.g. alacritty) and, when
available, an external clipboard tool (`wl-copy`, `xclip`/`xsel`, or `pbcopy`).

## Configuration

Actions live in a `runner.toml` — a TOML array of `[[action]]` tables at your
project root. Run `runner --generate-config` to drop a starter file, then edit
it. Every action needs a `label` and exactly one of `cmd` or `sequence`;
everything else is optional. The subsections below cover each feature; see
`man 1 runner` for the exhaustive schema.

<details>
<summary><strong>Writing actions</strong></summary>

Each `[[action]]` is one entry in the list. The two required fields are `label`
(what shows in the sidebar) and `cmd` (the command, run via `/bin/sh -c`):

```toml
[[action]]
label = "Build"                 # sidebar name
cmd   = "cmake --build build"   # run with /bin/sh -c — full POSIX sh: pipes, &&, globs
desc  = "Compile the project"   # optional one-line description
cwd   = "build"                 # optional working dir, relative to runner.toml
```

| Field  | Required | Meaning |
|--------|----------|---------|
| `label` | yes | Path-style name shown in the sidebar (see grouping below). |
| `cmd`   | yes\* | Command line, run with `/bin/sh -c`. \*Omit only when `sequence` is set. |
| `desc`  | no  | One-line description. |
| `cwd`   | no  | Working directory; empty → the config file's directory, relative paths resolve against it, and it must exist. |
| `env`   | no  | Per-action environment variables (see below). |

`cmd` runs through a shell, so `cmd1 && cmd2`, pipes, redirection, and globbing
all work. stdout and stderr are merged into the output pane with ANSI color
rendered.

</details>

<details>
<summary><strong>Grouping with path-style labels</strong></summary>

An action's `label` is a **path**: the last `/`-segment is the name shown in the
sidebar, and any earlier segments nest it under bold headings. A label with no
`/` is a top-level action, rendered under **Ungrouped**.

```toml
[[action]]
label = "Packaging/Debian/build"    # "build" under Packaging › Debian
cmd   = "./packaging/debian/build.sh"

[[action]]
label = "Packaging/Arch/build"      # a different "build" under Packaging › Arch
cmd   = "makepkg -si"
```

renders as

```
Packaging
  Debian
    build
  Arch
    build
```

Segments are whitespace-trimmed. A parent's subgroups are collected under one
heading even when the actions are scattered through the file, and a node can mix
direct actions with subgroups (they order by first appearance). Because the
group is part of the label, the **name only has to be unique within its group** —
`Packaging/Debian/build` and `Packaging/Arch/build` happily coexist.

</details>

<details>
<summary><strong>Environment &amp; <code>${VAR}</code> expansion</strong></summary>

Add per-action environment variables with an inline `env` table (values must be
strings). They're injected on top of runner's inherited environment:

```toml
[[action]]
label = "Run/staging"
cmd   = "./server"
env   = { APP_ENV = "staging", LOG_LEVEL = "debug" }
```

runner expands `${VAR}` in `cwd` and `env` values **at load time** from its own
environment — an undefined variable is a load error:

```toml
[[action]]
label = "Build/workspace"
cmd   = "make"
cwd   = "${HOME}/project/build"        # expanded by runner when the config loads
env   = { CACHE = "${HOME}/.cache/x" }
```

Inside `cmd`, expansion is left to the shell at run time, so `$VAR` there sees
the injected `env` plus full sh features (`${PATH%%:*}`, subshells, and so on).

</details>

<details>
<summary><strong>Chaining: dependencies, composites &amp; conditional skips</strong></summary>

Every action is addressed by its full label path, and can pull in others by that
same path:

```toml
[[action]]
label = "Dev/Build"
cmd   = "cmake --build build"

[[action]]
label = "Dev/Test"
cmd   = "ctest --test-dir build"
depends_on = ["Dev/Build"]          # runs Dev/Build first
only_if_cmd = "! git diff --quiet"  # ...but skips it when nothing changed

[[action]]
label = "Dev/Check"
sequence = ["Dev/Build", "Dev/Test"]  # composite: runs each in order
```

Triggering an action resolves a **chain**: its `depends_on` (recursively) and,
for a composite, its `sequence` members, deduplicated into one ordered run. A
shared dependency runs once per chain, and the chain stops on the first command
that exits non-zero. `only_if_cmd` runs before an action in that action's
`cwd`/`env`; a zero exit runs it, non-zero skips it. A reference is the target's
full label path (`Packaging/Arch/build`); duplicate labels, unknown references,
and dependency cycles are load errors. Press `p` to preview the resolved plan
before running it.

Add `hidden = true` to keep an action out of the menu while leaving it runnable
as a `depends_on`/`sequence` member — handy for a composite's steps you never
launch on their own.

</details>

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
