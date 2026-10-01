#!/usr/bin/env bash
# Linux LLVM cross-build for Apple Silicon/macOS 26. Uses an authorized Mac's
# Xcode SDK and Homebrew libraries; signing/notarization is a later Mac step.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cache=${ALLEGIANCE_MACOS_CACHE:-"$repo/build/macos-deps"}
build=${ALLEGIANCE_MACOS_BUILD:-"$repo/build/macos-clang"}
package=${ALLEGIANCE_MACOS_PACKAGE:-"$repo/build/macos-package"}
host=${MACOS_HOST:-}
refresh=false
checks=false
for arg in "$@"; do
    case "$arg" in
        --refresh-deps) refresh=true ;;
        --include-checks) checks=true ;;
        --help) echo 'build-macos.sh [--refresh-deps] [--include-checks] (set MACOS_HOST=user@mac-host to prepare/refresh dependencies)'; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done
for tool in clang clang++ ld64.lld llvm-ar llvm-ranlib llvm-nm llvm-strip make sha512sum llvm-otool llvm-install-name-tool cmake ninja python3 pkg-config glslc rsync ssh curl; do
    command -v "$tool" >/dev/null || { echo "Missing host tool: $tool" >&2; exit 1; }
done
mkdir -p "$cache/host-tools" "$cache/sdk"
# Skip synchronization for repeat builds once the dependency snapshot exists.
# --refresh-deps explicitly updates it after changing Xcode/Homebrew on the Mac.
if [[ ! -f "$cache/manifest.json" || $refresh == true ]]; then
    [[ -n $host ]] || { echo 'Set MACOS_HOST=user@mac-host to prepare or refresh the Mac dependency snapshot.' >&2; exit 1; }
    ssh -F /dev/null "$host" 'export PATH=/opt/homebrew/bin:$PATH; HOMEBREW_NO_AUTO_UPDATE=1 brew install sdl3 openal-soft glm protobuf freetype lz4 openssl@3 vulkan-headers vulkan-loader molten-vk'
    ssh -F /dev/null "$host" /usr/bin/python3 - < "$repo/tools/macos-dependencies.py" > "$cache/manifest.json"
    python3 - "$cache" <<'PY'
import json,sys
from pathlib import Path
p=Path(sys.argv[1]);m=json.loads((p/'manifest.json').read_text())
(p/'files.txt').write_text('\n'.join(m['files'])+'\n')
PY
    sdk=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["sdk"])' "$cache/manifest.json")
    rsync -aL -e 'ssh -F /dev/null' "$host:$sdk/" "$cache/sdk/$(basename "$sdk")/"
    rsync -aLr --files-from="$cache/files.txt" -e 'ssh -F /dev/null' "$host:/opt/homebrew/" "$cache/homebrew/"
    version=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["protoc"])' "$cache/manifest.json")
    curl --fail --location --silent --show-error "https://github.com/protocolbuffers/protobuf/releases/download/v$version/protoc-$version-linux-x86_64.zip" -o "$cache/host-tools/protoc.zip"
    python3 - "$cache" <<'PY'
from zipfile import ZipFile
from pathlib import Path
import sys
p=Path(sys.argv[1])/'host-tools'
with ZipFile(p/'protoc.zip') as z:z.extract('bin/protoc',p)
(p/'bin/protoc').chmod(0o755)
PY
    python3 "$repo/tools/relocate-macos-deps.py" "$cache"
fi
export MACOS_DEPS_PREFIX="$cache/homebrew"
export MACOS_SDK="$cache/sdk/$(python3 -c 'import json,sys;from pathlib import Path;print(Path(json.load(open(sys.argv[1]))["sdk"]).name)' "$cache/manifest.json")"
"$repo/tools/build-macos-ffmpeg.sh" "$cache" "$MACOS_SDK"
python3 "$repo/tools/relocate-macos-deps.py" "$cache"
prefixes=$(python3 - "$cache" <<'PY'
from pathlib import Path
import sys
print(';'.join([str((Path(sys.argv[1])/'ffmpeg').resolve())]+[str(p.resolve()) for p in (Path(sys.argv[1])/'homebrew/opt').iterdir() if p.is_dir() and p.name!='ffmpeg']))
PY
)
extra=()
if [[ -n ${GNS_SOURCE:-} ]]; then
    extra+=("-DFETCHCONTENT_SOURCE_DIR_GNS=$GNS_SOURCE")
elif [[ -d "$repo/../GameNetworkingSockets" ]]; then
    extra+=("-DFETCHCONTENT_SOURCE_DIR_GNS=$repo/../GameNetworkingSockets")
fi
cmake -S "$repo" -B "$build" -G Ninja -U '*FFMPEG*' \
    -DCMAKE_TOOLCHAIN_FILE="$repo/cmake/toolchain-macos-clang.cmake" \
    -DCMAKE_PREFIX_PATH="$prefixes" -DPKG_CONFIG_EXECUTABLE="$cache/host-tools/pkg-config" \
    -DVulkan_GLSLC_EXECUTABLE="$(command -v glslc)" \
    -DVulkan_INCLUDE_DIR="$cache/homebrew/opt/vulkan-headers/include" \
    -DVulkan_LIBRARY="$cache/homebrew/opt/vulkan-loader/lib/libvulkan.dylib" \
    -DALLEGIANCE_NATIVE_CLIENT=ON -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON "${extra[@]}"
targets=(allegiance-native allegiance-server allegiance-directory)
package_options=()
if [[ $checks == true ]]; then
    package_options+=(--include-checks)
    targets+=(allegiance-session-check allegiance-render-check \
    allegiance-network-check allegiance-ui-script-check allegiance-training-check allegiance-controls-check allegiance-keyboard-check \
    allegiance-audio-check allegiance-movie-check allegiance-presentation-check \
    allegiance-frustum-check allegiance-scale-check allegiance-teleport-check allegiance-console-check allegiance-player-pane-check allegiance-game-state-check allegiance-crew-check allegiance-inventory-check allegiance-endgame-check)
fi
cmake --build "$build" --parallel "${ALLEGIANCE_BUILD_JOBS:-8}" --target "${targets[@]}"
python3 "$repo/tools/package-macos.py" "$repo" "$build" "$cache/homebrew" "$package" "${package_options[@]}"
printf 'macOS application: %s/Allegiance.app\n' "$package"
