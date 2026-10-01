#!/usr/bin/env python3
"""Assemble a relocatable app and rewrite only its copied Mach-O dependencies."""
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('repo', type=Path)
parser.add_argument('build', type=Path)
parser.add_argument('deps', type=Path)
parser.add_argument('package', type=Path)
parser.add_argument('--include-checks', action='store_true', help='Include development diagnostics in the app')
args = parser.parse_args()
repo, build, deps, package = (p.resolve() for p in (args.repo,args.build,args.deps,args.package))
if package in (repo, build, deps) or package in repo.parents:
    raise SystemExit('Package must be a separate output directory')
app = package / 'Allegiance.app.staging'
if app.exists():
    shutil.rmtree(app)
macos = app / 'Contents/MacOS'
frameworks = app / 'Contents/Frameworks'
resources = app / 'Contents/Resources'
for d in (macos,frameworks,resources):
    d.mkdir(parents=True)
queue = []
programs = ['allegiance-native','allegiance-server','allegiance-directory']
if args.include_checks:
    programs += ['allegiance-session-check',
             'allegiance-loadouts-check','allegiance-inventory-check',
             'allegiance-render-check','allegiance-network-check',
             'allegiance-ui-script-check','allegiance-players-check','allegiance-training-check','allegiance-controls-check','allegiance-keyboard-check',
             'allegiance-audio-check','allegiance-movie-check','allegiance-presentation-check',
             'allegiance-frustum-check','allegiance-scale-check','allegiance-teleport-check','allegiance-console-check','allegiance-player-pane-check','allegiance-game-state-check','allegiance-crew-check','allegiance-endgame-check']
for name in programs:
    source = build / 'src/native' / name
    if source.is_file():
        target = macos / name
        shutil.copy2(source,target)
        queue.append(target)
    elif name in ('allegiance-native','allegiance-server','allegiance-directory'):
        raise SystemExit(f'Missing release executable: {source}')


def dependencies(binary):
    lines = subprocess.check_output(['llvm-otool','-L',str(binary)],text=True).splitlines()[1:]
    return [s.strip().split(' (')[0] for s in lines]


def rpaths(binary):
    commands = subprocess.check_output(['llvm-otool','-l',str(binary)],text=True)
    paths = []
    for command in commands.split('Load command '):
        if 'cmd LC_RPATH' in command:
            paths.extend(line.strip()[5:].split(' (offset ')[0]
                         for line in command.splitlines() if line.strip().startswith('path '))
    return paths


media=deps.parent/"ffmpeg"
library_sources={}
for root in (deps,media):
    for path in root.rglob("*.dylib"):
        library_sources.setdefault(path.name,[]).append(path)

def source_for(name):
    if name.startswith(("@rpath/","@loader_path/")):
        matches=library_sources.get(Path(name).name,[])
        if not matches:
            raise SystemExit(f"Missing relative runtime dependency: {name}")
        hashes={hashlib.sha256(p.read_bytes()).hexdigest() for p in matches if p.is_file()}
        if len(hashes)!=1:
            raise SystemExit(f"Ambiguous relative runtime dependency: {name}")
        return matches[0]
    if name.startswith('/opt/homebrew/'):
        path = deps / name.removeprefix('/opt/homebrew/')
    elif name.startswith((str(deps) + '/',str(media) + '/')):
        path = Path(name)
    else:
        raise SystemExit(f'Unresolved non-system dependency: {name}')
    if not path.is_file():
        raise SystemExit(f'Missing dependency {name}: {path}')
    return path

copied = {}


def library(source):
    # Keep real versioned names: install IDs and indirect imports often differ
    # from the unversioned filename used to link the application.
    names = dependencies(source)
    install_id = names[0]
    name = Path(install_id).name
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    if name in copied:
        if copied[name] != digest:
            raise SystemExit(f'Conflicting libraries with the same name: {name}')
    else:
        copied[name] = digest
        target = frameworks / name
        shutil.copy2(source,target)
        target.chmod(0o755)
        queue.append(target)
    return name

library(deps / 'opt/molten-vk/lib/libMoltenVK.dylib')
library(deps / 'opt/vulkan-loader/lib/libvulkan.1.dylib')
while queue:
    binary = queue.pop(0)
    changes = []
    # Bundle imports are self-contained. Do not retain Linux build paths or
    # Homebrew search paths ahead of the bundled framework directory.
    for old in rpaths(binary):
        changes.extend(['-delete_rpath',old])
    dylib = binary.parent == frameworks
    for old in dependencies(binary):
        if old.startswith('/usr/lib/') or old.startswith('/System/Library/'):
            continue
        name = library(source_for(old))
        new = ('@loader_path/' if dylib else '@rpath/') + name
        changes.extend(['-change',old,new])
    if dylib:
        changes.extend(['-id','@rpath/'+binary.name])
    else:
        changes.extend(['-add_rpath','@executable_path/../Frameworks'])
    subprocess.run(['llvm-install-name-tool',*changes,str(binary)],check=True)

# Include the license notices supplied with the dependency snapshot. FFmpeg is
# built separately with only LGPL decoders; its source/license remains cached.
notices=resources/'dependencies'
notices.mkdir()
for prefix in (deps/'opt').iterdir():
    if not prefix.is_dir() or prefix.name=='ffmpeg':continue
    for pattern in ('LICENSE*','COPYING*','Copyright*','copyright*'):
        for notice in prefix.glob(pattern):
            if notice.is_file():
                target=notices/prefix.name
                target.mkdir(exist_ok=True)
                shutil.copy2(notice,target/notice.name)
for name in ('COPYING.LGPLv2.1','COPYING.LGPLv3','LICENSE.md'):
    source=deps.parent/'ffmpeg-src'/name
    if source.is_file():
        target=notices/'ffmpeg';target.mkdir(exist_ok=True)
        shutil.copy2(source,target/name)
source=repo.parent/'GameNetworkingSockets/LICENSE'
if source.is_file():shutil.copy2(source,notices/'GameNetworkingSockets.txt')

shader_dir = resources / 'shaders'
shader_dir.mkdir()
for shader in (build / 'src/native/shaders').glob('*.spv'):
    shutil.copy2(shader,shader_dir / shader.name)
if not any(shader_dir.iterdir()):
    raise SystemExit('Missing compiled shaders')
(resources / 'MoltenVK_icd.json').write_text(json.dumps({
    'file_format_version':'1.0.0',
    'ICD':{'library_path':'../Frameworks/libMoltenVK.dylib','api_version':'1.4.0',
           'is_portability_driver':True}},indent=2)+'\n')
with (app / 'Contents/Info.plist').open('wb') as f:
    plistlib.dump({'CFBundleExecutable':'allegiance-native','CFBundleIdentifier':'org.freeallegiance.Native',
        'CFBundleName':'Allegiance','CFBundleDisplayName':'Allegiance','CFBundlePackageType':'APPL',
        'CFBundleVersion':'1','CFBundleShortVersionString':'0.1','LSMinimumSystemVersion':'26.0',
        'NSHighResolutionCapable':True,'NSSupportsAutomaticGraphicsSwitching':True,
        'NSPrincipalClass':'NSApplication'},f)
shutil.copy2(repo / 'LICENSE',resources / 'LICENSE.txt')
shutil.copy2(repo / 'docs/native-macos.md',resources / 'README.md')
shutil.copy2(repo / 'docs/native-ui-parity.md',resources / 'native-ui-parity.md')
identity=(build/'src/native/network_build.hpp').read_text().split('"')[1]
(resources/'build-info.json').write_text(json.dumps({
    'platform':'macos-arm64','minimum_macos':'26.0','network_build':identity,
    'steam_assets_included':False,'release_programs':programs,
},indent=2)+'\n')
destination=package/'Allegiance.app'
if destination.exists():shutil.rmtree(destination)
app.rename(destination)
print(f'{destination}: {len(copied)} runtime libraries, arm64, macOS >=26.0')
