# Arch Linux package

The `freeallegiance-native` package contains the Vulkan client, game server,
public directory service, compiled shaders, and desktop entry. It uses Arch's
system libraries and statically links our modified GameNetworkingSockets code,
including the optional io_uring backend. It targets generic x86-64 CPUs with
ThinLTO, without fast-math or machine-specific instructions.

Install the generated package with:

```sh
sudo pacman -U /path/to/freeallegiance-native-*.pkg.tar.zst
allegiance-native
```

Steam game assets are not included. The client and server read the installed
data from `~/.local/share/Steam/steamapps/common/Allegiance/Artwork`.
For another Steam library, supply `--artwork /path/to/Allegiance/Artwork`.
A compatible Vulkan driver and a system sans-serif font are required;
Verdana is preferred and DejaVu provides a fallback.

The package also supplies `allegiance-server` and `allegiance-directory`.
Each program documents its command-line options with `--help`. Services do
not start automatically. Network peers must use the same network build;
`/usr/share/freeallegiance/build-info.json` records that identity and the source
snapshot used to build the package.

## Rebuild

From the Allegiance repository, with the modified GNS checkout in the adjacent
`../GameNetworkingSockets` directory:

```sh
python tools/build-arch.py
```

Arch's `base-devel` group and the PKGBUILD's build/runtime dependencies must
already be installed. The script creates checksummed source snapshots,
`PKGBUILD`, `.SRCINFO`, a binary `.pkg.tar.zst`, and a complete `.src.tar.gz`
under `build/arch`. It includes current working-tree source changes, so the
source package can rebuild this release without access to either repository.
No Steam assets, local PGO profile, or prebuilt executables are copied into the
package. Build output can be redirected with `--output`; `--gns` selects another
modified GNS checkout. `--prepare-only` writes the inputs for inspection.

To rebuild from the generated source package:

```sh
bsdtar -xf freeallegiance-native-*.src.tar.gz
cd freeallegiance-native
makepkg
```

The checks run the existing affinity, frustum, presentation and shared arena
tests, plus the three executable startup/help paths. Building does not install
the package or change system dependencies.
