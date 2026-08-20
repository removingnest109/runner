#!/bin/sh
# Phase A of a release: bump the runner version, propagate it to every packaging
# file, then commit and create a *local* annotated git tag.
#
# The tag is NOT pushed — pushing it is what makes GitHub generate the release
# tarball, so it stays a deliberate manual step. After you push the tag, run
# scripts/refresh-checksums.sh (Phase B) to fill in the per-distro checksums.
#
# Usage: scripts/bump-version.sh <major|minor>
#   major:  X.Y.Z -> X.(Y+1).0   (project convention — not semver "major")
#   minor:  X.Y.Z -> X.Y.(Z+1)
set -eu

part="${1:-}"
case "$part" in
  major | minor) ;;
  *)
    echo "usage: $0 <major|minor>" >&2
    exit 2
    ;;
esac

# Run from the repo root no matter where runner invoked us from.
cd "$(git rev-parse --show-toplevel)"

# --- guards ----------------------------------------------------------------
if ! git diff --quiet || ! git diff --cached --quiet; then
  echo "error: working tree has uncommitted changes; commit or stash first." >&2
  exit 1
fi

cur=$(sed -n 's/^project(runner VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt)
if [ -z "$cur" ]; then
  echo "error: could not read current version from CMakeLists.txt" >&2
  exit 1
fi

# Split X.Y.Z and compute the next version.
X=${cur%%.*}
rest=${cur#*.}
Y=${rest%%.*}
Z=${rest#*.}
case "$part" in
  major)
    Y=$((Y + 1))
    Z=0
    ;;
  minor) Z=$((Z + 1)) ;;
esac
new="$X.$Y.$Z"

if git rev-parse -q --verify "refs/tags/v$new" >/dev/null; then
  echo "error: tag v$new already exists." >&2
  exit 1
fi

# Dot-escaped current version, for use inside sed regexes.
cur_re=$(printf '%s' "$cur" | sed 's/\./\\./g')

echo ">> bumping $cur -> $new"

# --- rewrite version literals ----------------------------------------------
# CMakeLists.txt is the single source of truth (feeds RUNNER_VERSION).
sed -i "s/VERSION $cur_re /VERSION $new /" CMakeLists.txt

# AUR PKGBUILD: pkgver + reset pkgrel. The source= line uses \$pkgver, so it
# tracks automatically.
sed -i "s/^pkgver=$cur_re\$/pkgver=$new/; s/^pkgrel=.*/pkgrel=1/" packaging/aur/PKGBUILD

# AUR .SRCINFO mirrors PKGBUILD but has no interpolation — bump the literals in
# place (avoids a hard makepkg dependency on non-Arch hosts).
sed -i "/pkgver = /s/$cur_re/$new/; \
        s/^\([[:space:]]*\)pkgrel = .*/\1pkgrel = 1/; \
        /source = /s/$cur_re/$new/g" packaging/aur/.SRCINFO

# Nix package.nix (fetchFromGitHub tag tracks \${version}).
sed -i "s/version = \"$cur_re\";/version = \"$new\";/" packaging/nix/package.nix

# Void template: version + reset revision. distfiles= uses \${version}.
sed -i "s/^version=$cur_re\$/version=$new/; s/^revision=.*/revision=1/" packaging/void/template

# Debian build.sh (TAG derives from VERSION).
sed -i "s/^VERSION=$cur_re\$/VERSION=$new/" packaging/debian/build.sh

# Homebrew formula: the url carries a literal tag, so rewrite it.
sed -i "s|v$cur_re\.tar\.gz|v$new.tar.gz|" packaging/homebrew/runner.rb

# Man page .TH line: version token + revision date. Matched generically (not
# against $cur) so it self-heals even if the page ever drifts from the code
# version. The date reflects when the manual was last revised (this release).
today=$(date +%Y-%m-%d)
sed -i "s/\"runner [0-9][0-9.]*\"/\"runner $new\"/; \
        s/^\(\.TH RUNNER 1 \)\"[0-9-]*\"/\1\"$today\"/" docs/runner.1

# --- prepend a new debian/changelog stanza ---------------------------------
name=$(git config user.name || true)
email=$(git config user.email || true)
name=${name:-runner maintainer}
email=${email:-nobody@example.com}
# RFC-2822 date in a format both GNU and BSD date understand.
date=$(date "+%a, %d %b %Y %H:%M:%S %z")

tmp=$(mktemp)
{
  printf 'runner (%s-1) unstable; urgency=medium\n\n' "$new"
  printf '  * Bump to %s.\n\n' "$new"
  printf ' -- %s <%s>  %s\n\n' "$name" "$email" "$date"
  cat packaging/debian/changelog
} >"$tmp"
mv "$tmp" packaging/debian/changelog

# --- commit + local tag ----------------------------------------------------
git add CMakeLists.txt \
  packaging/aur/PKGBUILD packaging/aur/.SRCINFO \
  packaging/nix/package.nix \
  packaging/void/template \
  packaging/debian/build.sh packaging/debian/changelog \
  packaging/homebrew/runner.rb \
  docs/runner.1
git commit -m "release: v$new"
git tag -a "v$new" -m "v$new"

cat <<EOF

>> bumped to $new, committed, and tagged v$new (local only).

Next steps:
  1. Review the release commit, then push it and the tag:
       git push && git push origin v$new
  2. Once the tag is on GitHub (the release tarball now exists), run Phase B:
       scripts/refresh-checksums.sh
     to fill in the AUR / Void / Homebrew sha256 and the Nix hash.
EOF
