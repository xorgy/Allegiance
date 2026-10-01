#!/bin/bash
# Run locally on the signing Mac. No credentials or private keys leave Keychain.
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
if [[ -f "$script_dir/signing.conf" ]]; then source "$script_dir/signing.conf"; fi
app=${ALLEGIANCE_SIGN_APP:-"$script_dir/Allegiance.app"}
output=${ALLEGIANCE_SIGN_OUTPUT:-"$script_dir/dist"}
profile=${NOTARY_PROFILE:-freeallegiance-notary}
mode=${1:-finalize}
case "$mode" in
    --help)
        cat <<'HELP'
Run locally on macOS with Xcode's command line tools:
  ./finalize-macos-release.sh --check          Inspect the bundle and signing identity
  ./finalize-macos-release.sh --setup-notary   Store notarization credentials interactively in Keychain (once)
  ./finalize-macos-release.sh                  Sign, notarize, staple, verify and make the release ZIP

Optional signing.conf or environment: CODE_SIGN_IDENTITY, DEVELOPER_TEAM_ID,
NOTARY_PROFILE, ALLEGIANCE_SIGN_APP, ALLEGIANCE_SIGN_OUTPUT.
The source app is preserved. Signed output and Apple submission logs go in dist/.
HELP
        exit 0 ;;
    --check|--setup-notary|finalize) ;;
    *) echo "Unknown option: $mode (use --help)" >&2; exit 1 ;;
esac
[[ $# -le 1 ]] || { echo 'Expected at most one option.' >&2; exit 1; }
[[ $(uname -s) == Darwin ]] || { echo 'Run this script locally on the signing Mac.' >&2; exit 1; }
for tool in codesign security xcrun ditto plutil file shasum spctl; do
    command -v "$tool" >/dev/null || { echo "Missing tool: $tool" >&2; exit 1; }
done
xcrun --find notarytool >/dev/null
xcrun --find stapler >/dev/null

identities=()
while IFS= read -r name; do identities+=("$name"); done < <(
    security find-identity -v -p codesigning | sed -n 's/.*"\(Developer ID Application: [^"]*\)".*/\1/p'
)
identity=${CODE_SIGN_IDENTITY:-}
if [[ -z $identity ]]; then
    [[ ${#identities[@]} -eq 1 ]] || { echo 'Set CODE_SIGN_IDENTITY to the intended Developer ID Application certificate.' >&2; exit 1; }
    identity=${identities[0]}
fi
found=false
for name in "${identities[@]}"; do [[ $name != "$identity" ]] || found=true; done
[[ $found == true ]] || { echo "No valid Developer ID Application identity: $identity" >&2; exit 1; }
team=${DEVELOPER_TEAM_ID:-${identity##*(}}
team=${team%)}
[[ $team =~ ^[A-Z0-9]{10}$ ]] || { echo 'Set DEVELOPER_TEAM_ID to your 10-character developer team ID.' >&2; exit 1; }
printf 'Signing identity: %s\nTeam: %s\nNotary Keychain profile: %s\n' "$identity" "$team" "$profile"
if [[ $mode == --setup-notary ]]; then
    # notarytool prompts for the Apple ID and app-specific password, or API
    # credentials. They are stored in Keychain, never in this configuration.
    xcrun notarytool store-credentials "$profile" --team-id "$team"
    exit 0
fi
[[ -f "$app/Contents/Info.plist" && -f "$app/Contents/Resources/build-info.json" ]] || { echo "Missing packaged app: $app" >&2; exit 1; }
[[ $(plutil -extract CFBundleIdentifier raw -o - "$app/Contents/Info.plist") == org.freeallegiance.Native ]] || { echo 'Unexpected app identifier.' >&2; exit 1; }
[[ ! -d "$app/Contents/Resources/Artwork" ]] || { echo 'Release bundle must not contain Steam artwork.' >&2; exit 1; }
build_id=$(plutil -extract network_build raw -o - "$app/Contents/Resources/build-info.json")
[[ $build_id =~ ^[0-9a-f]{64}$ ]] || { echo 'Invalid build identity.' >&2; exit 1; }
for binary in "$app"/Contents/Frameworks/*.dylib "$app"/Contents/MacOS/*; do
    [[ -f $binary ]] || { echo "Missing runtime code: $binary" >&2; exit 1; }
    [[ $(file -b "$binary") == *Mach-O*arm64* ]] || { echo "Unexpected runtime architecture: $binary" >&2; exit 1; }
done
printf 'Bundle: %s\nBuild: %s\n' "$app" "$build_id"
[[ $mode != --check ]] || exit 0

# Validate the selected stored credentials before copying/signing the app.
xcrun notarytool history --keychain-profile "$profile" --output-format json >/dev/null
mkdir -p "$output"
output=$(cd -- "$output" && pwd)
work=$(mktemp -d "$output/signing-${build_id:0:12}.XXXXXX")
signed="$work/Allegiance.app"
ditto "$app" "$signed"
# Sign nested code first, then the containing app. Library validation stays
# enabled: all bundled code receives this same Developer ID team signature.
for binary in "$signed"/Contents/Frameworks/*.dylib "$signed"/Contents/MacOS/*; do
    codesign --force --sign "$identity" --options runtime --timestamp "$binary"
done
codesign --force --sign "$identity" --options runtime --timestamp "$signed"
codesign --verify --deep --strict --verbose=2 "$signed"
codesign -dv --verbose=4 "$signed" 2> "$work/signature.txt"
grep -q "^TeamIdentifier=$team$" "$work/signature.txt" || { echo "Signed app has the wrong team; see $work/signature.txt" >&2; exit 1; }

upload="$work/notarization-upload.zip"
ditto -c -k --sequesterRsrc --keepParent "$signed" "$upload"
shasum -a 256 "$upload" > "$work/notarization-upload.sha256"
result="$work/notarization-result.json"
if ! xcrun notarytool submit "$upload" --keychain-profile "$profile" --wait --output-format json > "$result"; then
    if submission=$(plutil -extract id raw -o - "$result" 2>/dev/null); then
        xcrun notarytool log "$submission" --keychain-profile "$profile" "$work/notarization-log.json" || true
    fi
    echo "Notarization failed; logs retained in $work" >&2
    exit 1
fi
status=$(plutil -extract status raw -o - "$result")
submission=$(plutil -extract id raw -o - "$result")
xcrun notarytool log "$submission" --keychain-profile "$profile" "$work/notarization-log.json"
[[ $status == Accepted ]] || { echo "Notarization status $status; see $work/notarization-log.json" >&2; exit 1; }
xcrun stapler staple "$signed"
xcrun stapler validate "$signed"
codesign --verify --deep --strict --verbose=2 "$signed"
spctl --assess --type execute --verbose=2 "$signed"

# Build the deliverable after stapling, so the ZIP carries the offline ticket.
archive="$output/FreeAllegiance-macos-arm64-${build_id:0:12}.zip"
ditto -c -k --sequesterRsrc --keepParent "$signed" "$archive"
shasum -a 256 "$archive" > "$archive.sha256"
printf '\nSigned, notarized release: %s\nSubmission/logs: %s\n' "$archive" "$work"
