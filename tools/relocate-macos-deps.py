#!/usr/bin/env python3
"""Relocate staged build metadata and isolate host tools from Mac binaries."""
import argparse
import json
from pathlib import Path
import re
import shlex
import shutil
import stat

parser=argparse.ArgumentParser()
parser.add_argument('cache', type=Path)
args=parser.parse_args()
cache=args.cache.resolve()
prefix=cache/'homebrew'
manifest=json.loads((cache/'manifest.json').read_text())
for file in prefix.rglob('*'):
    if not file.is_file() or file.suffix not in ('.pc','.cmake'):
        continue
    file.chmod(file.stat().st_mode | stat.S_IWUSR)
    text=file.read_text()
    text=re.sub(r'/opt/homebrew/Cellar/([^/]+)/[^/\s";]+',lambda m:str(prefix/'opt'/m[1]),text)
    text=text.replace('/opt/homebrew',str(prefix))
    if file.suffix=='.pc' and file.relative_to(prefix).parts[0]=='opt':
        formula=file.relative_to(prefix).parts[1]
        text=re.sub(r'^prefix=.*$', 'prefix='+str(prefix/'opt'/formula),text,flags=re.M)
        text=re.sub(r'^exec_prefix=.*$', 'exec_prefix=${prefix}',text,flags=re.M)
        # Some Homebrew .pc templates embed the original prefix twice.
        if formula=='abseil':
            text=re.sub(r'^libdir=.*$', 'libdir=${prefix}/lib',text,flags=re.M)
            text=re.sub(r'^includedir=.*$', 'includedir=${prefix}/include',text,flags=re.M)
    file.write_text(text)
# Apple's SDK provides zlib but no .pc file. Supply metadata for this SDK's
# actual version; linking remains through the compiler's sysroot.
sdk=cache/'sdk'/Path(manifest['sdk']).name
version=re.search(r'#define ZLIB_VERSION "([^"]+)"',(sdk/'usr/include/zlib.h').read_text())[1]
pcs=prefix/'pkgconfig';pcs.mkdir(exist_ok=True)
(pcs/'zlib.pc').write_text(f'Name: zlib\nDescription: Apple SDK zlib\nVersion: {version}\nLibs: -lz\nCflags:\n')
for file in (prefix/'opt/protobuf/bin').glob('protoc*'):
    if file.name=='protoc' or re.fullmatch(r'protoc-[\d.]+',file.name):
        file.chmod(0o755)
        shutil.copy2(cache/'host-tools/bin/protoc',file)
dirs=':'.join([str(cache/'ffmpeg/lib/pkgconfig'),str(pcs)]+[str(p) for p in prefix.glob('opt/*/lib/pkgconfig')])
wrapper=cache/'host-tools/pkg-config'
wrapper.write_text('#!/bin/sh\nexport PKG_CONFIG_LIBDIR='+shlex.quote(dirs)+'\nunset PKG_CONFIG_PATH\nexec pkg-config "$@"\n')
wrapper.chmod(0o755)
