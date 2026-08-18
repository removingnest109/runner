#!/bin/sh
# Assemble a proper (non-native) Debian source package from the v0.1.0 upstream
# release tarball — the same source used by the AUR, Nix, and Void packaging —
# plus this debian/ directory, then build the .deb.
#
# Requirements (Debian 13 "trixie"+ / Ubuntu 25.04 "plucky"+):
#   sudo apt install build-essential debhelper cmake curl \
#        libftxui-dev libtomlplusplus-dev doctest-dev
#
# Usage:
#   packaging/debian/build.sh            # build source + binary .deb
#   packaging/debian/build.sh -b         # binary-only
#   WORK=/path/to/dir packaging/debian/build.sh   # choose the work dir
set -eu

VERSION=0.1.0
OWNER=removingnest109
REPO=runner
TAG="v${VERSION}"

HERE=$(cd "$(dirname "$0")" && pwd)          # packaging/debian
WORK="${WORK:-$(mktemp -d)}"

ORIG="${REPO}_${VERSION}.orig.tar.gz"
SRC="${REPO}-${VERSION}"

mkdir -p "$WORK"
WORK=$(cd "$WORK" && pwd)                     # normalise to an absolute path
echo ">> work dir: $WORK"
cd "$WORK"

# 1. Upstream release tarball -> Debian .orig.tar.gz (canonical GitHub release).
if [ ! -f "$ORIG" ]; then
  curl -fL -o "$ORIG" \
    "https://github.com/${OWNER}/${REPO}/archive/refs/tags/${TAG}.tar.gz"
fi

# 2. Extract and drop in the debian/ packaging directory (minus these helpers).
rm -rf "$SRC"
tar xzf "$ORIG"
cp -r "$HERE" "$SRC/debian"
rm -f "$SRC/debian/build.sh" "$SRC/debian/README.md"

# 3. Build. dpkg-buildpackage runs configure -> build -> the doctest suite ->
#    install, and dh_shlibdeps computes the FTXUI runtime dependency.
cd "$SRC"
dpkg-buildpackage -us -uc "$@"

echo ""
echo ">> Built artifacts in: $WORK"
ls -1 "$WORK"/*.deb 2>/dev/null || true
