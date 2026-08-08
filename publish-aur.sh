#!/bin/bash
# Publish kshot to the AUR as kshot-git.
#
# Split out because the AUR was down for maintenance when the package was first
# prepared. Idempotent: safe to re-run to push an updated PKGBUILD.
set -euo pipefail

PKG=kshot-git
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

cd "$(dirname "$0")"
SRC=$PWD

echo "==> cloning aur/$PKG"
git clone "ssh://aur@aur.archlinux.org/$PKG.git" "$WORK/$PKG"

cd "$WORK/$PKG"
cp "$SRC/PKGBUILD" .
# .SRCINFO must be regenerated from the PKGBUILD, not copied blindly -- the AUR
# rejects a push whose .SRCINFO disagrees with its PKGBUILD.
makepkg --printsrcinfo > .SRCINFO

git add PKGBUILD .SRCINFO
if git diff --cached --quiet; then
    echo "==> nothing changed"
    exit 0
fi
git commit -m "${1:-Initial import: kshot-git}"
git push origin master
echo "==> https://aur.archlinux.org/packages/$PKG"
