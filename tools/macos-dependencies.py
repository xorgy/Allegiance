#!/usr/bin/env python3
"""Inventory Mac build headers/configs plus the exact Mach-O runtime closure."""
import json
import os
from pathlib import Path
import subprocess

root = Path('/opt/homebrew')
formulas = ['sdl3', 'openal-soft', 'glm', 'protobuf', 'abseil', 'freetype',
            'lz4', 'openssl@3', 'vulkan-headers', 'vulkan-loader',
            'molten-vk', 'zlib', 'libpng', 'brotli', 'bzip2']
files = set()
versions = {}
queue = []
for formula in formulas:
    prefix = root / 'opt' / formula
    if not prefix.exists():
        if formula in ['zlib', 'bzip2']:
            continue  # Also available in the Apple SDK.
        raise SystemExit(f'Missing Homebrew dependency: {formula}')
    versions[formula] = str(prefix.resolve())
    for subdir in ['include', 'lib', 'share/cmake', 'share/glm', 'share/vulkan']:
        directory = prefix / subdir
        if directory.exists():
            files.add(str(directory.relative_to(root)) + '/')
    if formula == 'protobuf':
        files.add('opt/protobuf/bin/')
    for pattern in ['LICENSE*', 'COPYING*', 'Copyright*', 'copyright*']:
        for notice in prefix.glob(pattern):
            if notice.is_file():files.add(str(notice.relative_to(root)))
    queue.extend(prefix.glob('lib/*.dylib'))
seen = set()
while queue:
    lib = queue.pop()
    if lib.resolve() in seen:
        continue
    seen.add(lib.resolve())
    files.add(str(lib.relative_to(root)))
    output = subprocess.check_output(['otool', '-L', str(lib)], text=True)
    for line in output.splitlines()[1:]:
        dependency = line.strip().split(' (')[0]
        if dependency.startswith(str(root) + '/'):
            path = Path(dependency)
            if not path.exists():
                raise SystemExit(f'Missing runtime dependency {path}, required by {lib}')
            queue.append(path)
            files.add(str(path.relative_to(root)))
print(json.dumps({'files': sorted(files), 'formulas': versions,
                  'sdk': subprocess.check_output(['xcrun', '--sdk', 'macosx', '--show-sdk-path'], text=True).strip(),
                  'protoc': subprocess.check_output([str(root / 'bin/protoc'), '--version'], text=True).strip().split()[-1]}, indent=2))
