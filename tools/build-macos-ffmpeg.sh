#!/usr/bin/env bash
# Cross-build LGPL FFmpeg without the unrelated Homebrew codec dependency tree.
set -euo pipefail
cache=$1
sdk=$2
archive="$cache/ffmpeg-n9.0.2.tar.gz"
signature=avi-mpeg4-wmav2-v1
if [[ ! -f "$cache/ffmpeg/configuration" || $(cat "$cache/ffmpeg/configuration") != "$signature" ]]; then
    if [[ ! -f "$archive" ]]; then
        if [[ -n ${FFMPEG_SOURCE_ARCHIVE:-} ]]; then
            cp "$FFMPEG_SOURCE_ARCHIVE" "$archive"
        else
            curl --fail --location --silent --show-error \
                https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n9.0.2.tar.gz -o "$archive"
        fi
    fi
    printf '%s  %s\n' 21bf3fbcdfd2f41ea6edeab40433fecfe362b09ecaa177a652471b54f9357011b4f74dab18245ab1c26f1a473b94205490baecb42289afd2effd603cd0b22b59 "$archive" | sha512sum --check
    mkdir -p "$cache/ffmpeg-src" "$cache/ffmpeg-build"
    tar -xf "$archive" -C "$cache/ffmpeg-src" --strip-components=1
    cd "$cache/ffmpeg-build"
    ../ffmpeg-src/configure --prefix="$cache/ffmpeg" \
        --arch=aarch64 --target-os=darwin --enable-cross-compile \
        --cc=clang --cxx=clang++ --ar=llvm-ar --ranlib=llvm-ranlib --nm=llvm-nm --strip=llvm-strip \
        --sysroot="$sdk" --extra-cflags='--target=arm64-apple-macos26.0' \
        --extra-ldflags='--target=arm64-apple-macos26.0 -fuse-ld=lld' \
        --enable-shared --disable-static --install-name-dir=@rpath \
        --disable-autodetect --disable-programs --disable-doc --disable-network \
        --disable-avdevice --disable-avfilter --disable-everything \
        --enable-protocol=file --enable-demuxer=avi --enable-decoder=mpeg4,wmav2 \
        --enable-parser=mpeg4video --disable-videotoolbox
    make --jobs="${ALLEGIANCE_BUILD_JOBS:-8}"
    make install
    printf '%s\n' "$signature" > "$cache/ffmpeg/configuration"
fi
