#!/usr/bin/env python3
"""Package only release programs, runtime libraries and notices; no game data."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
from zipfile import ZipFile, ZIP_DEFLATED

parser = argparse.ArgumentParser(description=__doc__)
for name in ('repo', 'build', 'vcpkg', 'package'):
    parser.add_argument(name, type=Path)
args = parser.parse_args()
repo, build, vcpkg, package = (getattr(args, n).resolve() for n in ('repo', 'build', 'vcpkg', 'package'))
if package in (repo, build, vcpkg) or package in repo.parents:
    raise SystemExit('Package must be a separate output directory')
staging = package.with_name(package.name + '.staging')
if staging.exists():
    shutil.rmtree(staging)
binary_dir = staging / 'bin'
notices = staging / 'share/freeallegiance/dependencies'
binary_dir.mkdir(parents=True)
notices.mkdir(parents=True)
installed = vcpkg / 'installed/x64-windows-clang'
queue = []
for name in ('allegiance-native.exe', 'allegiance-server.exe', 'allegiance-directory.exe'):
    source = build / 'src/native' / name
    if not source.is_file():
        raise SystemExit(f'Missing release executable: {source}')
    target = binary_dir / name
    shutil.copy2(source, target)
    queue.append(target)
libraries = {p.name.lower(): p for p in (installed / 'bin').glob('*.dll')}
copied = set()
external = set()
while queue:
    binary = queue.pop()
    imports = subprocess.check_output(['llvm-readobj', '--coff-imports', str(binary)], text=True)
    for name in re.findall(r'^\s+Name: (.+\.dll)\s*$', imports, re.MULTILINE | re.IGNORECASE):
        key = name.lower()
        # The display driver's Vulkan loader must come from the target system.
        if key == 'vulkan-1.dll' or key not in libraries:
            external.add(key)
            continue
        if key not in copied:
            copied.add(key)
            target = binary_dir / libraries[key].name
            shutil.copy2(libraries[key], target)
            queue.append(target)
shaders = staging / 'share/freeallegiance/shaders'
shaders.mkdir(parents=True)
shader_files = sorted((build / 'src/native/shaders').glob('*.spv'))
if not shader_files:
    raise SystemExit('Missing compiled shaders')
for source in shader_files:
    shutil.copy2(source, shaders / source.name)
for dependency in ('sdl3', 'openal-soft', 'fmt', 'ffmpeg', 'freetype', 'glm', 'lz4', 'abseil', 'protobuf', 'vulkan-headers', 'zlib'):
    source = installed / 'share' / dependency / 'copyright'
    if source.is_file():
        shutil.copy2(source, notices / (dependency + '.txt'))
cache = (build / 'CMakeCache.txt').read_text()
gns = re.search(r'^FETCHCONTENT_SOURCE_DIR_GNS:[^=]*=(.+)$', cache, re.MULTILINE)
gns_source = Path(gns[1]) if gns else build / '_deps/gns-src'
shutil.copy2(gns_source / 'LICENSE', notices / 'GameNetworkingSockets.txt')
shutil.copy2(repo / 'LICENSE', staging / 'LICENSE.txt')
shutil.copy2(repo / 'docs/native-windows.md', staging / 'README.md')
shutil.copy2(repo / 'docs/native-ui-parity.md', staging / 'native-ui-parity.md')
(staging / 'play-allegiance.cmd').write_bytes(b'@echo off\r\n"%~dp0bin\\allegiance-native.exe" %*\r\n')
identity = re.search(r'"([0-9a-f]{64})"', (build / 'src/native/network_build.hpp').read_text())[1]
(staging / 'build-info.json').write_text(json.dumps({
    'platform': 'windows-x86_64', 'network_build': identity, 'steam_assets_included': False,
    'system_dlls': sorted(external),
    'sha256': {str(p.relative_to(staging)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in sorted(staging.rglob('*')) if p.is_file()},
}, indent=2) + '\n')
if package.exists():
    shutil.rmtree(package)
staging.rename(package)
archive_path = package.with_name(package.name + '.zip')
with ZipFile(archive_path, 'w', ZIP_DEFLATED, compresslevel=6) as archive:
    for path in sorted(package.rglob('*')):
        if path.is_file():
            archive.write(path, Path(package.name) / path.relative_to(package))
print(f'{archive_path}: 3 release programs, {len(copied)} runtime DLLs, {len(shader_files)} shaders; no Steam assets')
