# Native Windows client

The portable Vulkan client and authority now cross-build as x64 Windows
executables with clang-cl, lld-link, the MSVC CRT, and the Windows SDK. This is
the MSVC ABI/toolchain approach used by cargo-xwin; it does not use MinGW.
The original WinTrek build remains a separate, unchanged CMake entry path.

The portable client defaults to the classic interface at its original pixel
size on a 1× display. It follows the platform's requested display scale with
nearest-neighbor integer scaling; midpoint ties round down (150% stays 1×).
Use `--classic-ui-scale 2` (or another integer) to override it, or `auto` to
restore automatic scaling; the requested scale must fit the drawable. `--modern-ui` explicitly
enables the experimental frontend. The original UI restoration queue remains
in [native-ui-parity.md](native-ui-parity.md).

## Build on Linux

Install Clang/LLVM (including clang-cl, lld-link, llvm-lib, llvm-rc and llvm-mt),
CMake, Ninja, Python/pip, Git, pkg-config and glslc. Then run:

```sh
./tools/build-windows.sh
```

The script keeps its SDK, dependency sources and host tools under
`build/windows-deps`, pins cargo-xwin 0.23.1 and a vcpkg baseline, and produces
`build/windows-package` and its distributable `build/windows-package.zip`.
An existing SDK can be selected with `XWIN_ROOT`
(the directory containing `crt/` and `sdk/`); `VCPKG_ROOT` and `GNS_SOURCE`
can select existing dependency workspaces. Our adjacent modified
GameNetworkingSockets workspace is selected automatically when present.

`ALLEGIANCE_WINDOWS_CACHE`, `ALLEGIANCE_WINDOWS_BUILD`, and
`ALLEGIANCE_WINDOWS_PACKAGE` override the three output directories.
`VCPKG_MAX_CONCURRENCY` defaults to 12. The first build downloads the SDK and
builds dependencies; subsequent builds reuse them.

SDL must include its Vulkan feature, and FFmpeg must include zlib for the
original PNG artwork. The overlay contains small LLVM library-tool detection
fixes for FFmpeg's configure and DLL export-generation scripts. OpenAL and
FFmpeg are runtime DLLs; other dependencies use the static MSVC CRT. Dependency
notices accompany the package. The GPU driver supplies `vulkan-1.dll`.

## Run on Windows

Keep the package directories together and run `play-allegiance.cmd`, or:

```bat
bin\allegiance-native.exe
bin\allegiance-native.exe --connect 127.0.0.1:27500
bin\allegiance-server.exe --auto-start
```

Artwork is found through the Windows Steam registry path and library list.
An `Artwork` directory beside the executable or at the package root also works.
For a different installation, pass `--artwork "D:\path\Allegiance\Artwork"`.
The default settings path is `%LOCALAPPDATA%\FreeAllegiance\native.conf`.
The ZIP includes client/server executables, runtime DLLs, shaders and notices;
game artwork comes from the existing Steam installation.

Release packaging starts from a clean staging directory and follows executable
DLL imports, so stale diagnostics, SDK headers/libraries and unrelated DLLs do
not enter the ZIP. `build-info.json` records the shared network build identity
and file hashes. To repackage an existing release build without rebuilding its
dependency cache:

```sh
python3 tools/package-windows.py . build/windows-clang /tmp/allegiance-vcpkg build/windows-package
```

The third argument is the vcpkg checkout used for that build.

The renderer requires Vulkan 1.3 and the descriptor-indexing features used by
the native renderer. Use current GPU drivers. Windows and Linux builds from
the same source revision can connect: the handshake checks source/content
identity and actual wire-record sizes/alignment, rather than requiring the
same host compiler or operating system.

## Wine validation on this machine

```sh
WINEDEBUG=-all WINEDLLOVERRIDES=vulkan-1=b,winedbg.exe=d \
  wine build/windows-package/bin/allegiance-native.exe \
  --artwork 'Z:\home\aaron\.local\share\Steam\steamapps\common\Allegiance\Artwork'
```

Validated with Wine 11.18 on the Radeon 8060S/RADV machine:

* Real Vulkan rendering, a 2,000-frame flight/boost/camera run, and screenshots.
* Vulkan pixel checks for instancing, alpha/additive ordering, textured asteroid
  LODs, radar brackets, HUD cutouts, planet alpha and perspective stars.
* 28,888 keyboard/mouse reference comparisons and 44,615 flight-control comparisons.
* Encrypted Windows client/server throttle, turning, braking, vector-lock and
  reconnect checks at the compile-time 10 Hz publication rate.
* A rendered Windows client joining and launching on an isolated Linux server:
  110 snapshots, no missing baselines, 0.84 m maximum correction in that run.
* OpenAL original sound composition/lifecycle, large voice pools and combat.
* Original FMV decoding, PCM, pause/resume, natural completion and repeated skip.
* Original UI parsing, button layouts, bitmap fonts and launch-animation decoding.
* Original popup menu cascades, shortcuts, immediate preference changes and
  input-popup parent/flight return behavior.
* Original player leader/mute popups and team-screen compositing. Lobby reports
  retain authoritative pilot IDs and role columns, including duplicate names,
  report reordering, departures and chat capture. Encrypted lobby checks verify
  leader/host changes, command wing, unchanged funds/donations and private mute.
  Team changes preserve the match and pilot identities rather than recreating
  the authority. Player checks also cover the No Team departure/re-entry
  foundation, private/shared report arenas and preservation of occupied crew
  hulls. Connected admission controls now cover pending requests, commander
  Accept/Reject, Auto Accept, lobby/side locks, Away and separate force-ready.
* Authored game-settings artwork, twenty-one choice menus, shared WinTrek presets,
  coupled resource/treasure values, draft cancellation including readiness,
  invalid-setting rejection and loss of network-host editing authority.
  Generated previews cover all 140 valid map/team/size combinations and release
  their reference cycles. Popup input capture preserves parent control faces.
* Original input-mapping selection, multiple binds, conflicts, mouse/joystick
  capture, Restore/Cancel, continuous-key/chord semantics and full roster scrolling.
  The virtual joystick/session checks pass in Wine; Vulkan pixel checks cover
  clipped and unclipped original bitmap text.
* Original loadout artwork and Vulkan ship preview, staged browsing/cancellation,
  defaults, preset shortcuts, IGC preview purity, pricing and partial-amount
  equivalence. Hull/part cascades retain original prices, placement and hover
  statistics, with overflow scrolling checked against 600 cargo choices.
  Part-stat backgrounds retain their authored label images and coordinates.
  The rendered Linux refit/launch/session-restart diagnostic passes.
* Original credits popup, bitmap text, scrolling and viewport clipping, with
  reviewed screenshots from the Linux and Wine clients.

The current Wine build also passes the shared teleport/investment/modal policy
checks, original script and keyboard-map checks, and Vulkan pixels for both
consoles, integer anchors/slides, font/shading composition and construction-site
markers. A 700-frame client run verifies actual F5/F8 toggles, launch beneath
investments, continuous keyboard steering with the fresh pane open, Toggle Mouse
release and immediate flight return during the exit animation. These are UI
checks, not presentation-latency measurements.

The original UI restoration is in progress; the Windows executable uses the
same current frontend as Linux. See [the UI parity queue](native-ui-parity.md)
for implemented screens and remaining work. This build does not speak the
legacy WinTrek/FedSrv protocol.

Wine exposes present-wait2 here, but not EXT presentation timing. The Wine run
therefore does not establish precise input-to-display latency or missed-refresh
statistics. Those measurements and real Windows GPU/input/gamepad testing remain
part of the community playtest. The optional Vulkan validation layer is absent
from this Wine installation; the pixel check accepts `--no-validation` after
the artwork argument for that environment.

The updated Windows build passes original F6/mini-team player-pane, console,
script, keymap and session checks under Wine. Vulkan pixel checks and a
1,325-frame actual SDL check cover collapse/expand, context-recipient picking,
private chat, the hangar/flight overlay, launch-camera suppression and held-key
flight steering. Encrypted group/wing/waypoint routes and commander Stop
Donation also pass in the Windows executables. Player-pane art remains at its
authored size, with the shared automatic whole-integer scaling policy.

The player-history/turret follow-up passes closed-pane observation/retirement,
authoritative dock-duration, hull reload, public departure/ID reuse and observed
drone-death checks. The eight original turret swap/promotion commands retain
their imported bindings and hardpoint indices. Encrypted crew boarding, firing,
promotion, death/recovery, sector/console, health and throttle regressions pass
under Wine. The packaged executable also passes the 1,326-frame actual SDL
player-pane check; Vulkan pixel checks use `--no-validation` for Wine's loader.
Linux, Windows and macOS binaries were rebuilt with the same network identity.

The original LAN entry restoration builds both Windows client and server and
passes the headless Winsock discovery check under Wine in an isolated temporary
prefix. It checks simultaneous shared-port authorities, host-local multicast,
stable identities, duplicate reply suppression and responder teardown. The
UI/metadata/expiry checks are recorded in [the UI parity audit](native-ui-parity.md).
This batch does not establish behavior on a physical Windows LAN or change pacing.
