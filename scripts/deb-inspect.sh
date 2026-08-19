#!/bin/sh
# Show metadata, runtime Depends, and the file list of the most recently built
# runner .deb. Meant to run inside the debian:sid container (via
# scripts/deb-podman.sh), where cwd is the repo root (/src); also works directly
# on a real Debian host.
set -eu

deb=$(ls -t dist/deb/runner_*_*.deb 2>/dev/null | head -1)
if [ -z "$deb" ]; then
  echo 'no .deb in dist/deb — build it first' >&2
  exit 1
fi
dpkg-deb --info "$deb"
echo '--- Depends ---'
dpkg-deb -f "$deb" Depends
echo '--- contents ---'
dpkg-deb --contents "$deb"
