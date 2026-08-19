#!/bin/sh
# Phase B of a release: after the version tag has been pushed to GitHub (so the
# release tarball exists), fetch that tarball and refresh every per-distro
# checksum, then commit.
#
# Updates:
#   - AUR      packaging/aur/PKGBUILD + .SRCINFO   (tarball sha256)
#   - Void     packaging/void/template            (runner sha256, first line only)
#   - Homebrew packaging/homebrew/runner.rb        (tarball sha256)
#   - Nix      packaging/nix/package.nix           (fetchFromGitHub SRI hash),
#              only when the `nix` CLI is available — skipped with a warning
#              otherwise.
#
# Usage: scripts/refresh-checksums.sh
set -eu

cd "$(git rev-parse --show-toplevel)"

ver=$(sed -n 's/^project(runner VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt)
if [ -z "$ver" ]; then
  echo "error: could not read current version from CMakeLists.txt" >&2
  exit 1
fi

owner=removingnest109
repo=runner
url="https://github.com/$owner/$repo/archive/refs/tags/v$ver.tar.gz"

# --- fetch the release tarball and hash it ---------------------------------
tarball=$(mktemp)
trap 'rm -f "$tarball"' EXIT
echo ">> fetching $url"
if ! curl -fL -o "$tarball" "$url"; then
  echo "error: could not download $url" >&2
  echo "       has the v$ver tag been pushed to GitHub yet?" >&2
  exit 1
fi

if command -v sha256sum >/dev/null 2>&1; then
  sum=$(sha256sum "$tarball" | awk '{print $1}')
else
  sum=$(shasum -a 256 "$tarball" | awk '{print $1}')
fi
echo ">> tarball sha256: $sum"

# --- AUR -------------------------------------------------------------------
sed -i "s/^sha256sums=('.*')/sha256sums=('$sum')/" packaging/aur/PKGBUILD
sed -i "s/^\([[:space:]]*\)sha256sums = .*/\1sha256sums = $sum/" packaging/aur/.SRCINFO

# --- Void ------------------------------------------------------------------
# The template lists two checksums (runner, then the vendored FTXUI tarball) as a
# multi-line value. Only the line that opens with checksum=" is the runner hash.
sed -i "s/^checksum=\"[0-9a-fA-F]*/checksum=\"$sum/" packaging/void/template

# --- Homebrew --------------------------------------------------------------
sed -i "s/^  sha256 \".*\"/  sha256 \"$sum\"/" packaging/homebrew/runner.rb

# --- Nix (optional — needs the nix CLI) ------------------------------------
nix_done=no
if command -v nix-prefetch-url >/dev/null 2>&1; then
  echo ">> computing Nix SRI hash (nix-prefetch-url --unpack)"
  if b32=$(nix-prefetch-url --unpack --type sha256 "$url" 2>/dev/null) && [ -n "$b32" ]; then
    # `nix hash` needs the nix-command experimental feature; pass it explicitly
    # so this works even where it isn't enabled in nix.conf (nix-prefetch-url
    # above is a legacy command and needs no such flag).
    nixhash="nix --extra-experimental-features nix-command hash"
    if sri=$($nixhash to-sri --type sha256 "$b32" 2>/dev/null); then :;
    elif sri=$($nixhash convert --hash-algo sha256 --to sri "$b32" 2>/dev/null); then :;
    else sri=""; fi
    if [ -n "$sri" ]; then
      sed -i "s|hash = .*;|hash = \"$sri\";|" packaging/nix/package.nix
      echo ">> nix hash: $sri"
      nix_done=yes
    fi
  fi
fi
if [ "$nix_done" = no ]; then
  echo ">> WARNING: skipped packaging/nix/package.nix (nix CLI unavailable or" >&2
  echo "            hash computation failed). Fill 'hash' manually with:" >&2
  echo "              nix-prefetch-url --unpack $url" >&2
  echo "            then: nix hash to-sri --type sha256 <printed-hash>" >&2
fi

# --- commit ----------------------------------------------------------------
git add packaging/aur/PKGBUILD packaging/aur/.SRCINFO \
  packaging/void/template packaging/homebrew/runner.rb
[ "$nix_done" = yes ] && git add packaging/nix/package.nix

if git diff --cached --quiet; then
  echo ">> checksums for v$ver already up to date; nothing to commit."
  exit 0
fi

git commit -m "release: refresh checksums for v$ver"

cat <<EOF

>> checksums refreshed for v$ver and committed.
   Push when ready:  git push
EOF
