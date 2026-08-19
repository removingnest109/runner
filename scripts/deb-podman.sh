#!/bin/sh
# Run a Debian packaging command inside a debian:sid podman container with the
# repo bind-mounted at /src. Lets the "Debian: *" runner.toml actions build,
# lint, and inspect the .deb from a non-Debian host (e.g. Void), which has no
# dpkg/debhelper/libftxui-dev >= 6.
#
# The container image (runner-debsid) is built once from an inline Containerfile
# and cached. Force a rebuild (e.g. to pick up newer sid packages) with:
#     podman rmi runner-debsid
#
# Rootless podman maps container-root to the invoking user, so artifacts written
# under /src (dist/deb/...) come back owned by you.
#
# Usage: scripts/deb-podman.sh '<shell command executed in /src>'
set -eu

IMAGE=runner-debsid

if [ "$#" -ne 1 ] || [ -z "$1" ]; then
  echo "usage: scripts/deb-podman.sh '<command>'" >&2
  exit 2
fi

ROOT=$(git rev-parse --show-toplevel)

if ! podman image exists "$IMAGE"; then
  echo ">> building $IMAGE image (first run; cached afterward)..." >&2
  ctx=$(mktemp -d)
  trap 'rm -rf "$ctx"' EXIT
  cat > "$ctx/Containerfile" <<'EOF'
FROM docker.io/library/debian:sid
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      build-essential debhelper devscripts lintian cmake curl ca-certificates \
      libftxui-dev libtomlplusplus-dev doctest-dev \
 && rm -rf /var/lib/apt/lists/*
EOF
  podman build -t "$IMAGE" "$ctx"
fi

exec podman run --rm \
  -v "$ROOT":/src -w /src \
  "$IMAGE" \
  sh -euc "$1"
