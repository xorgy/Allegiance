#!/usr/bin/env bash
# Cross-build the portable client with clang-cl, lld-link and the MSVC SDK.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cache=${ALLEGIANCE_WINDOWS_CACHE:-"$repo/build/windows-deps"}
build=${ALLEGIANCE_WINDOWS_BUILD:-"$repo/build/windows-clang"}
package=${ALLEGIANCE_WINDOWS_PACKAGE:-"$repo/build/windows-package"}
mkdir -p "$cache"
for program in clang-cl lld-link llvm-lib llvm-rc llvm-mt llvm-readobj cmake ninja git python3 glslc pkg-config; do
    command -v "$program" >/dev/null || { echo "Missing host tool: $program" >&2; exit 1; }
done

# An isolated SDK avoids modifying a user's cargo-xwin installation/cache.
# cargo-xwin 0.19 cannot extract current SDKs; pin the tested bootstrap tool.
if [[ -z ${XWIN_ROOT:-} ]]; then
    if [[ ! -x "$cache/xwin-tool/bin/cargo-xwin" ]]; then
        python3 -m pip install --target "$cache/xwin-tool" cargo-xwin==0.23.1
    fi
    XWIN_CACHE_DIR="$cache/sdk" XWIN_ARCH=x86_64 "$cache/xwin-tool/bin/cargo-xwin" xwin env --target x86_64-pc-windows-msvc > "$cache/xwin-env.txt"
    export XWIN_ROOT="$cache/sdk/xwin"
fi
export XWIN_ROOT

vcpkg=${VCPKG_ROOT:-"$cache/vcpkg"}
if [[ ! -d "$vcpkg/.git" ]]; then
    git clone https://github.com/microsoft/vcpkg.git "$vcpkg"
    git -C "$vcpkg" checkout 19780d9cdf84d0944cf9a318666703b89ab6629c
fi
if [[ ! -x "$vcpkg/vcpkg" ]]; then
    "$vcpkg/bootstrap-vcpkg.sh" -disableMetrics
fi
# FFmpeg's MSVC linker wrapper invokes link.exe by name on the host.
mkdir -p "$cache/host-tools"
ln -sf "$(command -v lld-link)" "$cache/host-tools/link.exe"
ln -sf "$(command -v llvm-lib)" "$cache/host-tools/lib.exe"
export PATH="$cache/host-tools:$PATH"

# Stage only the tested FFmpeg configure fix, retaining vcpkg's pinned port.
overlay="$cache/ports"
mkdir -p "$overlay"
rm -rf -- "$overlay/ffmpeg"
cp -R "$vcpkg/ports/ffmpeg" "$overlay/ffmpeg"
cp "$repo/cmake/patches/ffmpeg-llvm-lib.patch" "$overlay/ffmpeg/"
python3 - "$overlay/ffmpeg/portfile.cmake" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
marker = '        0052-fix-disable-unstable-swscale-link.patch'
if marker not in s:
    raise SystemExit('Unsupported vcpkg baseline: FFmpeg patch list changed')
p.write_text(s.replace(marker, '        ffmpeg-llvm-lib.patch\n' + marker, 1))
PY
export VCPKG_MAX_CONCURRENCY=${VCPKG_MAX_CONCURRENCY:-12}
"$vcpkg/vcpkg" install --recurse --triplet x64-windows-clang --overlay-triplets="$repo/cmake/triplets" --overlay-ports="$overlay" \
    'sdl3[vulkan]' openal-soft glm 'freetype[core]' lz4 vulkan-loader protobuf 'ffmpeg[core,avcodec,avformat,swresample,swscale,zlib]'

extra=()
if [[ -n ${GNS_SOURCE:-} ]]; then
    extra+=("-DFETCHCONTENT_SOURCE_DIR_GNS=$GNS_SOURCE")
elif [[ -d "$repo/../GameNetworkingSockets" ]]; then
    extra+=("-DFETCHCONTENT_SOURCE_DIR_GNS=$repo/../GameNetworkingSockets")
fi
cmake -S "$repo" -B "$build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$vcpkg/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET=x64-windows-clang -DVCPKG_MANIFEST_MODE=OFF \
    -DVCPKG_OVERLAY_TRIPLETS="$repo/cmake/triplets" \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$repo/cmake/toolchain-windows-clang.cmake" \
    -DALLEGIANCE_NATIVE_CLIENT=ON -DCMAKE_BUILD_TYPE=Release \
    -DGNS_HOST_PROTOC="$vcpkg/installed/x64-linux/tools/protobuf/protoc" \
    -DProtobuf_USE_STATIC_LIBS=ON "${extra[@]}"
cmake --build "$build" --target allegiance-native allegiance-server allegiance-directory --parallel "$VCPKG_MAX_CONCURRENCY"
python3 "$repo/tools/package-windows.py" "$repo" "$build" "$vcpkg" "$package"
printf 'Windows client: %s/bin/allegiance-native.exe\n' "$package"
printf 'Windows distribution: %s.zip\n' "$package"
