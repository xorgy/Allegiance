#!/usr/bin/env python3
"""Build an Arch package from this checkout and the adjacent modified GNS tree."""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def digest(path):
    with path.open('rb') as content:
        return hashlib.file_digest(content, 'sha256').hexdigest()


def snapshot(repo, archive, prefix, epoch, paths=()):
    # Include working-tree changes, including new GNS io_uring sources. Never
    # copy .git, ignored builds, caches, or files outside the selected sources.
    inventory = git(repo, 'ls-files', '-z', '--cached', '--others',
                    '--exclude-standard', '--', *paths)
    names = sorted(set(os.fsdecode(n) for n in inventory.split(b'\0') if n))
    with archive.open('wb') as raw:
        with gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode='w') as tar:
                for name in names:
                    path = repo / name
                    if not path.is_file() and not path.is_symlink():
                        continue  # Deleted files or disabled, uninitialized submodules.
                    info = tar.gettarinfo(str(path), f'{prefix}/{name}')
                    info.uid = info.gid = 0
                    info.uname = info.gname = 'root'
                    info.mtime = epoch
                    info.mode = 0o755 if info.mode & 0o111 else 0o644
                    if info.isfile():
                        with path.open('rb') as content:
                            tar.addfile(info, content)
                    else:
                        tar.addfile(info)


parser = argparse.ArgumentParser(description=__doc__)
repo = Path(__file__).resolve().parents[1]
parser.add_argument('--gns', type=Path, default=repo.parent / 'GameNetworkingSockets')
parser.add_argument('--output', type=Path, default=repo / 'build/arch')
parser.add_argument('--prepare-only', action='store_true', help='Write snapshots and PKGBUILD without building')
args = parser.parse_args()
gns, output = args.gns.resolve(), args.output.resolve()
if output in (repo, gns) or output in repo.parents or output in gns.parents:
    parser.error('Output must be a separate packaging directory')
if not (gns / 'src/steamnetworkingsockets/clientlib/steamnetworkingsockets_iouring.cpp').is_file():
    parser.error('--gns must point to our GNS checkout with the io_uring backend')
for command in ('git', 'makepkg'):
    if not shutil.which(command):
        parser.error(f'Missing prerequisite: {command}')
output.mkdir(parents=True, exist_ok=True)
epoch = int(os.environ.get('SOURCE_DATE_EPOCH', git(repo, 'show', '-s', '--format=%ct', 'HEAD').strip()))
allegiance_archive = output / 'freeallegiance-source.tar.gz'
gns_archive = output / 'GameNetworkingSockets-source.tar.gz'
snapshot(repo, allegiance_archive, 'freeallegiance', epoch, (
    'CMakeLists.txt', 'LICENSE', 'README.md', 'cmake', 'docs', 'src/native',
    'src/Igc', 'src/ClientCore', 'src/zlib', 'src/_Utility', 'src/training',
    'src/soundengine', 'src/WinTrek/mappvmaker.cpp', 'src/WinTrek/mappvmaker.h',
    'src/Lib/nlohmann/json',
))
snapshot(gns, gns_archive, 'GameNetworkingSockets', epoch)
allegiance_sha, gns_sha = digest(allegiance_archive), digest(gns_archive)
revision = git(repo, 'rev-parse', 'HEAD').decode().strip()
combined = hashlib.sha256((allegiance_sha + gns_sha).encode()).hexdigest()
version = f'0.1.r{git(repo, "rev-list", "--count", "HEAD").decode().strip()}.g{revision[:8]}.s{combined[:12]}'
provenance = output / 'source-snapshot.json'
provenance.write_text(json.dumps({
    'allegiance_revision': revision,
    'allegiance_source_sha256': allegiance_sha,
    'gns_revision': git(gns, 'rev-parse', 'HEAD').decode().strip(),
    'gns_source_sha256': gns_sha,
    'working_tree_sources': True,
    'source_date_epoch': epoch,
}, indent=2) + '\n')
template = (repo / 'packaging/arch/PKGBUILD.in').read_text()
for key, value in {
    'PKGVER': version, 'ALLEGIANCE_SHA256': allegiance_sha,
    'GNS_SHA256': gns_sha, 'SNAPSHOT_SHA256': digest(provenance),
}.items():
    template = template.replace(f'@{key}@', value)
(output / 'PKGBUILD').write_text(template)
environment = os.environ.copy()
environment['SOURCE_DATE_EPOCH'] = str(epoch)
(output / '.SRCINFO').write_bytes(subprocess.check_output(
    ['makepkg', '--printsrcinfo'], cwd=output, env=environment))
print(f'Prepared {output}/PKGBUILD ({version})', flush=True)
if not args.prepare_only:
    # No installation, sudo, dependency mutation, or automatic service startup.
    subprocess.run(['makepkg', '--force', '--cleanbuild', '--noconfirm'],
                   cwd=output, env=environment, check=True)
    subprocess.run(['makepkg', '--allsource', '--force', '--noconfirm'],
                   cwd=output, env=environment, check=True)
    for path in sorted(output.glob('*.tar.*')):
        if '.pkg.' in path.name or '.src.' in path.name:
            path.with_name(path.name + '.sha256').write_text(f'{digest(path)}  {path.name}\n')
            print(path)
