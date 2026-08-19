#!/bin/sh
# Lint the most recently built runner .deb with lintian. Meant to run inside the
# debian:sid container (via scripts/deb-podman.sh), where cwd is the repo root
# (/src); also works directly on a real Debian host.
set -eu

deb=$(ls -t dist/deb/runner_*_*.deb 2>/dev/null | head -1)
if [ -z "$deb" ]; then
  echo 'no .deb in dist/deb — run "Debian: build .deb" first' >&2
  exit 1
fi
exec lintian --info --display-info "$deb"
