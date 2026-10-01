# Native fleet scaling

For human-roster server capacity, shared visibility arenas and the encrypted
four-faction 1,024-connection evaluations, see
[Native human-roster network scaling](native-network-scale.md).

The practice/skirmish frontend accepts 1,024 enemies and 1,024 allied wingmen,
and larger counts within IGC's ship identity range. Click either numeric value,
type a replacement and press Enter. The former 8-enemy/7-wingman and 32-local-seat
limits are gone. Validation counts the pilot, local seats and initial AI together.
Native allocation reserves and recycles IDs without signed overflow or reuse of
a live/staged identity. The remaining representation limit is 32,768 simultaneous
nonnegative ship IDs across the mission. Conquest's original miner/drone production
rules remain mission rules; this change does not reinterpret their byte fields.

For ordinary play, after installing the build:

```sh
/tmp/freeallegiance-install/bin/allegiance-native --practice --enemies 1024 --allies 1024
```

For an interactive battle observed from command view:

```sh
/tmp/freeallegiance-install/bin/allegiance-native --scale-test 1024 --scale-command --seed 1
```

This keeps the pilot docked, with no player ship or player target in space to
influence the AI encounter. The initial opposing discs use 25% tighter spacing
than the earlier runs: at least 72 m between ships and a 2,016 m disc radius for
1,024 ships per side. Both sides cycle equally through interceptor, enhanced
fighter, advanced stealth fighter and scout hulls, with their original preferred
equipment. Replacement
AI ships continue spawning while docked, on the two opposing planes instead of
relative to the player's ship. Original wingman decision-making remains in use;
command view retains the team's normal sensor visibility and ability to issue
orders. Leave units unselected to watch their autonomous behavior.

Control+period toggles both the command grid and yellow height lines. The saved
command preference is separate from the pilot grid preference and defaults on.

The command camera starts 3 km from the encounter center. Pan with the arrow keys
or screen edges, zoom with the mouse wheel, PageUp/PageDown or +/-, and use
Shift+F2 to toggle top-down view. `--scale-distance M` changes the initial camera
distance. Omit `--frames` for interactive viewing without a time limit. Ordinary
scale runs still default to the physical observer 1.5 km from the encounter.

## Changes to simulation and presentation

The original IGC wingman decisions, weapons, collision solver and visibility
policy remain in use. Native broad phases reduce the work before those policies:

| Path | Scaling change | Ordering and policy retained |
| --- | --- | --- |
| Drone avoidance | Conservative spatial trees over position, velocity and radius; separate cached snapshots for players and each side | Original `Ignore` and impact calculations; original linked-list order and first-object ties |
| Friendly drone precedence | Nodes and leaves prune lower-ID drones that original policy ignores | Mining and ripcord exceptions remain; state, identity and allegiance changes invalidate snapshots |
| Path diversion | Conservative bounds along the original goal segment | Original stationary-obstacle eligibility, diversion calculations and controls |
| Scanner searches | Spatial bounds over scanner range, position and radius | Original `InScannerRange`, cone checks and first spotter |
| Collision traversal | Complete swept bounds reject nonoverlapping original KD subtrees and leaf pairs | Original pivots, traversal, pair direction, precise solver, collision times and queue ordering |
| Model removal | Cached nodes unlink models from cluster lists in constant time | Original lists, reference ownership and traversal order |
| Presentation objects | Dense arenas; a separate list for actual models; scanner history, damage and remembered intel allocated when needed | No artificial limit on scene models or contacts |
| World audio | Silent projectiles skip unnecessary source work; logical world voices grow separately from the 128 physical OpenAL sources | Original sound graphs, audibility, priority, cones and virtual playback progression |

Spatial snapshots are scoped to the original AI/visibility phases, with mutation
invalidation. Interleaved ships from different teams reuse their side's snapshot
instead of rebuilding one tree on each team switch. The small-fleet fallback uses
the original scans. These IGC/Utility additions are guarded by
`ALLEGIANCE_NATIVE`; WinTrek does not construct these indices or perform their work.
A Windows build/runtime performance test has not been run here.

Opaque Vulkan instances group by shared geometry, with bindless materials per
instance. Different skins of the same mesh therefore share a draw. Grouping uses
linear histogram/scatter passes rather than a per-frame sort. Alpha geometry
keeps depth order, and the common additive-effects path avoids sorting and
per-particle batch construction. Conservative sphere bounds cull offscreen
billboards and stretched projectile sprites; lines, ribbons and oriented planes
retain their existing geometry paths.

Model upload also resolves the original `Geo::FindTexture` inheritance. All seven
asteroid assets have a bare middle LOD (projected radius greater than 8 through
32 pixels); WinTrek seeds the texture from the base geometry before drawing it.
The native loader now carries that default through groups, transforms and LODs,
and upload uses it for bare meshes. This fixes the white asteroids without adding
per-frame material searches, duplicating geometry or changing the Windows path.

Sector posters and inventory/map panel artwork also honor the original black
color-key hints. Opaque black cutouts become zero alpha once when loaded, before
filtering and mip generation; authored nonblack colors and coverage stay intact.
Vulkan pixel checks exercise the actual planet background and HUD panel textures
over a bright scene, retaining the visible disk, borders and lettering while
revealing the background through their cutouts. No extra per-frame pass is used.

Per-frame instance, indirect, particle and HUD buffers grow geometrically after
their reuse fence. Descriptor changes affect only the frame whose fence was
waited. The old draw/particle cutoffs no longer silently drop work. GPU storage
range limits still fail explicitly. The particle live pool scales with the fleet
and quality setting; near-camera exhaust retains the original 60 Hz presentation
cadence, while subpixel exhaust/damage effects reduce emission density. Original
trajectories, appearance curves and explosion ordering/fill policy remain.

Radar marker and label occupancy is based on screen space. Overlapping ordinary
markers share space, while selected targets, orders, threats, objectives and
command selections retain priority. All contacts and commandable ships still
exist in the simulation. Audio similarly retains all persistent logical voices;
512 transient one-shots have a separate budget and retire in constant time,
without stealing persistent fleet loops. Sound handles include generation and
slot identity so stale handles cannot stop recycled voices.

## Reproduction

The optimized build used on this Ryzen AI MAX+ 395/Radeon 8060S machine is:

```sh
cmake -S . -B build/linux-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
  -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld \
  '-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -march=native -ffp-contract=off'
cmake --build build/linux-release --clean-first -j8
ctest --test-dir build/linux-release --output-on-failure
cmake --install build/linux-release --prefix /tmp/freeallegiance-install
```

`-march=native` produces a machine-specific binary. Floating-point contraction
is disabled deliberately: enabling FMA changed simulation results from the
first tick. This build does not enable fast-math.

The native client and server place their critical threads automatically on
Linux. CPU selection uses the inherited allowed mask, actual SMT/cache topology
and the platform's preferred-core ranking. Simulation, publication and rendering
use distinct physical cores; on this machine simulation also has a separate
last-level cache. Simulation avoids the most preferred desktop cores when the
cache domain has enough comparably ranked alternatives; rendering retains the
highest-ranked core in its domain. Startup, transport and movie worker creation temporarily use
a background mask rather than inheriting the calling thread's critical CPU.
No `taskset` command is required. An explicit taskset/cpuset restriction remains
authoritative; insufficient topology information or fewer than four allowed
physical cores disables automatic placement. The affinity check covers sparse
and restricted masks, multiple packages, SMT, ranking and mask restoration.

The 512-per-team authority measurements on 2026-10-02 additionally use a Clang
IR profile, trained on sustained combat, networking, policy comparisons and
native visual paths. The profile is a local build artifact; ordinary builds do
not require it. Reproduce it with the same compiler version and current source:

```sh
native_profile_dir="$PWD/build/native-profile"
native_profile_data="$PWD/build/native.profdata"
cmake -S . -B build/linux-profile -G Ninja \
  -DCMAKE_PREFIX_PATH=/tmp/allegiance-gns-install \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
  "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -fprofile-generate=$native_profile_dir" \
  "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -march=native -ffp-contract=off -fprofile-generate=$native_profile_dir"
cmake --build build/linux-profile --target allegiance-scale-benchmark \
  allegiance-network-check allegiance-scale-check allegiance-native -j8
LLVM_PROFILE_FILE="$native_profile_dir/combat-%p.profraw" \
  build/linux-profile/src/native/allegiance-scale-benchmark \
  --authority --publication --ships 512 --ticks 10800 --warmup 0 --observer-distance 750
LLVM_PROFILE_FILE="$native_profile_dir/network-%p.profraw" \
  build/linux-profile/src/native/allegiance-network-check
LLVM_PROFILE_FILE="$native_profile_dir/policy-%p.profraw" \
  build/linux-profile/src/native/allegiance-scale-check
LLVM_PROFILE_FILE="$native_profile_dir/visual-%p.profraw" \
  build/linux-profile/src/native/allegiance-native \
  --smoke-test --frames 3600 --config /tmp/allegiance-profile-training.json
llvm-profdata merge -o "$native_profile_data" "$native_profile_dir"/*.profraw
cmake -S . -B build/linux-release \
  "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -march=native -ffp-contract=off -fprofile-use=$native_profile_data"
cmake --build build/linux-release -j8
ctest --test-dir build/linux-release --output-on-failure
build/linux-release/src/native/allegiance-network-check --impaired
```

The dependency prefix above is the existing GameNetworkingSockets installation
on this machine. Remove it when using a system package or the normal pinned
FetchContent build. Keep the merged profile when rebuilding this release tree;
to build without it, restore the unprofiled `CMAKE_CXX_FLAGS_RELEASE` above.
Replacing the merged profile at the same path requires a clean rebuild: compiler
dependency tracking does not notice changes to the profile's contents.
The [authority audit](native-equivalence.md#512-per-team-authority-performance-and-effect-lifetime--2026-10-02)
records the live measurements and CPU placement.

Headless simulation and scene preparation, using the installed original core
and artwork:

```sh
build/linux-release/src/native/allegiance-scale-benchmark \
  --ships 1024 --ticks 10000 --warmup 100 --observe --presentation \
  --csv /tmp/allegiance-scale-dense.csv
```

Add `--spread` for wider starting formations or `--idle` to disable the fleet AI.
The CSV separates simulation, camera, scene, particle and world-sound preparation,
and records live objects, ship-state fingerprints and spatial snapshot rebuilds.
The diagnostic asserts that all 2,049 initial ships were created.

The actual Wayland renderer and presentation feedback can be measured with:

```sh
build/linux-release/src/native/allegiance-native \
  --scale-test 1024 --frames 6000 --validation \
  --config /tmp/allegiance-scale-profile.cfg \
  --scale-csv /tmp/allegiance-scale-render.csv \
  --timing-csv /tmp/allegiance-scale-present.csv \
  --screenshot /tmp/allegiance-scale.ppm
```

Add `--scale-spread` or `--scale-idle` for the other arrangements. These are
diagnostics: they use a stationary healed observer, paired attack orders,
unlimited kills and a fixed 1/165-second simulation step. They are not the normal
play mode. Long runs include projectile traffic, destruction and practice AI
replacements. Simulation time in a diagnostic advances more slowly than wall
time when a frame exceeds the display period.

`--scale-distance M` moves the observer to M meters from the encounter center,
retaining the viewing angle; the default is now 1,500 m. Use
`--scale-distance 9899.495` for the original far view. Initial teams use independent
disc rejection sampling with at least 72 m separation in the dense setup
(256 m with `--scale-spread`), on the original opposing planes. Placement runs
once during setup. The headless benchmark accepts the same observer setting as
`--observer-distance M`. The client CSV also records actual eye position and
field of view.

The headless and rendered scale diagnostics now use the same four-hull mix.
Historical measurements below used interceptor-only fleets and the earlier
96 m dense spacing; those timings are not directly comparable to the mixed,
tighter encounters. Ordinary practice/skirmish sessions retain their existing
ship selection.

`step_ms`, `prepare_ms` and `render_ms` are CPU costs; rendering includes command
recording, submission and presentation calls. Screenshot readback makes the final
frame unsuitable for timing. `ready_ns - submitted_ns` measures queue completion
feedback, not isolated shader execution. Actual displayed frame intervals come
from `displayed_ns`. The printed refresh rate is the display's refresh rate and
must not be reported as achieved FPS. Run benchmarks separately from builds,
tests and other benchmarks.

## Correctness gates

`native-scale-policy` compares accelerated and frozen-original behavior on the
same live IGC objects: 96,000 Dodge decisions, 9,600 path controls, 19,200 original
first-spotter queries and 9,827 ordered collisions. It exercises dense/moving
ships and projectiles, alliances, equal-time overlaps, mutations, removal,
mining/ripcord exceptions and rekeying. Collision comparisons check pairs,
times and shapes before queue sorting and after it. ID tests reserve the entire
32,768-ID range, reject exhaustion and safely reuse a retired ID.

The explicit Vulkan pixel checker verifies 2,048 mixed-skin instances in one
shared-geometry draw, 20,000 instances plus 20,000 particles through arena growth
and both frame descriptors, offscreen sprite rejection, visible rotated/stretched
sprites at frustum edges, transparency, crowded radar priority, keyed brackets
and perspective stars. Original asteroid pixels at 80, 48, 16 and 4 pixel radii
must match the same meshes with their authored texture explicitly supplied; the
16-pixel fixture must also differ from a white-material control. Headless model
checks cover all seven asteroid types and exact LOD boundaries. Audio checks include 4,096 virtual backend voices,
2,049 facade loops, separate one-shot pressure, narration and stale handles.

Full headless integration checks run in ordinary, optimized and AddressSanitizer
builds, with LeakSanitizer enabled. GPU address checks disable leak detection
because the previously identified unsuppressed Mesa shutdown allocations remain;
that is not a claim that the GPU process has a clean leak check.

Ship-state fingerprints are useful before collisions but are not a portable
replay guarantee. Original moving collision IDs are heap addresses. Allocation
layout can change collision pair direction and subsequent outcomes, including
between runs of the same executable. The same-object original-policy comparisons
avoid that confound. Establishing portable collision identities and original
Windows/network reference traces remains a separate compatibility task.

## Measured limits and follow-up

The initial uncapped implementation spent about 60–64 ms per simulation step in
the opening 1,024-per-team formation. Native spatial queries reduce the optimized
opening step to roughly 2.5 ms. Congested sustained combat remains substantially
more expensive; these improvements do not establish comfortable 165 Hz play for
2,048 active AI in one sector.

Isolated actual-client runs on the 2,560 × 1,440, 165 Hz Wayland display, with
validation, full logical fleet audio and original effect textures, measured:

| Arrangement / frame window | Simulation median / p95 (ms) | Preparation median / p95 (ms) | Render CPU median / p95 (ms) | Displayed frames/s |
| --- | --- | --- | --- | --- |
| Dense opening, 100–799 | 2.77 / 3.45 | 1.67 / 2.48 | 0.80 / 0.97 | 164.76 |
| Dense congested, 3000–3999 | 18.61 / 29.09 | 9.97 / 13.17 | 1.66 / 1.85 | 28.50 |
| Dense whole run, 100–5998 | 10.55 / 23.06 | 6.52 / 10.93 | 1.54 / 2.37 | 44.10 |
| Spread opening, 100–999 | 2.81 / 3.60 | 1.39 / 2.03 | 0.66 / 0.87 | 163.91 |
| Spread late combat, 9000–9998 | 14.07 / 16.50 | 10.09 / 10.91 | 1.60 / 1.77 | 34.90 |
| Spread whole run, 100–9998 | 6.22 / 14.87 | 6.08 / 10.68 | 1.06 / 1.63 | 54.42 |
| Dense idle control, 100–998 | 1.09 / 1.67 | 0.45 / 0.66 | 0.35 / 0.57 | 165.00 |

The dense run peaked at 138,713 submitted particle quads, 3,502 draw items,
19 world draw commands and 1,376,839 visible triangles; the spread run peaked at
144,285 particle quads and 16 world draws. Queue-completion feedback medians
were about 1.0–1.3 ms in these windows. This supports efficient GPU submission,
but the simulation and preparation medians already exceed the 6.06 ms display
period during sustained combat. Opening input-sample-to-display medians were
11.81 ms dense and 10.32 ms spread; dense congestion reached 46.42 ms median.
The 1,000-frame idle control retained all 2,049 ships and had zero pacing misses;
its sample-to-display median was 7.39 ms. Its corrected asteroid scene is
`/tmp/allegiance-scale-final-idle.png`.
The runs completed with no Vulkan validation errors. Their raw CSVs and captures
are `/tmp/allegiance-scale-final-{dense,spread}-{gpu,timing}.csv` and
`/tmp/allegiance-scale-final-{dense,spread}.png`. Dense and spread measurements
preceded the load-time asteroid inheritance correction; the per-frame submission
algorithm was the same. The idle control includes the correction.

Earlier headless presentation profiles used placeholder 64 × 64, one-frame
effect-image metadata. They remain simulation/spatial-query diagnostics, but
their particle counts and preparation costs are not equivalent to actual-client
effects. The benchmark now loads and caches the real effect dimensions/frame
counts. The actual Wayland measurements above always used real metadata.

A fresh isolated 10,000-tick dense headless run with real effect metadata
completed with simulation median/p95/p99 9.78/14.19/16.46 ms and presentation
2.88/4.62/6.09 ms. It peaked at 21,889 live models, 3,428 draw items and 142,663
particle quads (`/tmp/allegiance-scale-final-real-metadata.csv`). Headless
presentation prepares the world-sound descriptions; it does not run the sound
graphs/OpenAL backend, HUD or Vulkan, so this is not an achieved-FPS measurement.

Profiles identify original KD collision traversal, avoidance queries/tree builds,
target and visibility lookups, particle preparation and world-audio preparation
as the remaining work. Wider and idle fleets are useful controls, but cannot
substitute for the congested battle when assessing the frame budget. Further
scaling work should reduce the measured candidate/lookup and presentation costs,
then examine independent work that can execute concurrently. Changing AI cadence,
collision ordering or simulation timesteps requires its own equivalence audit;
it must not be introduced as an unreported benchmark shortcut.

## 512 and 256 per team, with a closer observer

These measurements predate the disc placement change: four isolated actual-client
runs used the same dense 120 m grid formations, paired
attack orders, seed 1, 6,000 frames, 1/165-second simulation step, 65-degree FOV,
full audio and Vulkan validation at 2,560 × 1,440. Each count was measured at the
original 9.9 km observer distance and again at 3.5 km. Actual cockpit eye distances
were 9,895.2 and 3,495.7 m, respectively, and remained fixed through each run.
Initial creation was verified at 1,025 and 513 ships, including the observer.
Both closer captures were inspected and show the corrected textured asteroids.

Whole-run statistics omit the first 100 frames and the last screenshot frame.
The busy comparison window is frames 3,000–3,999, matching the earlier dense
1,024-per-team comparison. CPU columns below are busy-window medians in ms:

| AI per team | Observer distance | Whole-run FPS | Busy-window FPS | Simulation | Preparation | Render CPU |
| --- | --- | --- | --- | --- | --- | --- |
| 512 | 9.9 km | 91.29 | 67.61 | 6.44 | 3.93 | 1.09 |
| 512 | 3.5 km | 84.08 | 54.28 | 7.06 | 4.33 | 1.41 |
| 256 | 9.9 km | 164.86 | 165.00 | 1.98 | 1.21 | 0.56 |
| 256 | 3.5 km | 164.91 | 164.67 | 2.18 | 1.53 | 0.72 |

| AI per team / distance | Busy total CPU median / p95 / p99 (ms) | Busy queue-completion median / p95 (ms) | Busy sample-to-display median / p95 (ms) | Peak submitted particles | Peak world draws |
| --- | --- | --- | --- | --- | --- |
| 512 / 9.9 km | 11.39 / 16.57 / 17.86 | 0.84 / 0.90 | 20.75 / 24.33 | 69,259 | 19 |
| 512 / 3.5 km | 12.66 / 19.76 / 20.72 | 1.25 / 1.48 | 23.44 / 27.78 | 84,392 | 19 |
| 256 / 9.9 km | 3.74 / 5.58 / 6.31 | 0.59 / 0.62 | 7.63 / 8.77 | 32,631 | 19 |
| 256 / 3.5 km | 4.47 / 6.37 / 7.18 | 0.85 / 0.96 | 8.43 / 12.08 | 40,380 | 19 |

Total CPU is measured per frame before taking percentiles; it is not the sum of
the separate percentile columns. In these 6,000-frame runs, at 256 per team,
displayed cadence stays near 165 Hz even closer to the encounter, although the
busy close-view tail exceeds
the 6.06 ms period. At 512 per team, simulation alone reaches or exceeds that
period in the busy phase, and preparation adds another roughly 4 ms. Doubling
teams from 256 to 512 roughly triples busy simulation time in both views, while
render CPU approximately doubles and queue completion remains below 1.5 ms at
p95. These runs reinforce the remaining simulation/presentation CPU bottleneck.

Moving the physical observer also changes visibility, audible sources and
possible AI interactions; each run is an independent live battle, with the
original heap-address collision identity caveat above. The distance comparison
therefore includes those effects as well as closer LODs and increased pixel
coverage. All four runs completed without Vulkan validation warnings or errors.

Local run metadata records opening, combat, busy, late and whole-run windows.
Generated reports are not tracked. Raw artifacts are
`/tmp/allegiance-scale-{256,512}-{far,near}-{gpu,timing}.csv`, logs with the same
count/view prefix, and `.png` captures. The standalone analysis script is
`/tmp/allegiance-scale-compare.py`. Run the same count and view with the current
disc arrangement using:

```sh
/tmp/freeallegiance-install/bin/allegiance-native \
  --scale-test 512 --scale-distance 3500 --seed 1 --frames 6000 --validation \
  --config /tmp/allegiance-scale-512-near.cfg \
  --scale-csv /tmp/allegiance-scale-512-near-gpu.csv \
  --timing-csv /tmp/allegiance-scale-512-near-timing.csv \
  --screenshot /tmp/allegiance-scale-512-near.ppm
```

Use 256 for the smaller teams and `--scale-distance 9899.495` for the original
view. The previous figures used grid placement and executable SHA256
`c439bc17ecf6a31ac311d8e246bb7d9ddce7b052fcbf577d645eddbd028a8720`.

## Longer 256-per-team grid runs

The previous grid build also ran for 12,000 frames at three observer distances.
These later encounters revise the optimistic opening result:

| Observer distance | Whole-run FPS | First 6,000 frames FPS | Later half FPS | Final 1,000 frames FPS |
| --- | --- | --- | --- | --- |
| 9.9 km | 163.71 | 164.92 | 162.53 | 164.83 |
| 3.5 km | 150.33 | 164.92 | 138.30 | 116.92 |
| 1 km | 137.22 | 159.28 | 120.78 | 104.71 |

All three completed with no Vulkan validation warnings or errors. The physical
observer affects AI interactions and visibility as well as rendering.
Local run reports retain all per-phase results. Raw artifacts are under
`/tmp/allegiance-scale-256-{far,near,close}-long`.

## Initial disc placement at 1.5 km

A 12,000-frame actual-client run with 256 AI per team used the simple independent
initial disc arrangement, 96 m minimum spacing, seed 1 and the new 1.5 km view.
The sampler rejects points outside the disc or too close to accepted points;
there is no persistent deployment machinery. Headless arrangement cost for both
teams was 0.19 ms at 256 per team, 0.56 ms at 512, and 1.79 ms at 1,024.

The displayed frame rate averaged 157.04 FPS overall, 162.52 in the first half,
152.00 in the later half and 123.50 in the last 1,000 frames. Whole-run total CPU
median/p95/p99 was 4.72/6.15/7.17 ms. The run started with 513 ships, finished with
505, and peaked at 39,502 submitted particles and 18 world draws. The actual eye
stayed 1,495.7 m from the center. There were no Vulkan validation warnings or
errors; the final capture shows close combat and textured asteroids.

Local run reports and raw artifacts under `/tmp/allegiance-scale-256-disc`
retain the measurements.

## Larger disc encounters with the same setup

The same installed executable then ran 512 vs 512 and 1,024 vs 1,024, sequentially,
for 12,000 frames each. Seed 1, initial disc sampling with 96 m minimum separation,
1.5 km starting observer distance, 65-degree FOV, fixed 1/165-second simulation
step, original audio, full effects, 2,560 × 1,440 Wayland output and Vulkan
validation were retained. Disc radius grows with the square root of fleet count,
as in the 256-per-team run; all requested initial ships were verified.

| Encounter | Whole-run p99 simulation (ms) | Later-half p99 simulation (ms) | Whole-run displayed FPS |
| --- | --- | --- | --- |
| 256 vs 256 (reference) | 3.71 | 3.89 | 157.04 |
| 512 vs 512 | 10.83 | 11.24 | 65.26 |
| 1,024 vs 1,024 | 25.97 | 26.98 | 34.52 |

Simulation means CPU `step_ms`, excluding scene preparation and rendering.
Whole-run percentiles use frames 100–11,998, omitting startup and final screenshot
readback; later-half percentiles use frames 6,000–11,998. Both larger runs
completed without Vulkan validation warnings or errors. The 512 run began with
1,025 ships and ended with 1,007; the 1,024 run began with 2,049 and ended with
2,034. Actual eye distance drifted during the larger battles: 1,495.7–1,580.1 m
at 512 per team and 1,495.7–1,576.1 m at 1,024, versus a fixed 1,495.7 m in the
reference. These are independent live encounters, not exact replays.

Local comparison reports retain the command lines and timing distributions.
Raw CSVs, logs and captures use the prefixes
`/tmp/allegiance-scale-{256,512,1024}-disc`.

## 8,192 vs 8,192 failure and slowdown probe

The same executable and initial setup ran a shorter 3,000-frame probe: 18.18 s
of fixed-step simulation, including the first combat phase. All 16,385 ships
were created, the client exited normally, and the log contained no Vulkan
validation warnings or errors. It ended with 16,367 ships. This establishes
startup and early-combat survival at this count, rather than long-run stability.

| Window | Simulation median / p99 (ms) | Preparation median / p99 (ms) | Displayed FPS |
| --- | --- | --- | --- |
| Frames 100–2,998 | 82.00 / 132.69 | 32.26 / 64.60 | 7.48 |
| Combat, frames 2,000–2,998 | 106.44 / 154.37 | 49.75 / 65.28 | 5.81 |

Render CPU median/p99 was 9.24/13.52 ms; submitted queue-completion feedback was
2.02/3.66 ms, not isolated GPU shader time. Peaks were 1,129,273 submitted
particles, 3,711,597 triangles and 18 world draws. Observed resident memory at
1m52s was 2.20 GiB (a sample, not a peak). The eye remained fixed at 1,495.7 m.
The final capture shows functioning rendering and textured asteroids, with
severe HUD contact clutter.

For matching frames 100–2,998, the prior 1,024-per-team run had p99 simulation
12.93 ms, versus 132.69 ms here: about 10.3 times the cost for eight times the
ships. The observed limit is CPU simulation and scene preparation; this probe
did not encounter a hard object-count limit or a renderer failure.

Local probe reports retain the measurements and matched-window comparisons.
Raw CSVs, log and capture use the prefix
`/tmp/allegiance-scale-8192-disc`.

## 2,048 vs 2,048 completed rerun

A fresh 12,000-frame run used the same executable, seed 1, initial disc placement,
1.5 km starting observer position and renderer/audio settings. The first attempt
was discarded after accidental input. All 4,097 initial ships were verified; the
completed run ended with 4,083 ships and no Vulkan validation warnings or errors.

The observer was killed during this run, returned to hangar, and was manually
launched again. The diagnostic heals between steps and does not prevent lethal
damage within one step. Frames 6,216–6,306 had no simulation step; the active
statistics below exclude those 91 frames, as well as the usual first 100 and
final screenshot frame. Position and orientation changed after launch, so this
is not a fixed-camera rendering comparison.

| Active window | Simulation median / p99 (ms) | Active displayed FPS |
| --- | --- | --- |
| Whole run | 34.64 / 60.15 | 17.77 |
| First half | 23.62 / 36.89 | 23.58 |
| Later half | 45.84 / 61.99 | 14.26 |
| Final 1,000 frames | 52.42 / 64.97 | 12.53 |

Active FPS uses consecutive compositor display intervals with active endpoints;
menu gaps and transitions are excluded. Including menu frames gives 17.79 FPS
for the whole run. Peaks were 299,189 particles, 1,664,653 triangles and 19 world
draws. For matching frames 100–2,998, simulation p99 was 29.14 ms, versus 12.93 ms
in the prior 1,024-per-team run.

Local reports retain the raw and active timing distributions and command.
The completed run's artifacts use the prefix
`/tmp/allegiance-scale-2048-disc`; the discarded attempt uses
`/tmp/allegiance-scale-2048-disc-interrupted`.

## Extended 768 vs 768 encounter

The same executable and initial setup ran 24,000 frames to give the later grouped
AI behavior more time. All 1,537 ships were created, and measured simulation
advanced 141.27 seconds: approximately twice the completed 2,048-per-team run.
There were three menu intervals, totaling 691 frames with no measured simulation
step; the active statistics below exclude them, plus the first 100 and final
screenshot frame. The viewpoint changed during play and ended in external view
near the base, so the FPS comparison includes those camera changes.

| Active window | Simulation median / p99 (ms) | Active displayed FPS |
| --- | --- | --- |
| Whole run | 12.64 / 17.12 | 43.11 |
| First 12,000 frames | 11.38 / 17.18 | 45.78 |
| Second 12,000 frames | 13.21 / 17.03 | 40.70 |
| Final 1,000 frames | 12.91 / 16.63 | 41.88 |

The client exited normally, ended with 1,483 ships and had no Vulkan validation
warnings or errors. Peaks were 112,446 particles, 818,927 triangles and 20 world
draws. The final capture shows clustered fighting and tangled ship trails near
the base, with multiple wingmen reporting the same target. Later simulation p99
remained similar to the first half instead of continuing to grow.

Local reports retain the extended run's timing distributions and command.
Raw CSVs, log and capture use the prefix
`/tmp/allegiance-scale-768-disc`.

## Installed checkpoint

The optimized build and all six shaders are installed in
`/tmp/freeallegiance-install`. Build and installed executable match SHA256
`35c213d757cd158987982b3f6c7401662a8e68f1bcd79843f60ae39770c06b9d`.
This checkpoint adds the optional docked command observer. The command, camera
and session checks passed, including replenishment of both AI teams while
docked. An interactive Wayland/Vulkan validation run created all 2,048 AI ships
and the docked pilot, with the command camera at the new 3 km default. The
planet/panel Vulkan pixel checks also passed. The historical benchmark reports
above retain the earlier executable hash and physical-observer setup.
Command camera input now requires keyboard focus, and edge scrolling also
requires mouse focus, preventing an inactive `(0, 0)` pointer from panning the
observer away from the encounter.
HUD font sizes are reduced to better match the original bitmap fonts' visible
height relative to the artwork; a 120-frame 2560×1440 command-view capture
verified the smaller inventory, map and radar labels with no Vulkan validation
messages.
This checkpoint also adds separate saved pilot/command grid preferences and
the tighter four-hull diagnostic fleets described above. The session, command
and effects checks passed, including armed mixed hulls, nonoverlapping initial
spawns and hiding both command overlays while preserving world effects. A
120-frame Wayland/Vulkan run at 2560×1440 verified 256 ships of each hull on
each side, the 3 km command camera, and command overlays disabled while the
pilot grid preference remained enabled. No Vulkan validation messages appeared.
