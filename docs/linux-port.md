# Native Linux port

The native client starts in a standalone menu on Wayland, with scenario selection,
local AI lobbies, configuration, ship/equipment selection, results and return to
menu. It uses Allegiance's original IGC flight, weapon, damage, collision and
wingman AI code with installed models, textures, collision hulls and training
maps. The frontend uses Vulkan, SDL input, OpenAL audio, and FFmpeg movies.
Networking is not built or connected.

The [completeness/equivalence audit and work queue](native-equivalence.md) records
source comparisons, completed extractions, verification evidence and remaining gaps.

See [the standalone frontend guide](native-frontend.md) for the session flow,
configuration, installation and exact scenario/effect coverage. The two guided
exercises are native implementations; the original narrated training campaign,
commander/economy systems and full Windows-client parity remain unfinished.

Camera motion and engine audio response now live in shared `src/ClientCore`
headers used by both WinTrek and the native client. This includes afterburner and
collision shake, animated cockpit/chase transitions, velocity-dependent chase
motion, head look, aleph FOV compression, and the original engine gain/pitch curves.
V toggles chase view; keypad directions look around; Page Up/Down adjust chase
distance. See the frontend guide for the remaining camera and audio gaps.

## Build and run

Tested with Clang, CMake, Ninja, SDL 3.4, Vulkan headers/loader 1.4.354,
glslc, GLM, OpenAL Soft, FFmpeg 8, FreeType, and Fontconfig on this Arch machine.
Development headers and pkg-config metadata for these libraries are required.
The presentation integration uses recent Vulkan headers containing
`VK_EXT_present_timing`, `VK_KHR_present_id2`, and `VK_KHR_present_wait2`.

```sh
cmake -S . -B build/linux -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_COMPILER=clang++
cmake --build build/linux -j8
build/linux/src/native/allegiance-native
```

The default is a borderless desktop-sized Wayland window at the display's native
resolution. `--windowed` selects a smaller resizable borderless window.
Esc opens the session menu and releases the mouse; F10 exits.
Use `--practice` to bypass the menus. `cmake --install build/linux --prefix "$HOME/.local"`
installs the executable, shaders and desktop entry; the installed client runs
independently of the build tree.

Data is read, without modification, from:

```text
~/.local/share/Steam/steamapps/common/Allegiance/Artwork
```

Use `--artwork /path/to/Artwork` for another installation. Game data is not
copied into or distributed with this port. A working default OpenAL audio device
and a system sans-serif font are required (Verdana is preferred, with a Fontconfig fallback).

## Practice and controls

Choosing combat practice starts you in the Neptune base hangar with an Enhanced
Fighter. Launch to face an interceptor controlled by the original wingman AI. Destroyed opponents are
replaced. Tab selects ships, the base, and any jump gates; the HUD shows target
distance, hull, a gun lead cue, and an off-screen target direction cue. A green
lead cue indicates gun range. F2 engages return-to-base autopilot; docking opens
the hangar for ship/equipment selection and relaunch. R after destruction
recovers to the hangar. This practice mode gives full contact visibility.

| Control | Action |
| --- | --- |
| Mouse / arrow keys | Turn |
| W / S, mouse wheel | Increase / decrease throttle |
| A / D | Strafe left / right |
| Space / C | Strafe up / down |
| Q / E | Roll |
| Left Shift | Afterburner |
| Left Ctrl | Coast |
| Left / right mouse button, M | Selected guns / missiles |
| 1–4 / 0 | Select a fixed gun / group fire |
| P | Autopilot toward the selected target; steering cancels it |
| K | Toggle cloak, if equipped |
| X / N | Deploy chaff / dispenser (mine or probe) |
| Backtick | Toggle cursor; click contacts or inventory guns |
| Tab | Next target |
| V | Cockpit / chase camera |
| R | Reload from reserves; recover to hangar after destruction |
| B / G / Delete | Cargo menu / next cargo stack / drop selected cargo |
| F2 | Autopilot return to a friendly landing station |
| Enter | Launch when docked |
| F1 | Show flight controls |
| F3 | Play the original introduction movie |
| F4 / F6 / F7 / F8 | Inventory / contacts / sector map / event log |
| Z | Cycle radar detail: Default, Target, None, All |
| H / F9 | Toggle flight HUD / timing diagnostics |
| + / - | Adjust HUD size while running |
| Esc | Open session menu / resume; skip a movie |
| F10 | Quit |

The core docking callback replenishes the ship and Enter launches it through an
original station launch slot. The integration test flies to the friendly base on
autopilot, docks, checks repairs, and launches. The warp callback is wired,
and the multi-sector scenarios include gates.

## Classic flight HUD

The native HUD now uses the default CortUI Classic flight layout as its visual
reference. It reads the original gauge, target, reticle, contact-icon, inventory,
and minimap artwork from the installed data. It does **not** execute `dialog.mdl`
or recreate the Windows Image/Pane/Context tree. Positions and gauge source
rectangles are transcribed into native composition code in `src/native/hud.cpp`.

Implemented for the offline world:

- Hull/shield damage flashes; throttle, fuel, ammunition and energy gauges;
  speed, signature and actual experience bonus; vector lock, cloak, autopilot
  and scanner-detection indicators.
- Ship/station target identification, distance, speed, hull/shield gauges, a live
  3D target view, gun lead/range cues, missile lock/reload feedback, velocity
  direction and peripheral contact markers using the original radar icons.
- Live equipment names/counts/status, individual/group gun selection, actual
  chaff/mine/probe deployment, cursor target selection, and autopilot commands.
- Sector topology/status, a selectable contact list, bounded local event history,
  help, pause, death and docking overlays. Contact visibility remains unrestricted
  in this practice world; the eye indicator separately uses enemy scanner range.

The presentation boundary is a typed `GameState` snapshot plus contacts and
commands. Skins do not access legacy globals. Gauges clip source rectangles
rather than stretch their contents. Anchored logical panels preserve aspect
ratio and keep the bottom panels separated on narrow windows. `--ui-scale 1.25`
changes the preferred HUD size; the layout limits it to fit. FreeType renders
text at the selected device-pixel size, with a bounded, resident R8 coverage atlas
updated on scale changes. Current text coverage is Latin-1; complex-script
shaping and broader localization remain future work.

The UI sends **52 bytes per quad**, including glyphs, instead of six expanded
vertices. Its shader generates the corners. Normal alpha and additive art share
one ordered batch. The 3D target viewport reuses the resident mesh/instance arenas
and clears only its small depth region within the existing rendering pass; it
splits the UI batch into background and foreground. There is no offscreen target
texture, per-widget Vulkan object, or per-frame image upload. Text layout uses
resident glyph metrics, and unchanged fonts/art need no rasterization or uploads.

The classic flight HUD and standalone menus are functional, but full
Windows-client interface parity remains unfinished. Commander/economy/team
management, cargo swapping, purchases, queued orders, teleport/ripcord workflows,
flag/artifact modes and strategic-map commands still need native implementations
and their underlying offline services. The local hangar already supports hull
selection and compatible mounted-equipment replacement. The F6 contact list and
event log are local interfaces. See [HUD parity notes](native-hud.md) and
[frontend coverage](native-frontend.md).

## Renderer and pacing

The native backend uses Vulkan 1.3 dynamic rendering and synchronization2, a
2,048-entry bindless sampled-image array, resident shared vertex/index arenas,
per-frame instance data, indexed multidraw indirect, reverse-Z depth, sphere
frustum culling, mipmaps, and an instanced HUD with ordered alpha/additive composition. Geometry and textures
are cached and uploaded before normal flight. Per-frame GPU buffers are reused;
movie frames reuse an image and mapped upload buffers. There is no per-frame
device-idle wait in normal rendering. The current static arenas are sized for
this practice world, and fail explicitly if exhausted.

Two frame-resource sets and a three-image FIFO swapchain permit resource reuse
without treating the swapchain depth as permission to build a render queue.
Acquisition and pacing waits precede SDL input polling, simulation, and rendering.

`FramePacer.{h,cpp}` adapts the user-supplied controller at 0ad commit
`5d9243f14f581d85e4e4ceb1667f89c6a53257d7` (namespace/build integration changed;
original attribution retained). `presentation.cpp` connects it to actual Vulkan
presentation feedback, calibrated to `CLOCK_MONOTONIC`, with queue-completion and
display timestamps, absolute presentation targets, bounded present waits, and
adaptive frame budgets. It falls back to present-wait or FIFO acquisition when
the timing extensions are unavailable; the low-latency behavior verified here
depends on this machine's presentation support.

The HUD's millisecond value is **CPU input-sampling-point to reported display
presentation**, not physical mouse-event-to-photon latency. It excludes device
polling and cannot measure panel response. `--timing-csv` records both CPU and
presentation timestamps so latency consistency can be assessed rather than
inferred from frame rate or GPU utilization.

```sh
build/linux/src/native/allegiance-native --smoke-test --frames 10000 \
  --timing-csv /tmp/allegiance-timing.csv
python3 src/native/analyze_timing.py /tmp/allegiance-timing.csv --skip-frames 1800
```

The smoke sequence changes ships, selects weapons, toggles HUD panels and help,
fires guns/missiles, respawns, plays/skips a movie, and changes UI scale twice
(exercising glyph atlas uploads). It also requests two window sizes with `--windowed`; this compositor currently keeps the window at the
display size, so those requests do not verify swapchain resizing. Longer runs continue
combat and respawn automatically. Automated runs do not capture the mouse.
Timing traces reset frame IDs at swapchain recreation and identify each segment.

Before the classic HUD replacement, a 10,000-frame run on the Radeon 8060S/RADV display at 2560x1440 and 165 Hz,
with validation disabled and the first 1,800 frames excluded, reported:

| Measurement | Median | p95 | p99 |
| --- | ---: | ---: | ---: |
| Sampling point to presentation | 1.588 ms | 4.427 ms | 4.437 ms |
| Sampling point to present submission | 0.029 ms | 0.076 ms | 0.123 ms |
| Sampling point to queue completion | 0.246 ms | 0.303 ms | 0.376 ms |
| Presentation interval | 6.061 ms | 6.063 ms | 6.063 ms |

Mean presentation rate was 164.959 Hz, with two new pacer misses in the measured
8,198 presentations. Two transient stalls increased the budget, which then
converged down: the final 1,000-frame window had 1.302 ms median and 1.361 ms p95
sample-to-present latency. These are measurements of this practice scene and
compositor, not a guarantee for every workload or system.

With the classic HUD and target viewport enabled, another 10,000-frame run
(excluding the first 1,800 frames) measured:

| Measurement | Median | p95 | p99 |
| --- | ---: | ---: | ---: |
| Sampling point to presentation | 1.588 ms | 4.025 ms | 4.028 ms |
| Sampling point to present submission | 0.031 ms | 0.084 ms | 0.143 ms |
| Sampling point to queue completion | 0.287 ms | 0.347 ms | 0.418 ms |
| Presentation interval | 6.061 ms | 6.063 ms | 6.064 ms |

Mean presentation rate was 164.959 Hz, with two new pacer misses in 8,198 measured
presentations. The final 1,000 frames settled to 1.394 ms median / 1.438 ms p95
sample-to-present. This run included the attachment-name fix and final help
layout, but preceded converting the font atlas from RGBA to R8 coverage. Its
timing numbers are a workload sample, not a regression bound.

With the native frontend and textured effects added, a further 10,000-frame
combat run (again excluding the first 1,800 frames) measured 1.653 ms median /
4.054 ms p95 / 4.058 ms p99 sample-to-present, 0.039 ms median sample-to-submit,
and 0.314 ms median sample-to-queue-completion. Mean presentation rate remained
164.959 Hz, with two new pacer misses in 8,198 measured presentations. These are
compositor presentation measurements, not input-to-photon measurements.

External cameras also include target, flyby, orbit, station tracking and missile
views. Alt+V cycles the views; V returns to cockpit or chase. Hold Alt with the
arrow/roll keys to rotate an external view, and use PgUp/PgDn for distance.

## Audio and movies

OpenAL provides positional effects, listener orientation/velocity, distance
attenuation, Doppler, engine loops, and gun/missile/damage/explosion sounds.
FFmpeg decodes the installed Ogg/WAV effects while preserving authored sample
positions. The original platform-independent sound-template engine handles gain,
pitch, priority, cones, paired and weighted-random sounds, intermittent ambience,
serialized speech, repeated fire, and attack/sustain/release loops. A native
sound-only MDL reader constructs those templates. The OpenAL backend retains
virtual voices and assigns up to 128 physical sources by audibility/priority.
Core sounds are decoded, mixed and uploaded before flight, including future
research/building equipment. Moving emitters retain their model-local offsets;
ship engine, weapon, mining, base, aleph and probe loops follow original policies.

The installed catalog has 1,025 usable definitions referencing 1,004 wave files.
Twenty additional optional-core definitions reference ten missing `faohpsi*`,
`faohtheta*`, `faohmu*`, and `faohaspg` files in the Steam artwork. Missing assets
are reported once rather than silently treated as equivalent playback. Original
training/quick-chat integration and full event coverage remain in the audit queue.

FMV decoding runs on a worker with bounded queues. OpenAL streams stereo audio;
the audio sample clock drives frame selection, and Vulkan uploads only changed
video frames. Playback returns to the calling menu or flight at EOF and can be skipped with Esc or Space.

```sh
build/linux/src/native/allegiance-native --movie intro_microsoft.avi
```

## Validation

Headless integration tests use the installed data and register when CMake finds
`PCore014.igc`. Override its test path with `-DALLEGIANCE_ARTWORK=/path/to/Artwork`.

```sh
ctest --test-dir build/linux --output-on-failure
build/linux/src/native/allegiance-native --windowed --validation --smoke-test
```

The tests load both static cores and all six supported hulls/missiles, verify
control direction, weapon selection versus actual ammunition consumption, autopilot
and manual takeover, missile/chaff/mine launches, station target/preview state,
radar icon assets, and autopilot docking/launch. They run combat through player
destruction, then respawn and switch loadouts. Vulkan validation includes
synchronization validation. The new session test also checks all four maps, lobby
teams, configuration/bindings, equipment changes, exercise completion and a
skirmish win. A validation layer installation is required for `--validation`.
The camera test also checks actual IGC collision/afterburner events, view changes,
pause behavior, and chase integration across refresh rates. Use
`--camera-smoke-test --validation` for the corresponding native display exercise.

AddressSanitizer can be enabled through normal CMake compiler/linker flags. The
port fixes existing core defects exposed during testing: mismatched collision-hull
allocation/deletion, deletion through a nonvirtual hit-test base destructor, an
array-delete mismatch, and unbalanced territory references at shutdown. Native CVH
loading also trims CRLF from attachment names; otherwise docking planes and
weapon/launch hardpoints silently fail to resolve on Linux.
Shared portability changes retain the Windows build path; a Windows build has
not been tested in this Linux environment.

The simulation passes AddressSanitizer with leak detection. The complete client
smoke test has no reported address errors or Vulkan synchronization errors, but
LeakSanitizer reports two 256-byte allocations in Mesa's Radeon/lavapipe driver
initialization at shutdown (identified against the loader's library maps).
Those external allocations are not suppressed and make the full-client leak
check exit nonzero. Fontconfig's cache is explicitly released after atlas creation.

## Remaining work

- Original narrated campaign scripting, commander/economy UI, remaining radar/order
  states, cargo and saved loadout management, and gamepad interfaces.
- Full MDL/effect coverage: the reader handles the geometry/image subset used by
  these worlds; it chooses the highest available LOD. Textured particles, exhaust,
  trails and animated explosions now render natively. Shield-lens geometry,
  oriented shockwaves, model lights and construction/mining/teleport effects
  still lack complete legacy visual parity.
- Complete sound-template semantics, ambient music, and broader warp/gameplay coverage.
- Networking and Steam services, intentionally deferred. No DirectPlay shim is
  part of this client.
- Broader GPU/compositor testing, distribution packages, and macOS support. This backend is
  currently Linux/Wayland-specific; macOS would need surface/platform integration
  and a review of MoltenVK feature and presentation-timing support.

Vulkan references: [present timing](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_present_timing.html),
[timing design](https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_present_timing.html).
