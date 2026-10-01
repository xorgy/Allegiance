# macOS native client

The Apple Silicon port targets macOS 26.0 and later. Linux builds use Clang,
ld64.lld and an Xcode macOS SDK copied from the authorized Mac. SDL3 provides
Cocoa windowing/input, MoltenVK provides Vulkan over Metal, CoreText locates
system fonts, and OpenAL Soft supplies the original sound engine's output.
The renderer retains its bindless texture table, instanced indirect draws,
dynamic rendering and presentation-time interpolation. MoltenVK on the M1
provides GOOGLE display timing and present-wait2. Actual drawable timestamps,
calibrated from the mach clock into the application's monotonic clock, drive
frame pacing and the predicted display deadline used for network interpolation.

## Cross build

The Linux host needs LLVM (including ld64.lld and llvm-install-name-tool),
CMake, Ninja, Python 3, pkg-config, glslc, rsync, curl and make. The Mac needs
Xcode with a macOS 26 or newer SDK and Homebrew. The preparation step installs
missing libraries there, records their versions and stages only the headers,
package metadata and runtime dependency closure on Linux.

```sh
export MACOS_HOST=user@mac-host
./tools/build-macos.sh
./tools/deploy-macos.sh
```

Set `MACOS_HOST` to your signing/build Mac's SSH destination when preparing or
refreshing dependencies, and when deploying. The prepared SDK/dependency snapshot
is reused on repeat builds without a Mac connection. After intentionally
upgrading Xcode/Homebrew, run
`./tools/build-macos.sh --refresh-deps`. `ALLEGIANCE_MACOS_CACHE`,
`ALLEGIANCE_MACOS_BUILD` and `ALLEGIANCE_MACOS_PACKAGE` override the build paths;
`GNS_SOURCE` selects a GameNetworkingSockets checkout. The script otherwise
uses the adjacent checkout or CMake's pinned fallback.

The default package contains only the client, server and directory executables.
Use `--include-checks` to build/package the development diagnostics as well.

The output is `build/macos-package/Allegiance.app`. Its loader, MoltenVK driver,
OpenAL and other non-system libraries are bundled with relative install names;
a runtime Homebrew or Vulkan SDK installation is unnecessary. Shaders, original
license notices and the MoltenVK driver manifest are in the app's resources.
Game data remains external in the Steam library.

The Linux protoc executable matches the staged Mac protobuf runtime. Host
pkg-config searches only the staged Mac packages and SDK zlib metadata, so
Linux headers and libraries cannot accidentally enter the cross build.
Dead stripping both removes unused dependencies and avoids ld64.lld's duplicate
zero compact-unwind entries; the malformed-MDL exception check exercises this.

## Run

The deployment script installs the app at `~/Applications/Allegiance.app` on the Mac:

```sh
open "$HOME/Applications/Allegiance.app"
"$HOME/Applications/Allegiance.app/Contents/MacOS/allegiance-native" --windowed
"$HOME/Applications/Allegiance.app/Contents/MacOS/allegiance-native" --connect SERVER:27500
```

The default artwork path is
`~/Library/Application Support/Steam/steamapps/common/Allegiance/Artwork`.
The complete Steam game directory was copied there without modifying its data.
Settings are stored in `~/Library/Application Support/FreeAllegiance/native.conf`.
The original interface is the default. Integer scaling follows the display's
request, with 150% rounding down to 1×; `--classic-ui-scale auto` restores this
if an earlier configuration explicitly selected 1× or another fixed scale.

Apple Silicon requires local ad hoc code signatures after modifying Mach-O
install names. Development deployment applies these on the Mac. They are not
release signing; Developer ID signing/notarization remains the publishing step.

## Finalize a release on the Mac

The distribution package excludes Steam data. Its app's `build-info.json`
records the same source/network identity as the Windows and Linux builds.
Place `tools/finalize-macos-release.sh` beside `Allegiance.app` on the signing
Mac, then run locally:

```sh
./finalize-macos-release.sh --check
./finalize-macos-release.sh --setup-notary  # Once, unless the selected Keychain profile already exists
./finalize-macos-release.sh
```

The script discovers the sole valid Developer ID Application identity, or uses
`CODE_SIGN_IDENTITY` from `signing.conf`/the environment. `DEVELOPER_TEAM_ID`
and `NOTARY_PROFILE` can be set there too; the default profile is
`freeallegiance-notary`. The setup command prompts through `notarytool` and
stores credentials in Keychain. No password or private key belongs in the
configuration or package.

Finalization preserves the source app, signs a copy's nested code before its
containing bundle with hardened runtime and secure timestamps, and submits it
with `notarytool`. All bundled libraries use the same Developer ID team;
library validation remains enabled. After Apple accepts the submission, the
script staples and validates the ticket, verifies signatures and Gatekeeper
acceptance, then creates the final app ZIP and SHA-256 sidecar in `dist/`.
Submission/signature logs and the signed working app are retained there. A
failed submission does not produce a new release ZIP. This follows Apple's
[notarization workflow](https://developer.apple.com/documentation/security/customizing-the-notarization-workflow)
and [nested-code signing guidance](https://developer.apple.com/library/archive/technotes/tn2206/).

## Native media

ImageIO/CoreGraphics decode the original PNG/JPEG artwork. AVFoundation decodes
and resamples the Ogg Vorbis sounds into the sound engine's 48 kHz PCM buffers;
AudioToolbox supplies their original granule durations for authored loop bounds.
A direct framework probe on macOS 26.6.2 decoded the original Ogg sound but
AVFoundation rejected both original AVI movies. They contain MPEG-4 video
(tag FMP4) with WMA v2 audio. The FMV fallback therefore uses a narrowly built
LGPL FFmpeg: AVI demuxing, MPEG-4/WMA v2 decoding, file access and pixel/audio
conversion only. It retains the original movie files, bounded background
decoding and OpenAL sample-clock synchronization.

MoltenVK requires refreshing stage-specific argument-buffer bindings after
changing between the HUD and instanced world pipelines. This refresh is
compiled only for macOS; Linux/Windows keep their existing descriptor-bind cost.
Camera push constants target only the vertex stage where they are consumed.

## Frame pacing

The controller separates one WindowServer composition interval from CPU/GPU
work and the adaptive latch margin. This permits one frame per refresh with
bounded overlapping frames even when composition requires submitting before
the previous frame is displayed. A reusable strict one-shot dispatch timer
provides scheduled wakes without busy waiting. In an isolated 10 ms timer probe,
`mach_wait_until` woke about 5 ms late even at user-interactive QoS; the strict
timer reduced median lateness to 0.05 ms and p99 to 0.13 ms. The timer's event
handler only signals a semaphore; rendering remains on the original thread.
Cancellation waits for the handler to finish before releasing its context.
GOOGLE refresh information is checked periodically, and swapchain recreation
discards old presentation identities and targets.

The Mac path submits ready drawables immediately: timed `presentAtTime` requests
added another refresh in the measured WindowServer path. Actual presentation
feedback controls the CPU start time instead. MoltenVK's present-wait2 completion
can precede display and is used only to limit outstanding work here, never to
seed display timestamps. The synthetic presentation regression explicitly
exercises that distinction and the native clock calibration. The Linux EXT
presentation timing path continues to supply its more detailed stage reports.

On macOS, the acquired swapchain view's Metal texture is resolved before input
sampling, so MoltenVK's lazy drawable acquisition cannot insert an unmeasured
wait inside command encoding. Two timestamp queries, one per existing frame
slot, measure rendering completion. Device-counter calibration is bracketed by
the application's clock every half second; query results are polled without
waiting. Reliable counters replace the blind quarter-refresh GPU allowance
with measured work and an adaptive residual margin. Calibration/query failures
restore the conservative allowance. These queries and Metal operations are
compiled only for macOS; Linux and Windows keep their existing rendering path.

Spatial audio updates, game sound events and mixer maintenance now run after
GPU submission, using the same frame's game state. They overlap rendering
instead of extending the CPU budget before presentation. Training completion
flushes that last world-sound update before replacing it with debrief narration
or retiring the game. The training transition smoke check follows actual
completion events and clicks the original training controls, so its assertions
work at both 60 Hz and 165 Hz rather than depending on a particular frame count.

`--timing-csv FILE` records frame starts, submissions, actual displayed times,
predicted display deadlines, scheduled wake times and misses. The CPU-to-drawable
interval is useful for pacing validation; it does not measure hardware input latency or physical
scanout of an individual screen pixel. GOOGLE's margin fields are estimates,
not rendering-completion counters, and are deliberately not substituted for
the measured GPU timestamps. See the
[MoltenVK drawable implementation](https://github.com/KhronosGroup/MoltenVK/blob/v1.4.2/MoltenVK/MoltenVK/GPUObjects/MVKImage.mm).

On the M1/1920×1080/60 Hz test machine, paired 1280×720 windowed runs with the
same release build and scenarios measured the following CPU-start-to-drawable intervals. The
comparison changes only the Mac frame wait; statistics use the final 400
reported frames of each run.

| Scene | Normal kernel wait median / p99 | Strict timer median / p99 |
|---|---:|---:|
| Original main menu | 22.67 / 28.81 ms | 18.93 / 21.46 ms |
| 128 versus 128 mixed-ship command view | 27.35 / 30.09 ms | 25.96 / 26.04 ms |

Scheduled-wake p99 fell from 7.71 to 0.12 ms in the menu and from 5.03 to
0.12 ms in combat. The 600-frame menu run missed no refreshes; the 1200-frame
combat run had two misses as work exceeded its recent budget, with none after
frame 314. Both settled at one frame per 16.667 ms refresh. The retained Linux
EXT timing path ran 900 frames at 165 Hz with no misses and a 3.86 ms p99
CPU-start-to-presentation interval (3.85 ms before the Mac work).

The default 1920×1080 borderless fullscreen mixed-ship run completed 900 frames
with no misses: median 27.20 ms, p99 27.81 ms, and wake p99 0.10 ms. Metal's
validation layer passed the original bitmap-font, alpha-cutout, starfield and
integer-scale pixel probes and the 1600-frame frontend transition check. The
matching Mac client joined a Linux server, launched and flew for 900 frames:
198 snapshots, 16.7 ms RTT, 3.5 cm maximum correction, no missing baselines, and
settled median/p99 sample-to-present of 20.00/20.04 ms.

The subsequent audio scheduling change reduced the submission path's median
CPU cost on the M1 from 7.27 to 6.49 ms in native borderless mode. Paired
128-versus-128 mixed-ship command-view runs (seed 47, 900 frames, statistics over
the final 400 reported frames) measured these CPU-start-to-presentation intervals:

| Platform and window | Before median / p99 | After median / p99 |
|---|---:|---:|
| M1, 1920×1080 borderless, 60 Hz | 26.71 / 27.90 ms | 26.03 / 26.74 ms |
| Linux Wayland, 1280×720 windowed, 165 Hz | 3.66 / 3.76 ms | 3.35 / 3.42 ms |

The final Mac and Linux runs each missed zero refreshes. The Mac's settled
prediction error had a 0.002 ms p99 and scheduled-wake lateness a 0.142 ms p99.
The Linux renderer and presentation controller are unchanged; only audio's
place in the native client's frame loop changed. The final original-training
death/debrief/next-lesson cycle passed on macOS, Linux and the Windows build
under Wine; the original frontend transition check and audio regression also
passed. The three cross-platform builds retain matching source identities.

An isolated Metal display-link probe and an unsynchronized drawable experiment
were also measured. Neither was retained: the former increased latency on this
Mac, and the latter produced irregular presentation timestamps. Synchronized
drawable feedback, present-time interpolation and the separate composition
interval remain in use.

A further paired submission test measured 25.93 ms median latency with either
synchronous or asynchronous MoltenVK submission. Asynchronous submission moved
about 0.65 ms of encoding from the CPU interval into the GPU-ready interval,
without shortening the path to display, so the simpler synchronous path remains.
An isolated build reserving half a refresh for composition missed 35 refreshes
in 1200 combat frames; its final 400 frames had 29.48 ms median and 41.33 ms p99
latency. That experiment was reverted. These measurements support retaining
the composition interval while reducing the actual work before submission.

The subsequent original-lobby build retained the same pacing code. Its final
900-frame release recheck measured 25.56 / 26.96 ms median/p99 on the M1 in
the same borderless scenario, with one missed refresh at frame 635 when the
submission path rose to 8.28 ms. The paired Linux Wayland recheck measured
3.55 / 3.63 ms at 165 Hz with no misses. These statistics again use the final
400 reported frames; they measure CPU sampling to drawable presentation,
not physical input or per-pixel scanout latency. Metal validation separately
passed the 900-frame combat run and original popup/setup pixel checks.

Apple Silicon now uses NEON for the native AI avoidance index's swept bounds
and packs frustum planes across SIMD lanes for model/particle rejection.
These reduce actual work between input sampling and submission without moving
input earlier or shortening the composition allowance. The original avoidance
policy, candidate order, float operation order and scene contents are retained.
The x86 renderer's existing scalar plane tests and SSE avoidance path remain
unchanged, as does the Linux presentation controller.

A final paired release comparison used the same M1, 1920×1080 borderless mode,
128-versus-128 mixed ships, seed 47 and 900 frames. Statistics discard the first
500 frame identities:

| Measurement | Before ARM SIMD | After ARM SIMD |
|---|---:|---:|
| CPU start to submission, median / p99 | 6.46 / 7.02 ms | 6.07 / 6.55 ms |
| CPU start to drawable presentation, median / p99 | 26.42 / 27.60 ms | 25.32 / 26.42 ms |

Both final runs missed zero refreshes. Earlier paired comparisons showed
smaller median display-latency gains (0.14–0.25 ms); the adaptive budget includes
recent expensive frames, so the CPU reduction does not always translate into
the same display-latency reduction. A longer 1200-frame pair encountered one
late frame in each settled interval during heavier combat. All 900 frames of
the final pair retained identical camera poses, scene counts, particle counts,
draw counts and submitted triangle counts.

The ARM/scalar frustum regression passed 266,752 comparisons, including exact
tangencies and large coordinates. On both macOS and Linux, the avoidance-policy
regression passed 142,080 original/indexed Dodge comparisons, 9,600 path-control
comparisons and snapshot-mutation checks. Native Metal pixel validation passed;
the Windows cross build and frustum check under Wine also passed. These are
CPU-start-to-drawable measurements with actual presentation feedback, rather
than physical input-to-photon measurements. The remaining full-refresh
WindowServer composition interval is still accounted for.

The final Linux Wayland run retained 165 Hz with zero misses, 3.71 / 3.78 ms
median/p99 CPU-start-to-presentation and 1.01 / 1.15 ms CPU-start-to-submission.
The earlier Linux reference measured 1.00 / 1.16 ms to submission. Display lead
varies with the recent maximum covered by the adaptive budget; the x86 culling
code and the Linux pacing policy were deliberately left unchanged.

The original UI restoration has the same remaining work on all three platforms;
see [the parity queue](native-ui-parity.md).
The restored `newgamescreen` passed the shared session/choice/host-authority
checks and native Metal pixel checks, including its unscaled 800×600 background,
popup palette and preservation of parent control faces. The preview lifetime
checks cover all 140 valid generated map/team/size combinations. The development
Mac does not have the Khronos Vulkan validation layer installed; these pixel
checks used `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1` with Vulkan validation
disabled, while Linux used its installed Khronos layer.
The restored admission controls also pass the shared session checks on macOS,
Linux and the Windows build under Wine, including separate Away/force-ready,
commander-only control visibility, pending applicants and No Team navigation.

The current installed development bundle also passes the shared original
teleport/investment/modal, script and keyboard-map checks. Metal debug/shader
validation passes the Vulkan pixels for the restored consoles and construction
markers. A 700-frame native client run verifies F5/F8 toggles, launch beneath
investments, continuous keyboard steering with its fresh pane open, Toggle Mouse
release and input return while the exit animation runs. Further Mac pacing work
is deferred; these bounded UI checks are not pacing benchmarks.

The installed bundle now also passes the restored F6/mini-team player-pane
policy and actual SDL interaction checks: original artwork and column sizes,
collapse/expand, roster/context picking, private chat, hangar/flight overlays,
launch-camera suppression and held-key flight steering. The 1,010-frame native
run used Metal debug/shader validation. Original console, script, keymap,
session and Vulkan pixel checks pass, as do encrypted group/wing/waypoint routes
and the commander Stop Donation toggle. Remaining interface discrepancies are
recorded in [the UI parity queue](native-ui-parity.md).

The installed player-history/turret follow-up also passes closed-pane sighting
and replica-retirement, authoritative dock time across hull reloads, public
departure/ID reuse and observed drone-death checks. Original turret shortcuts
and canonical crew identity pass offline and encrypted boarding, firing,
promotion, death/recovery tests. Console, sector, health and throttle network
regressions pass. Metal debug/shader pixel checks and a further 1,010-frame
actual SDL player-pane run pass with the existing pacing policy unchanged.

The original LAN entry restoration builds both macOS client and server. Its
headless discovery executable passes natively on the test Mac, including two
authorities sharing the discovery port and teardown. macOS loopback broadcast
does not fan out to those sockets; host-local multicast with TTL zero supplies
that fanout without leaving the machine. The wider UI/metadata/expiry checks are
recorded in [the UI parity audit](native-ui-parity.md). This test is run from
`/tmp`; a new application bundle is not deployed by this batch. Pacing is unchanged.
