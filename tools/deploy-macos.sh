#!/usr/bin/env bash
# Deploy a development bundle and give its edited Mach-O files local ad hoc
# signatures. Release signing/notarization is intentionally a separate step.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
host=${MACOS_HOST:-}
[[ -n $host ]] || { echo 'Set MACOS_HOST=user@mac-host to select the deployment destination.' >&2; exit 1; }
package=${ALLEGIANCE_MACOS_PACKAGE:-"$repo/build/macos-package"}
[[ -f "$package/Allegiance.app/Contents/Info.plist" ]] || { echo 'Run tools/build-macos.sh first.' >&2; exit 1; }
ssh -F /dev/null "$host" 'mkdir -p "$HOME/Applications"'
rsync -a --delete -e 'ssh -F /dev/null' "$package/Allegiance.app/" "$host:Applications/Allegiance.app/"
ssh -F /dev/null "$host" /bin/bash -s <<'REMOTE'
set -euo pipefail
app="$HOME/Applications/Allegiance.app"
for file in "$app"/Contents/Frameworks/*.dylib "$app"/Contents/MacOS/*; do
    codesign --force --sign - "$file"
done
codesign --force --sign - "$app"
codesign --verify --deep --strict "$app"
printf 'Installed %s\n' "$app"
REMOTE
