# Native-client completeness and equivalence audit

Status: active implementation queue. The native executable supports practice,
original-core local Conquest and multiplayer through a separate native authority.
This is not yet a certification of complete parity or original Windows wire compatibility.
This audit covers every native production module and the non-IGC client policies
it replaces. It distinguishes inspected differences from tested equivalence;
source reuse alone is not a passing result. A native authoritative multiplayer path is now implemented and under integration audit.

The [fleet scaling audit](native-scale.md) records the native-only spatial
acceleration, renderer batching, large-fleet tests and remaining performance limits.

## Procedure and acceptance gates

1. Inventory each replacement and every IGC site callback. Follow the original
   call chain through WinTrek, BaseClient, IGC and, where authority differs, FedSrv.
   An inherited no-op or `return true` is an unimplemented obligation, not a feature.
2. Compare inputs, object lifetime, time convention, random draws, state/command
   masks, ownership/authority, side visibility, and outputs. Separate presentation
   differences from gameplay differences and intentional practice-only rules.
3. Extract reusable original behavior into ClientCore. Keep OS/rendering and
   message transport adapters separate. Never make shared IGC depend on Vulkan,
   SDL, GLM, UI, or the native session.
4. Pin equivalence with original-reference comparisons and real IGC integration
   cases. Document intentional corrections and any platform/reference not tested.
5. Check hot-path cost before changing WinTrek: no added frame-wide scans, per-frame heap
   traffic, locks, virtual dispatch, or conversion to native data structures.
   Templates here write into existing caller storage and inline the RNG callback.
   Windows packet definitions, layouts and routing stay unchanged.
6. Run affected integration tests and sanitizers, then the actual native display
   path. Keep remaining gaps open. Do not use absence of crashes as an equivalence
   verdict or describe a successful native test as a tested Windows build.

The generated [callback inventory](native-callback-audit.md) is the exhaustive
name-level checklist for `IIgcSite`, `ThingSite`, `ClusterSite` and their `AttachSite` base; the matrix below
supplies behavioral findings. Regenerate with
`python src/native/audit_callbacks.py > docs/native-callback-audit.md`.

## Module and behavior matrix

| Area / native replacement | Original reference | Finding and equivalence gate | Queue |
|---|---|---|---|
| Core build / `CMakeLists.txt`, `legacy.*`, `zlib/platform.h` | `Igc/*`, zlib math/time/containers, Utility collision | Original simulation/math/collision compile directly. Audit found and fixed native tail-padding reuse corrupting ammo/fuel pack definitions. Native static-record layout, defined-field re-export, compact time and Windows-CRT RNG now have reference checks. Nineteen dynamic layouts and 144 offsets, all five installed map re-exports, and selective imports are checked. Original-build runtime traces and complete packet/pointer ABI coverage are still open; compiling common code is not deterministic replay proof. | Q09 |
| Session construction / `session.hpp`, `game.cpp` constructor, ship/equip/spawnBots/objectives | `training/*`, `clintlib/clintlib.cpp`, FedSrv mission setup | Practice retains PCore014/full-tech/free-equipment fixtures. Conquest now uses static_core, its five civilizations, original generated maps, buckets, funds, miners, constructors and technology constraints. Original narrated training now runs through its original actions/conditions; all fifteen generated-map types, two through six teams, per-team civilizations/alliances, expanded rules and local pilot/readiness/countdown flows are wired. Native remote joining and lobby lifecycle are implemented; further terminal-event coverage remains. | Q06, Q08 |
| Controls / `main.cpp`, `Game::step`, target/weapon/autopilot methods | `WinTrek::UpdateJoystick`, trek input, BaseClient command handling | Original IGC consumes the mask; current/accepted/queued command transitions and all target searches now share WinTrek policy. Device translation remains native; keyboard steering and mouse acceleration/virtual-joystick accumulation now share original policy. Forward/reverse thrust, ripcord/eject and domain-validated team orders are now present; SDL joystick/gamepad axes, buttons and hats now feed the same native actions as keyboard/mouse. Keyboard/controller configuration now includes 168 actions, with modifiers, aliases, configurable cargo swaps, station teleport, message paging, grid and screenshots, including HUD, chat, commander, look and camera controls. Original force-feedback callbacks now feed translated installed presets and virtual-tested rumble. Original mouse event-stream checks and simultaneous-device mapping now pass; further roster coverage and physical device/haptic verification remain. | Q07 |
| Missile launch / native FireMissile | `BaseClient::FireMissile`, `Inc/Messages.h::MissileLaunchData` | Fixed through shared `CombatActions.h`: time extrapolation, stratified horizontal/vertical salvo dispersion and randomized launch distance. 108 original-reference launch cases check vectors and RNG call ranges/order. Windows packet writes are unchanged. | Q01 |
| Chaff/mine/probe deployment / native FireExpendable | `BaseClient::FireExpendable` | Shared original offline creation/decoy selection and connected requests retained in BaseClient. Tests cover delayed mine arming, moving/stationary placement, probe target, chaff success/failure and isolation of unrelated missiles. Native depletion now invokes shared reload. | Q01 |
| Sector transit / native HitWarpEvent | `WinTrekClient::HitWarpEvent`, server transit handling | Fixed via `WarpTransit.h`: original entry-offset error drives pitch/yaw/roll and lateral displacement. Twelve normal-entry reference cases and actual IGC sector transit pass. Stationary-entry divide-by-zero is intentionally fixed. Online authority remains deferred. | Q02 |
| Inventory / equip, partChoices, replacePart; frontend Parts | `BaseClient::Reload`, ship/loadout IGC methods, Loadout UI | Shared original transfers/remount delay and depletion notifications; hangar stocks/clears all five cargo slots and fills empty mounted slots. Ammo/fuel/launcher depletion exercised through actual IGC callbacks. Conquest now charges hull/part/refit purchases, checks station technology, and uses original preferred/default cargo priorities. Flight cargo navigation, swap, drop and manual reload now use shared original policy. Persistent ten-bank loadouts retain original purchase ordering and binary format. Practice equipment is deliberately free. | Q03, Q06 |
| Lifecycle / kill, dock, recover, launch, treasure callbacks | `BaseClient::KillShipEvent`, WinTrek docking/carrier/rescue/ripcord; FedSrv | Shared ejection, salvage, carrier/ripcord placement and resupply replace direct deletion. Native pods/rescue preserve player IDs; station changes use IGC drone wait/safety policy. Flag/treasure cleanup and recovery are wired. Boarding, turret cycling/fire, pilot promotion, docked disembark and parent death now use original IGC relationships. Native end-to-end crew and actual carrier garage tests pass. Server/offline carrier refits share original policy and have 512 frozen comparisons. Station teleport shares original destination authorization, crew movement and hull refund/pod replacement with FedSrv. Shared station evacuation/elimination preserve observers, crew, human IDs and accounts; immediate station termination, same-update losses and continuing-team controls are verified. | Q05 |
| Visibility / Thing getters, contacts, scene, minimap, bot targeting | `ThingSiteImpl` visibility, `ClusterSiteImpl` scanner lists, model/ship CanSee, server visibility | Per-side scanner lists, cached authorized spotters, alliance/cloak/range/occlusion rules and retained discovery now filter native consumers. Shared fast predicate leaves the WinTrek fallback scan unchanged. Static station appearance survives unseen upgrades; constructed asteroids retain history through original IGC stale-rock callbacks. Native integration tests cover unseen station upgrades, capture/destruction and historical asteroid selection/navigation through both discovery paths. | Q04 |
| Core progress / research, building, mining, economy, side/mission state | IGC site completion callbacks, BaseClient, FedSrv, command UI | Conquest now executes IGC production, mining, construction, research, captures/destruction and shared FedSrv victory rules. Research and remote-sector legal-order menus are wired. Flags/artifacts, kill/prosperity/time goals and a team scoreboard now use original mission rules. Expanded mission parameters, individual wallets/donations/requests, leadership, departures and allied rosters are implemented; remaining economy/loadout terminal combinations still need coverage. | Q06 |
| Camera / `CameraMotion.h`, game adapter | original CameraControl / ComputeExternalChase | Extracted shake, interpolation, chase and target preview are shared/tested. Chase integration and animation cancellation intentionally corrected. Original tracking/flyby/orbit geometry is now shared and native exposes external, target, flyby, orbit, station tracking and missile modes in addition to chase/cockpit. Turret cameras use parent hardpoint offsets, independent gunner orientation and original throttle zoom. Command view, rectangle/multiple selection and height feedback use shared original policy; Original three-second launch/carrier and five-second frozen death framing now run through shared camera geometry. Further crew/terminal camera transitions remain under audit. | Q07 |
| Effects / `particles.*`, Thing/SectorSite adapters | `effect/thinggeo.cpp`, particle/trail/debris/shockwave/aleph/building effects | Particle appearance/trajectories, teleport ramp, mining bolt subdivision and build-shell motion now use shared original equations. Damage slots and trail fading are corrected; construction uses its authored mesh and alpha/additive shells. Curved shield hits, animated alephs, typed explosions/shockwaves, mine fields and hull impacts are wired. Authored model strobes, sector skies, planet posters and lens flares are now wired. Muzzle flashes and sector pulse rings are disabled in the original client, not missing native behavior. | Q10 |
| Art / `assets.*`, `media.cpp`, `spatial.hpp`, `primitives.*` | modeler MDL parser, MeshGeo, FrameData, ThingGeo, poster/environment code | The supported cores and local maps pass a complete reference inventory, including every mapmaker planet and sky variant. Binary mesh/texture/group/frame/light nodes and authored LOD thresholds are retained; unsupported functions fail explicitly. All inventory model nodes are supported; none require an animated material/transform graph. Sector environment/planet art is retained. CVH coordinates and original star/strobe/LOD math have reference tests; full cross-platform visual captures remain. | Q11 |
| GPU / `renderer.*`, shaders | engine context/geo/material rendering | Intentional Vulkan replacement, not D3D architecture adoption. Bindless reverse-Z rendering and instanced HUD/effects are present. Original sector primary/alternate/ambient vertex lighting is restored; alpha cutout remains. A depth-tested alpha/additive model pass now sorts transparent model instances without per-frame allocation. Authored model LOD selection and background layer ordering are restored. Core Preload warms future art; skin variants share immutable uploaded geometry. Visible alpha meshes and additive particles now share stable depth ordering; a pure-additive fast path avoids sorting. Real Vulkan pixels verify attenuation, line-center sorting and opaque occlusion. Full capture equivalence remains. Verify semantic output, not implementation identity. | Q11 |
| HUD / `hud.*` | inventory/indicator/radar/sector/command panes, MDL UI | Native atlas/layout presents the original gameplay view models with scalable fonts; status/target snapshots now use shared original weapon/relay and per-object radar policy, angular brackets and damage-bucket threats. Further labels/gauges, weapon readiness, orders and equipment combinations still need exhaustive coverage. Message paging retains history and the original ten-second return to newest messages; appearance preferences expose center/target/history visibility. Text rasterization/layout may intentionally improve. | Q07, Q04, Q06 |
| Frontend / `frontend.*`, `settings.*`, desktop/install | WinTrek screens/configuration/client events | Local menus now launch the original narrated missions and introduction/debrief slides; offline lobby teams/civilizations/alliances, real local pilot seats, readiness and cancellable countdowns are implemented. Remote lobby/player replication now has a separate native authority and integration checks. The expanded binding/settings set supports reassigning and clearing interface/camera keys; supported original JSON preferences and saved keyboard/mouse/controller maps migrate read-only, including reviewed multi-device routes; completed-session and inactive-team account/loadout/command guards pass. Training retains its deliberately inactive-side lifecycle; team scores and eliminated-team HUD/menu returns are verified. Native remote victory and next-round delivery are now implemented. OS persistence replacement is intentional. | Q06–Q08 |
| Audio / `audio.*`, ShipSound, game sound events | SoundSite/ShipSoundSite, sound-template graph/DirectSound | Original SoundEngine templates now compile directly; native sound MDL feeds OpenAL with original random/pair/ASR/repeating/intermittent/serialized/cone behavior. Ship/weapon/ambient loops, moving attachments, shared warnings, damage and explosion selection, and friendly AI chat are wired. Training narration uses actual original sound completion; All 307 authored quick-chat commands and voices are wired, with a preloaded fallback for the missing Woopie waveform; hangar station ambience and strongest-lock incoming-missile warnings are restored; live effects/voice category gains propagate through active and queued original graphs without decoding or frame polling; further sound/event coverage is under audit; ten optional-core audio assets are absent from the installation. | Q12 |
| FMV / `movie.*` | original movie wrapper and screen lifecycle | FFmpeg decode/audio sync is an intentional backend replacement. Original intro assets decode with video/PCM, focus pause, natural completion, skip/teardown and independent movie/world audio policy tested. Original training uses its authored slides; no additional campaign-movie caller was found. Startup credits→intro sequence, one-shot intro preference, skip and return to Home are verified. | Q08, Q12 |
| Window/input/pacing / main, presentation, FramePacer | Win32/DInput/window loop; user-provided 0ad controller | Intentional SDL/Wayland and presentation-feedback replacement. Measured 165 Hz behavior is not exhaustive multi-monitor/VRR/focus/gamepad testing. Platform integration must not alter simulation time semantics. WinTrek does not compile this code. | Q09 |
| Native multiplayer / `network_records.*`, `network_transport.*`, `network_session.*`, `network_game.inl` | `clintlib/appmsg.cpp`, message protocol, FedMessaging, FedSrv | A separate original-IGC authority and native replica now exchange exact-build flat records through GameNetworkingSockets. Prediction, visibility, commands, lobbies and reconnects have integration checks. Original Windows wire interoperability remains separate work. | Q13 |

## Ordered execution queue

Dependencies matter: visibility and lifecycle come before calling skirmish or
commander mode complete; a transport cannot fix incompatible client behavior.

| ID | Status | Work and exit criterion |
|---|---|---|
| Q01 | Implemented; native verified | Share missile launch data and offline expendables; verify original math, RNG sequence, timed launch state, IDs/targets, ammo accounting. Leave connected packet paths untouched. |
| Q02 | Implemented; native verified | Share offline aleph exit; compare original normal-entry kinematics and test center/off-center/zero-speed entry and real sector crossing. |
| Q03 | Implemented; native verified | Share cargo/ammo/fuel/launcher reload; expose reserves in hangar; test selection order, partial/full consumption, remount delay, alternate launchers and empty-part lifetime. |
| Q04 | Implemented; native visibility and historical UI verified | Side-aware scanner registration, visibility/current-eye/discovery policy; conceal contacts, target/lead, particles, scene and minimap consistently. Add cloak/alliance/sector tests. |
| Q05 | In progress: pods, salvage, station/carrier docking, ripcord and flag lifecycle | Death→pod→rescue/dock/rearm, salvage/treasure, carrier landing and ripcord. Preserve ship IDs and original event/authority boundaries; exercise all terminal transitions. |
| Q06 | In progress: original-core Conquest, economy, construction and mission rules | Genuine loadout transactions/tech/civilizations, lifecycle and mission parameters; station/asteroid capture/destruction, research, miners/builders, commanders/orders and team state. Use original domain operations, not a new economy. |
| Q07 | In progress: configurable interface/cameras, launch/death views and crew controls | Original command/input and HUD view models, all supported camera modes, weapons/turrets, radar/lead/range semantics and full keyboard/controller coverage. |
| Q08 | In progress: original missions and narrated slides run natively | Original narrated training actions/conditions via native services; real campaign/lobby/team/session flow. Retain custom exercises only as explicitly separate practice fixtures. |
| Q09 | In progress: static layouts, compact time, re-export and Windows-CRT RNG verified | Core/type/layout/time audit and reproducible state/event traces. Compare serialized fields and outcomes against the original build; avoid assuming LP64/raw structs/RNG are portable. |
| Q10 | In progress: shared particles, world effects and impact geometry | Extract effect simulation/curves from original sources; native batches consume shared state without reintroducing legacy rendering or changing WinTrek allocation/update cost. |
| Q11 | In progress: asset inventory, LOD, lighting, backgrounds and transparency pixels verified | Inventory every referenced asset from supported cores; preserve MDL/material/animation/LOD/environment semantics, transparency and lighting; expand reference captures. |
| Q12 | In progress: original graph and world sound integration | Original sound graph/event selection, spatial cones, world/weapon loops, notifications/speech, FMV completion/skip integration. |
| Q13 | Native multiplayer implemented; native integration checks | Original IGC authority and ClientCore policies, fixed simulation ticks, prediction/reconciliation, delayed remote presentation, visibility replication, exact-build flat records, reconnect and multiplayer lifecycle. GameNetworkingSockets is an isolated backend. Original Windows protocol interoperability and cross-platform replay are not established. |

## First execution batch: evidence and limits

- Q01: `ClientCore/CombatActions.h` replaces duplicated native launch/deployment
  code and the corresponding BaseClient blocks. Native salvo storage is reserved
  when loadouts are warmed. Windows still writes its own `MissileLaunchData[]`
  directly; no intermediate array or conversion is introduced. Corrected the
  original mismatched array `delete` to `delete[]`.
- Q02: `ClientCore/WarpTransit.h` is used by both offline adapters. The connected
  Windows guard is retained. Normal-entry math matches the reference; the only
  deliberate kinematic correction is finite behavior at zero entry speed.
- Q03: `ClientCore/Inventory.h` retains the original transfer order, full/partial
  record semantics, alternate-launcher fallback, mounted fractions and load time.
  Windows uses its original eight-record stack buffer. Notifications retain the
  original order. Connected reload/drop transport and offline AI resupply handling
  remain in BaseClient. Native AI resupply navigation/relaunch remains Q05; this
  batch completes reserve transfers, not the entire inventory/economy lifecycle.
  Cargo has a separate HUD section with quantities and no active fire/arming cues.
  Reload/remount/depletion sounds are preloaded; ship notification routing is shared
  with WinTrek so another ship's reload does not trigger the player's announcements.
- Q09 discovery: a Windows pack record has type at byte 396 and amount at 398,
  total 400. Before this fix Linux placed them at 393/394 and read padding as
  type -52. Explicit native-only base padding restores the on-disk ABI. Installed
  `static_core` and `PCore014` ammo/fuel definitions are now validated; Windows
  layout is untouched. This is not a whole-core or network ABI certification.

`native-actions` uses frozen excerpts from original commit
`39ef2c072b1eac0e9f97ab1428bc96572cbd5854`, independent of the production
ClientCore headers. It compares 108 timed/partial/full missile salvos, twelve
centered/off-center aleph exits, and 22 inventory cases against real IGC objects.
The scripted RNG compares draw ranges and order, not merely final directions.
Additional cases verify zero-speed transit, mine timing/ownership, probe targets,
and successful/failed chaff diversion. Explicit test-only core variants exercise
zero/nonzero salvo dispersion and target-only probes when this hull's authored
loadout does not cover those branches. These are same-compiler comparisons, not
cross-platform replay proof.

`native-sessions` now exercises actual cargo selection, ammo and missile depletion,
reserve reload, exhausted-launcher removal, docking and empty-slot refit, plus
fuel exhaustion and actual autopilot/collision-driven aleph transit. All eight
CTest checks pass normally and under AddressSanitizer/LeakSanitizer. The extended
1,600-frame Wayland/Vulkan frontend test stocks and clears cargo through actual
HUD hit testing, then launches/leaves/restarts; no Vulkan validation errors.
The cargo and hangar menus were also visually inspected at the native display resolution.
A 6,000-frame camera/boost/audio run of the installed final build with Vulkan synchronization validation held
164.9993 Hz on the 165 Hz display with zero pacing misses. After discarding 1,800
warm-up samples, sample-to-present median/p95/p99 were 2.640/4.390/4.408 ms;
sample-to-submit was 0.155/0.220/0.345 ms (4,198 feedback samples). These are
application/presentation timestamps, not measured input-to-photon latency. This
native run does not measure WinTrek performance.

The four connected packet blocks (`FIRE_MISSILE`, `FIRE_EXPENDABLE`, `RELOAD`,
`DROP_PART`) were compared with the original and are unchanged. These extractions
run on discrete gameplay events; no WinTrek frame-loop scans, native containers,
locks, renderer dependencies or new allocation passes were added. The shared
headers compile on Linux; a Windows build and performance run remain unperformed.
No Windows performance result or complete client equivalence is inferred.

## Subsequent execution: visibility, lifecycle and mission authority

- `ClientCore/Visibility.h` shares the WinTrek fast predicate and native per-side
  scanner registry/retention. Real IGC tests cover front/rear range, cloak,
  asteroid occlusion, stations/probes, alliance policy/revocation, sector changes
  and scanner teardown. Native consumers filter contacts, targeting, geometry,
  effects, sound and remote sector counts. Static appearance is cached separately
  from authoritative station upgrades; capture/destruction retain the original
  public-notification semantics. Construction retains unseen asteroid observations
  and connects `ClusterIGC`'s existing stale-rock removal callback.
- `ShipLifecycle.h` shares salvage, ejection kinematics, carrier launch, ripcord
  arrival and resupply decisions. Frozen original excerpts test kinematics. Real
  native tests fly a pod home, abandon/rescue/refit it, preserve ship IDs, and fly
  through an aleph before charging a ripcord home. Missile deletion now executes
  the original blast. Damage-track ownership is released on both ship termination
  and mission teardown; IGC detaches ships during mission termination rather than
  terminating each ship. Parent/turret/observer work is still open.
- `MissionEconomy.h` shares part budgeting, preferred/default cargo priorities,
  development effects, drone upgrades and authoritative station construction.
  Native Conquest uses the installed `static_core`, civilization base attributes
  and original map generators. Its test funds a miner, waits for actual bucket
  completion, follows He3 delivery, funds/orders a constructor and observes the
  original build effect replacing the asteroid, then completes real research.
  All five installed civilizations launch on all four offered map types.
- `MissionRules.h` is used by FedSrv and native for conquest/territory rankings,
  loss of restart bases, and alliance-survivor selection. Tests cover changed
  rankings, contested sectors, empty-team rules and elimination. Native station
  callbacks no longer leave base loss without a mission outcome.
- `ShipOrders.h` shares original default-command and miner-sector memory rules.
  The order UI enumerates known legal targets on demand across sectors. Undefined
  command IDs are rejected before entering IGC, whose LegalCommand switch has no
  invalid-enum rejection. No target enumeration was added to WinTrek's frame loop.
- `FlagLifecycle.h` shares server flag respawn/drop operations; native treasure
  expiration now terminates objects, flag loss scatters or respawns them, and
  delivery scores flags/artifacts. Full flag-mode UI and session tests remain.
- Native drone docking now waits for original IGC `OkToLaunch` (including its
  ten-second wait and nearby enemy rules), with station launch in `ChangeStation`.
  Recovered equipment applies its authored technology on docking at an own base.

Intentional corrections/policies: Conquest charges the remaining default-cargo
slots that the old client's loadout preview created without debiting its budget;
otherwise local refits could mint credits. Practice respawn is disabled for an
existing Conquest player. Native `CreateDrone` applies the passed skills, which
its original factory currently ignores. None of these asserts network equivalence.
The original stockpile completion hooks are retained; no original stockpile
consumption call site exists, so it is not treated as a supported purchasing API.

The first twelve-test run (before the latest static-history/flag changes) passed
both normal and ASan/LSan checks. Later tests are being rerun as the queue proceeds.
The Conquest research menu also ran for 900 frames on Wayland/Vulkan at 165 Hz,
zero pacing misses and no validation messages. This is a menu run, not a combat
renderer performance result. Windows compilation/performance remain unperformed.

## Confidence required before networking

Every gameplay-affecting callback must have an implemented, explicitly deferred,
or deliberately inapplicable disposition. Test full mission lifecycle, resource
conservation, object identity/lifetime, visibility and commands with original
cores, not only training fixtures. Compare original/native event and state traces
under controlled inputs. A Windows build and Windows-side performance/reference
run are still required: Linux checks cannot establish WinTrek binary performance
or complete behavioral compatibility. No such confidence is claimed yet.

## Sound-template and event integration

- Native now compiles `soundengine/soundtemplates.cpp`, the platform-independent
  portions of `soundutil.cpp`, and original event dispatch. The replacement
  backend implements the original PCM, position, instance and buffer-source
  interfaces. It preserves stereo for nonspatial sounds, mixes spatial stereo
  to mono, supports authored cones and AL loop points, and keeps release tails.
- A sound-only MDL parser supports every combinator in the installed sound
  namespaces. Preparation visits all random/intermittent/serialized branches
  without consuming playback choices or queue state; decoding, repeat-fire PCM
  mixing and AL buffer uploads occur before flight for core equipment.
- `ShipSound.h` additionally shares weapon activation/burst/last-shot selection,
  low-resource and empty-trigger transitions, ripcord countdowns, impact and
  explosion selection, and drone attack voice selection. Native world loops
  include other ships, mining, weapons/turrets, bases, alephs, asteroids, probes,
  missiles, projectiles and construction. Emitters follow live model offsets and
  orientation and terminate with their source. Original main/turn engine gain
  and pitch curves remain in use; view changes retain loop progress.
- Native receives friendly IGC AI chat/voice events with recipient filtering;
  station/drone attack alerts use the original thresholds and report cooldowns.
  Docking, shield/hull hits and explosion sizes now select the corresponding
  original sounds. This is not yet full training, quick-chat or event parity.
- The bounded OpenAL mixer assigns 128 physical sources by gain, priority,
  distance and cone, retaining virtual progress for less audible voices. Audio
  randomness uses a separate native stream, so sound choices cannot perturb
  simulation `rand()` state. Windows retains its original RNG path.

The installed catalog test resolves 1,025 definitions and 1,004 distinct wave
assets. A separate full-decode run validates actual PCM durations against ASR
parameters. Twenty optional-core definitions reference ten absent files:
`faohpsi1/2/3`, `faohtheta1/2/3`, `faohmu1/2/3`, and `faohaspg`. These are explicit
artwork limitations, not successful equivalence cases. Thirteen unused static
sound IDs have no definition. OpenAL loopback tests cover paired properties,
weighted random choice without simulation RNG contamination, intermittent
updates, ASR sustain/release, repeated-fire PCM, speech ordering/deduplication/
cancellation, cone attenuation, virtual voice capacity and facade teardown.

Corrections found during this work: the original repeated-fire 16-bit loop had
an always-false bound and read uninitialized PCM; cancelled queued speech could
later dereference a cleared template, and expired/reset speech could remain
logically pending. Those discrete sound operations are fixed in the shared
engine. FFmpeg Vorbis output is positioned at its decoded timestamps so authored
loop offsets survive its initial granule gap. The original HiLo generator also
leaked its extra central-sector creation reference; the five-civilization map
matrix now passes LeakSanitizer after balancing that reference.

An intermediate 3,000-frame Wayland/Vulkan flight run with the new graph/world
loops held 165 Hz with zero pacing misses. This precedes the final moving-source
and preload expansion; final verification is tracked in the subsequent run.
Windows build/performance and cross-platform sound/state traces remain unrun.

The final thirteen-test suite passed normally and under ASan/LSan, including the
native moving-source audio facade. A 900-frame sanitized Wayland flight with all
core sound definitions prewarmed reported no invalid-memory errors or Vulkan
validation errors and zero pacing misses. LSan separately reported two 256-byte
allocations whose stacks enter external/unloaded graphics modules (one through
the Vulkan loader/validation layer); that GPU process is not a clean leak check.
No suppression was added. The headless suite remains leak-clean.

`CameraFraming.h` now shares WinTrek's tracking, conservative flyby framing and
orbit rates. Native external, target, flyby, orbit, station tracking and missile
views retain original fallback and transition behavior. Tracking deliberately
handles coincident points without a zero-vector normalization. Geometry checks
compare the original formulas and actual game tests settle every mode. Missile
lifetime/render behavior is covered by the extended flight exercise.


## Crew lifecycle and gunner presentation

`ShipCrew.h` extracts FedSrv turret selection and WinTrek turret zoom. The bounded
selection retains cyclic equipped/unoccupied-mount policy, fixes nontermination
when changing to a hull with a different mount range, and requires an actual
weapon (the old scan could mistake another equipment type at mount zero for a
turret). Windows selection runs only on boarding requests; it introduces no
per-frame scan or allocations. Its transport, refund and player status handling
remain in FedSrv. The Windows build is still untested.

Native boarding refunds the original ship value in Conquest and retains the
player ID. Hull, equipment, audio, position and visibility come from the source
ship while input and turret orientation belong to the gunner. Original IGC
`SetParentShip`, `SetTurretID`, `ExecuteTurretMove` and `Promote` own the actual
transition and gunfire. Docking/launching carries children between station and
sector; destruction ejects each player separately. Observers are restricted to
practice's cheat-enabled context. A crew menu exposes boarding, cycling,
disembarking and promotion. Local pilot creation/control APIs provide offline
session authority and test multi-identity flows; they are not network joining.

The original-core Gunship test checks occupied/empty seats, observer conversion,
boarding, gunner ammo consumption/slew/zoom, promotion, autopilot docking with
passengers, relaunch, parent destruction and pod recovery. All thirteen tests
passed normally and under ASan/LSan after these changes. A subsequent 1,800-frame
Wayland/Vulkan gunner run exercised firing, turret cycling and cockpit/chase
transitions at 165 Hz with zero pacing misses or Vulkan validation errors; its
cockpit capture was inspected. Full original gunner HUD/command panes remain
part of Q07.

The preceding 6,000-frame external-camera/audio run also held 165 Hz with zero
misses. After 1,800 warm-up frames, sample-to-present median/p95/p99 were
2.305/4.623/4.651 ms; sample-to-submit was 0.176/0.228/0.372 ms (4,198 feedback
samples). These application timestamps are not input-to-photon measurements.


## Original particle and construction effects

`ParticleEffects.h` now supplies the original smoke/fire/afterburner/spark color,
alpha, size and trajectory equations, fire/smoke emission kinematics, damage
slot counts, teleport ramp/trajectory, mining bolt subdivision and construction
shell motion. WinTrek keeps its existing geometry, storage, callbacks and RNG;
these are inline equations, with no new per-frame scan or allocations. Native
keeps its independent visual RNG and bounded instanced batches.

Native exhaust/damage/teleport emission evaluates at a fixed 60 Hz cadence,
using the original quality/distance limits. This deliberately prevents the old
minimum-one-particle-per-present rule from multiplying density at high refresh.
Actual IGC boost runs produced 242/240/240 live sprites at 60/165/240 Hz. The
previous overly bright plume omitted original RGB/alpha fades. Damage retains
ten authored impact emitters. Teleport sparks follow ripcord/aleph approach;
mining uses the original subdivided lightning shape with a screen-width quad
backend. Trails now use the original half-second sections, continuously attached
head, eight-second lifetime and fading at both vertices of a segment.

The original damage-slot loop could write index ten of a ten-slot array, and
`ParticleGeoImpl` did not initialize its circular-buffer index. Both are fixed.
Zero-size exhaust, coincident damage positions and zero-count teleport emission
are guarded explicitly. These are intentional correctness fixes.

Construction consumes the original `build` mesh and IGC's existing envelope,
growth, opacity and shrink callbacks. Its complementary inner shell uses alpha
blending and its outer shell additive blending, with shared original rotations.
The Vulkan model pass adds a reusable transparent-instance scratch buffer and a
single sorted indirect-draw batch; the ordinary opaque path remains one batch.
Transparency between model instances and separate particle batches still needs
full material-level review under Q11.

The fourteen tests pass normally and under ASan/LSan. `native-effects` compares
288 appearance cases and emitted trajectories with frozen original excerpts,
plus bounded buffers and real IGC cadence cases. `native-economy` now observes
mining lightning and both construction shells during original production/build
progress. A 2,200-frame boost run and a 3,600-frame actual-constructor rendering
run each held 165 Hz with zero pacing misses and no Vulkan validation errors;
both final captures were inspected. The latter validates the transparency pass.
Windows build/performance remains untested.

## Subsequent execution: shield, aleph and surface effects

`ShieldFlare.h` preserves the original local/elliptical hit orientation. Native
loads all three authored `lens.mdl` exports and animates `fx09` over the original
0.75-second, four-live-hit limit. Hits follow the source hull without applying its
artwork half-turn to the lens. Model instances retain their 96-byte GPU layout;
the existing spare fields carry atlas coordinates.

`AlephEffects.h` supplies the original funnel and four detail levels (8×32,
4×16, 2×16, 2×8), scrolling UVs, twin rotating star and warp-bomb swelling. Native
uploads each static mesh once with its authored warp texture; UV motion executes
in the vertex shader. The original half-turn is restored. Both bomb endpoints
receive the warning on the mission clock in native builds; the old wall-clock
subtraction diverged during pause/fast-forward. Windows retains its clock path.
The target preview now carries the model's unlit/additive animation state too.

`ExplosionEffects.h` shares the original type-specific radii, counts, colors,
image sequences, spherical scatter, quarter-second staggering and 20-fps strips.
Native shockwaves use the destroyed hull's forward/right plane, not a billboard.
The original projected-fill budget of two is retained, processing burst ordinals
across events with counting sort into fixed scratch storage, then retiring later
bursts. Native ship/station deaths use the connected client's fighter/base
classification; the duplicate station explosion in the original offline adapter
is not reproduced. Model explosion sound selection uses the original doubled
radius. Sector pulse geometry and muzzle flashes are commented out in the
original WinTrek path and are intentionally inactive.

`SurfaceEffects.h` shares mine-field density/strength/brightness and hull-impact
geometry. Mines retain their individual positions as their strength declines;
source-alpha quads write depth before the additive batch. Transparent texels do
not occlude other effects. Hull impacts have the original tangent and normal
surfaces, growth, short lifetime and position relative to the moving hull.
Native impact storage is bounded at 64 per visible model; extreme excess replaces
the oldest entry. The original negative-Z/zero-normal degeneracy is corrected.

The native particle arena now retires its oldest slot on saturation instead of
dropping a newly emitted effect. Dense storage has insertion-order links so
expiry, replacement and iteration need no frame allocation or linear search for
an eviction victim. Quality changes preserve the fixed allocation. The audit also
caught that original `Color * scalar` preserves alpha: trail, shockwave and outer
construction fades now scale RGB only, avoiding an unintended second fade.

`native-effects` covers actual shield-to-hull combat, flare retirement, original
mine deployment/density, all aleph meshes, both bomb endpoints, seven explosion
profiles, emission timing/texture choice, oriented shockwaves, projected-fill
retirement, and saturated/interleaved particle cleanup. A 1,800-frame aleph run
on Wayland at 165 Hz completed with no misses or Vulkan validation errors and
its world capture was inspected. GPU combat and final sanitizer results are
recorded below after the final checks. Windows build/performance is untested.

The fourteen checks passed under ASan/LSan after these effects changes. The actual
GPU combat smoke reaches mine deployment, curved shields, surface hits and a
ship explosion (rather than injecting renderer-only test sprites). It exposed a
repeatable 2.8-ms CPU frame at shield failure. Phase tracing isolated audio:
preloading omitted shield activate/deactivate definitions and several core event
sounds. Those, plus development completion sounds, are now prepared at session
start. `native-audio` runs real shield-loss/hull combat and asserts that the decoded
wave count never grows after preload. A subsequent 1,933-frame Wayland combat
run held 165 Hz with zero misses and no Vulkan validation errors. The diagnostic
phase instrumentation was then removed; the event-driven GPU smoke remains.

## Subsequent execution: cargo controls and saved loadouts

`Inventory.h` now shares WinTrek cargo traversal, canonical stack selection,
equipment-type search, original mount swaps and manual reload state gates.
Native exposes cargo selection, swaps/unmounting, manual reload and dropping
actual IGC treasure through the flight HUD, B menu and rebindable R/G/Delete
controls. Swaps retain original arming delays. Passenger actions are rejected.
The controls screen paginates all bindings; F1 lists the additional actions.
Menu actions immediately drain their prewarmed sound events.

`native-actions` compares 51,200 traversal/selection cases with frozen original
bodies, plus swap mutations and 32 reload state combinations. Integration checks
exercise actual weapons/reserves, duplicate stack skipping, treasure drops and
passenger restrictions. The fourteen checks passed normally and under ASan/LSan.
A 1,000-frame Wayland cargo-menu run held 165 Hz with zero misses and no Vulkan
validation errors, and its capture was inspected.

`Loadout.h` shares BaseClient's successor/similar-part purchase order and defaults
for upgraded hulls. Native hangars now save, fit and clear ten banks, preserving
per-hull history, reverse insertion order for equal mounts, and current-hull or
most-recent selection. Persistence uses the original `loadouts_<core>.dat` format
with explicit little-endian 32-bit counts, 16-bit IDs and 8-bit mounts beside the
selected configuration file. Core namespaces stay separate. Loading validates
bounds/truncation before publishing a bank; fitting validates duplicate/illegal
slots before changing the ship. Missing part IDs are skipped without losing file
alignment. Atomic writes keep the previous file if saving fails.

Native refits trade the current equipment, honor station technology, budget and
launcher quantities, and fill upgraded hull defaults. An unavailable or
unaffordable saved hull falls back to the original first affordable default.
Native checks the successor hull's actual price to prevent a negative budget;
Windows retains its existing hull selection. Practice still supplies free gear.
All storage and selection work occurs on explicit hangar actions, never per frame.

The new `native-loadouts` check compares 124 purchases with frozen original code,
including real IGC fixtures for paid equipment, insufficient funds, partial
launcher amounts, successor upgrades and similar-part fallback. It also checks
exact Windows file bytes, every truncated prefix, malformed records, repeated
saves, cross-session restoration, launch, and rejected flight/duplicate-slot
refits. Installed static-core starter equipment is free; priced fixtures ensure
that those tests do not accidentally bypass spending behavior.

Connected FIRE_MISSILE, FIRE_EXPENDABLE, RELOAD, BUY_LOADOUT and SWAP_PART message
blocks still match HEAD byte for byte. Windows build/performance remains untested.

## Subsequent execution: native historical visibility

The native visibility integration check now drives a live Conquest mission, not
only a test ThingSite. It verifies that losing sight before a station upgrade
preserves the old model, icon, radius, name, hull/shield and target preview across
HUD contacts, world draws and command choices. Reacquisition updates that record;
public capture/destruction updates ownership and removes obsolete targets/draws.

A destroyed asteroid retained as historical intel now remains selectable. Its
preview uses the captured model and orientation; navigation uses an original IGC
waypoint without recreating a physical asteroid or exposing the hidden station.
Existing selection survives unseen construction. Scanning the old asteroid site
or discovering the new station removes the remembered contact, geometry and
target. The integration test exercises both discovery paths. This work also
caught a terminal-state early return that skipped queued object cleanup and HUD
refresh after a completion event; final-state stepping now settles those changes.

The Game adapter exposes original IGC mission/player services for training and
local authority integration. It introduces no replacement simulation and changes
no Windows frame path. The loadout GPU smoke completes save/change/refit with
Vulkan validation clean and its capture inspected. Its hull-change frame takes
about 4.8 ms and misses one 165-Hz deadline; explicit menu-time asset preparation
remains a measured transition cost, not a claim of zero-miss gameplay.

## Subsequent execution: mission goals and carrier policy

Conquest now exposes conquest/territory percentages, flags, artifacts, team kills,
prosperity and time limits, plus initial money, He3, miners and friendly fire.
Original `EnterGame` creates prosperity buckets and initializes map objects.
The original map generator supplies flag pedestals and artifacts. Native pickup,
ejection, delivery, respawn, sounds, carried-objective HUD and team scores use IGC
and shared policy. `DataStationIGC`/`DataTreasureIGC` initialization in the original
pedestal helper now initializes previously indeterminate fields.

`MissionRules.h` shares FedSrv's timeout ordering and tie-breaks and kill goal
threshold. Tests compare 69 frozen timeout cases and drive actual objective
collisions, pod recovery, station docking, repeated scoring, prosperity production,
kill completion and elapsed mission time. Completed sessions retain their results
and scoreboard without offering flight/launch actions or resetting the menu each
frame. Windows timeout calls remain under their original guards.

`CarrierLoadout.h` shares the original free-equipment/technology rearm policy from
FedSrv and cached-equipment restoration from BaseClient. These are deliberately
different policies: Conquest preserves paid quantities and replaces depleted free
launchers; offline practice can restore lost cached guns as well. The original
loadout traversal/order is retained with inline visitor adapters; Windows uses its
existing linked cache and stack arrays, with no added frame work or allocations.
The offline cargo search now stops at mount zero instead of querying past cargo.

`native-carrier` compares 512 frozen server/offline cases, including paid quantities,
missing launchers, duplicate reserves on changed mounts, free successor upgrades,
unavailable technology, player/drone cargo policies, and original part insertion
order. `native-lifecycle` flies into actual authored carrier garages in practice
and Conquest, then verifies equipment policy, resource replenishment, original
relaunch geometry, defend orders and clearance of the carrier collision hull.
Further terminal/team lifecycle and full mission configuration remain open.

## Subsequent execution: weapon indicators and radar

Original `indicator.cpp` weapon allegiance/relay checks and aiming color/size math
are shared in `WeaponIndicator.h` and `AimIndicator.h`. Native no longer grants
lead for every weapon/target, keeps stale range after target/weapon loss, or projects
cockpit lead from a fictitious point displaced from the camera. Chase and missile
views use the original aiming point and color policy. The station/ship relay scans
stay in their original WinTrek branch; extraction adds no frame allocation or new
Windows traversal.

`RadarModel.h` and `RadarPresentation.h` share original treasure usefulness and
object-specific icon/label/statistics/range rules. Windows metadata is queried
lazily in the same branches. Native consumes plain snapshot fields, retains
historical station capabilities, preloads core icons, draws expanding four-corner
selection/lock brackets and reads threat/flash from the local victim's damage
track. A read-only DamageBucket track getter distinguishes native local-authority
histories without changing IGC layout or the Windows damage path.

`native-indicator` compares frozen relay and weapon policy, 122,880 radar cases,
and live IGC allegiance, camera changes, weapon loss, treasure usefulness and
threat ownership. All 19 native and sanitizer checks passed, including the final threat
ownership extension.
Vulkan combat/HUD validation is clean; the inspected 1,100-frame run missed one
165-Hz deadline during an explicit resize/font rebuild, with no steady-flight miss.
Original training, complete input/HUD/commander flow and the rest of Q07 remain open.


## Subsequent execution: player commands and targeting

`PlayerCommands.h` shares WinTrek's current-target/default-order resolution,
queued-order acceptance/execute/join/rescue transitions and target-speed throttle.
Native previously overwrote accepted commands whenever a target was selected.
It now preserves the original queued, accepted, current and plan slots. Incoming
orders expire after the original 15 seconds; local pilots retain their own pending
orders when control changes. Commander orders to self are accepted/targeted and
executed as in WinTrek; orders to another pilot await acceptance, while drones keep
the server order path. Commands have an in-flight prompt, configurable shortcuts,
and a pilot command menu. Both acceptance and expiry are tested against real IGC.

`Targeting.h` extracts the original category/capability/cycling search and alliance,
rescue, reticle, hostile-damage, remote-sector and viewed-cluster branches.
WinTrek calls the shared selection policy; native no longer allocates/sorts a
custom list for target cycling. `Igc/TrekKeys.h` retains the original identifiers
unchanged and lets native input name them without including the IGC interfaces.
All target searches are configurable, including role-specific nearest/previous
variants; existing saved binding indices remain stable. Defaults add comma for
nearest similar target, period for previous, L for reticle and F12 for hostile.

`native-commands` checks 96 frozen original queued-order combinations and live
accept/execute/reject/join/expiry, command separation, local pilot routing and
match-speed behavior. `native-targeting` checks 41,138 frozen original results
(26,736 actual selections), with hidden models, alliances, gunner/observer views,
rescue/capital hulls and independent/docked/remote views. All 21 normal and
ASan/LSan checks passed. The pilot command menu capture was inspected; Vulkan
validation was clean and its 180-frame run missed no 165-Hz presentation deadlines.
Full controller/commander/HUD coverage and original narrated training remain open.


## Subsequent execution: weapon selection and controller input

`WeaponSelection.h` shares original BaseClient next/previous/valid-mount selection
and WinTrek's gun/group toggle. Native now retains the selected gun when grouping,
uses that gun for lead, rejects empty mounts and exposes next/previous bindings.
Weapon cycling is bounded even after a hull change leaves an invalid old mount;
valid original states retain the original traversal/result. `EquipmentStatus.h`
shares PartWrapper's mounting/arming/ready states and original launcher arming
curve, with native IGC time and original Windows wall-clock selection retained.
Chaff and dispensers now display their arming progress too. 50,364 frozen
weapon/readiness comparisons and live firing/group/launcher checks passed; all
22 normal and ASan/LSan tests passed after that batch.

SDL joystick/gamepad support now polls a retained device after the frame's event
pump. Enumeration, GUID strings and device-list allocation occur on configuration
or hotplug only. It supports configurable yaw/pitch/roll/throttle axes and
inversion, original linear/quadratic dead-zone response, hats/d-pad look, gamepad
triggers, held controls and edge-triggered actions. Focus/reset suppresses held
buttons until release; a throttle must move before taking control and keyboard
throttle/match-speed overrides it until the next physical movement. Menus expose
device selection, axes, inversion, response, dead zone, hat and all supported
button assignments. Controller and keyboard actions share native dispatch.

`native-controller` drives actual SDL virtual flight sticks and gamepads through
plug/unplug, focus, throttle takeover, endpoints, hats, trigger/button remapping,
and menu events. It compares 655,360 samples to original WinTrek MapJoystick.
The settings round-trip/menu tests include controller edits, pagination and
return/apply behavior. All 23 normal and ASan/LSan tests passed. The controller
screen capture was inspected and its Vulkan validation run was clean. No physical
controller was connected during validation. Multiple simultaneous input devices,
force feedback and the remaining original logical keys are still under audit.

All three guarded WinTrek ORDER_CHANGE packet blocks remain byte-identical to
HEAD. No Windows build or timing measurement has been performed on this machine.


The input audit also replaced continuous native W/S throttling with original
10% steps, original repeat delays and original zero/one-third/two-thirds/full
values. 20,000 frozen input sequences cover repeat timing, held/released keys and
clamps. Native presets are 5–8 and repeat delay is configurable. Periodic practice
spawning now counts/orders only drones and leaves both inactive local pilots and
an explicitly cleared player target alone; a live regression check runs through
the eight-second spawn maintenance interval. These were subsequently audited and implemented as described below.


## Subsequent execution: controls, original training, and command view

The native input path now shares `ClientCore/FlightControls.h` with WinTrek:
original autopilot dead-zone comparisons against the captured entry axes,
manual takeover/button edges, engine-vector throttle fallback, client control
mask preservation, weapon/expendable bits and cockpit FOV slew limits. Coast
and cloak are persistent IGC toggles. Inactive UI input does not cancel
navigation. Physical throttle resumes only when appropriate; authored training
throttle overrides remain authoritative. Controller curve comparisons now cover
1,310,720 samples, including both denominator signs in original MapJoystick.

All 87 authored training implementation files compile in `allegiance_training`.
`TrainingHost` adapts the original actions to real IGC, native input/presentation,
loadouts and original SoundEngine/OpenAL instances. The simulation clock supplies
training time and pause. All seven available original missions (2, 3, 4, 5, 6,
8, 10) load their original worlds, ship/team setup, narration and constraints.
Repeated mission startup checks include returning from mission 8's altered
civilizations to the ordinary training civilizations.

`native-training` now completes the entire flight lesson through authored
narration, throttle/booster exercises, autopilot and actual base collision. It
also completes the commander tutorial through selection, waypoint tower orders,
sector change, helium mining, retreat, AI combat and Team Display / Take Me To.
These tests do not skip goals or substitute a new tutorial script. Other missions
currently have startup/clock/narration/teardown checks; their complete player
walkthroughs remain open. Death/restore, station-loss termination, duplicate
termination, input restrictions and runtime audio-decode checks are covered.

The audits found ownership defects in original training: unbalanced CreateObject
references, unused actions, unexecuted respawn registrations and a leaking tech
mask string. These are corrected. Station loss now retires its current action
tree between evaluations; key/chat subscriptions cannot outlive deleted
conditions. Termination is idempotent. Collision death with a null or coincident
launcher has a finite fallback direction. Training pauses both OpenAL playback
and original sound-template timers, rather than merely muting them.

Native command view supplies object selection, legal IGC orders, provisional
waypoint ownership, sector-map selection, Team Display / Take Me To, plane
panning, zoom, roll, top-down orientation and tracking. Shared camera helpers
retain original plane movement and displacement bounds; 7,744 frozen numerical
comparisons cover the two orientations and all roll/pan combinations. The
original training policy still blocks actions where appropriate. Native contact
labels use bounded, stack-resident occupancy masks with priority for selected,
ordered and threatening objects; they avoid gauges and adjacent labels.
Rectangle/multiple selection and command altitude feedback are implemented in the subsequent batch below. The remaining commander/roster presentation is still under audit.

Training introduction/debrief MDL is consumed as static data: image groups,
translations, text, and the original sound IDs. It does not instantiate the
legacy UI object hierarchy. The sound graph and slide data share a bounded text
parser. Slides retain original order and narration, advance one second after
sound completion, and support click/key advancement and skipping. All 62 slide
entries across the seven available mission choices load with their installed
images and narration; no sound decoding occurs during playback. Text uses native
font rendering and proportional layout. The menu runs introductions before the
mission and debriefs after natural completion. Direct `--training N` remains a
simulation-start diagnostic; `--training-intro` includes its introduction.

## Subsequent execution: uncached offline carrier loadouts

`ClientCore/CarrierLoadout.h` now also shares the original no-cache fallback with
BaseClient and native Game: select the first landing station, budget the cargo
trade-in for the existing default-purchase transaction, or provision fuel/ammo
reserves if no station remains, then refill amounts. The original cached and
server policies are unchanged. A full-cargo bounds defect is intentionally
corrected so cargo traversal cannot create a pack at a non-cargo mount. Tests
cover 577 original-reference cases plus full-cargo saturation. The original
fallback's fuel-count policy is retained, including its treatment of newly
added fuel packs.

Validation at this checkpoint: all 25 normal tests pass. The controls/training/
command-view batch also passed all 25 ASan/UBSan/LSan tests; carrier fallback, slide integration and later combat/quick-chat changes subsequently passed the full sanitizer suite. A Wayland command-view
capture with Vulkan validation ran 1,200 frames at 165 Hz, with zero recorded
misses and a final sample-to-present estimate of 2.782 ms. This is a short local
run, not a latency guarantee or a Windows performance measurement. No Windows
build has been performed. Networking and the other open queue items remain open.


## Subsequent execution: original combat lessons and quick chat

Full input-driven replays now finish missions 2, 3, 4 and 5 without skipping
original goals. Mission 8 runs through reserve nanite mounting, real station
repair, miner defense, bomber escort and aleph transit to its authored bomber-loss
debrief. A successful bomber-defense outcome remains open. Arena/free-flight wave progression is covered by the subsequent damage-event replay; a full input-driven arena victory remains unverified. Mission 3 also exercises death/restore before completing the lesson.

This exposed original defects: an exactly aligned pointing test could take acos
of a value slightly above one; death recovery evaluated an unstarted timer;
modern default cargo left the weapons lesson without its assumed missile reserve;
and the engagement lesson waited for an accepted constructor waypoint after
construction had already removed that target. These are corrected in the shared
training source. Original AI pursuit waypoints also needed their creation
references released. These are event-path
fixes; they add no WinTrek frame-loop allocations.

Offline station construction now shares the original WinTrek placement/type
policy through `ClientCore/MissionEconomy.h`. The native training adapter no
longer substitutes the server construction policy. Native training deaths remove
nonlocal authored targets, including fixtures marked as player ships, while
preserving the original local pilot's death/restore behavior.

Native typed chat, recipient cycling and quick chat feed the original training
conditions. `quickchat.mdl` supplies all 13 menus and 307 commands, with original
accelerators, text, sounds and target predicates. `ClientCore/QuickChat.h` shares
WinTrek's target resolution; 921 frozen comparisons cover the installed commands.
The installed catalog uses one voice per command. Dynamic voice IDs are native
resources, not protocol identifiers. One installed voice (`vo_ma_woopie`) is
missing; its text/command remains available with the normal chat notification.
The catalog's accepted/queued target-mask constants disagree with the IGC names;
raw artwork values are currently preserved, matching WinTrek interpretation.

Tutorial text resolves configured controls when posted, including cargo weapon
swaps and execute-order bindings. Original recorded narration still speaks its
historical controls. Quick chat is on grave/tilde; cursor capture toggles on
backslash. Successful static-menu Vulkan validation is recorded separately from
flight performance; it does not establish flight latency.


## Sector visuals, commander groups and terminal pilots

`CommandPicking.h` shares the original rectangle threshold/radius test, command
height calculation and grid spans. Native rectangle selection, Shift-click group
editing, group orders through a shared waypoint, height feedback, north marker,
grid and object drop lines are wired. Height picking has 3,888 frozen comparisons.
Selection uses a fixed membership bitset; event paths own the group vector.
Contact label placement also reserves other markers and stat bars before labels.

`ModelLod.h` shares the strict original projected-radius selection. The native
loader retains each lower-detail mesh and the renderer skips it outside its
interval. `Strobe.h` shares the authored flash envelope and includes a bounded
period advance; tests cover 648,000 envelope cases. Original model transforms,
physical scaling and the 32-pixel strobe cutoff feed the existing sprite batch.

The asset inventory covers both installed cores, four authored maps, four
supported generated maps, all eight generated skies and all 25 mapmaker planet
entries. It loads 383 model references and 185 image references, comprising
1,042 meshes, 480,718 triangles across all LODs, 3,624 strobes and 641 meshes with
LOD intervals, with zero failures. The loaded functions are MeshGeo, TextureGeo,
GroupGeo, LODGeo, LightsGeo, FrameData, ImportImage and ModifiableNumber.

Sector primary/alternate colors, light direction and bidirectional ambient now
feed original vertex-lighting semantics. Backdrops consume the authored globe
meshes; planets use the original direction, 400-unit distance and scale.
`StarField.h` retains original folded projection and pixel-motion streaks for
WinTrek. The native perspective correction is recorded below. Native stars use a private Windows CRT sequence for authored seeds,
without reseeding simulation randomness. Original lens-flare positions, colors,
scale, rotation and fixed +Z direction are retained. Rendering orders environment,
stars, posters and flares before world geometry, with no backdrop depth writes.
Both HUD layers share one fenced instance arena; the frame path decodes no art.

The original IGC `IsGhost()` threshold now governs exhausted pilots. They remain
pods in command view, retain their IDs and cannot launch, reset, refit, restore a
saved loadout or board. A surviving teammate keeps the team active; loss of the
last live Conquest pilot deactivates the side and applies the original survivor
rule. `MissionHasPlayers` shares FedSrv's player/ghost count without changing its
player classification. Mission rules expose the number of deaths allowed, and
the HUD shows remaining lives when below ten.

Crewed allied station rearm now restores the appropriate expendables, repairs,
refills and relaunches the pilot and gunner together. Allied pod docking returns
to the last own station, with an own restart fallback if that station was lost
or captured. `ShipLifecycle.h` shares original rescue selection and boarding
permission with FedSrv, including allied boarding under friendly fire. No allied
base secures a carried flag or grants recovered equipment technology. These are
event paths; the WinTrek frame loop gains no scans or allocations.

All 26 normal and ASan/UBSan/LSan tests pass for this batch. A Vulkan validation
flight at 2560×1440 ran 1,000 frames at 165 Hz with no recorded misses. After the
first 200 reported frames, sample-to-present median/p95 were 3.777/3.825 ms.
The commander capture overlapped compilation and recorded one miss; it is not
an idle timing measurement. These runs precede the final vertex-lighting and
contact-placement refinements, which receive a fresh capture. Windows build and
performance measurements, full training outcomes, complete mission/team menus,
remaining presentation/state coverage and transport integration are still open.


## Subsequent execution: input, cinematic views, compositing and ambient sound

Keyboard and controller actions now include 153 configurable bindings. HUD panes,
radar detail, quick/typed chat, research, commander controls, camera modes, look
angles and zoom use the same configured actions as flight input. Existing saved
binding indices are preserved. Keys can be exchanged or cleared; cleared keys
survive configuration reload. F10 consistently targets friendlies and no longer
quits from an interface screen. Unbound keys no longer invoke hidden recovery,
movie or HUD actions. Escape remains the menu escape/cancel control. Training
messages follow the configured help and inventory keys as well as flight bindings.

Launch and carrier rearm use the original three-second tracking camera. Death
uses the original upright five-second frozen frame, retaining the pre-ejection
position/radius and target or backward fallback. The replacement pod remains the
same IGC pilot, but its model and controls are withheld during that death view.
Training death/restore uses the same frame and duration without replacing its
original mission lifecycle. The native adapter holds the viewed cluster only for
the override and releases it before mission teardown. Camera/audio selection now
follows the applied camera, including authored training and cinematic overrides.

Transparent model instances and additive particles are merged by depth when an
alpha mesh is visible. Stable ties retain model/particle order, and line sprites
use their endpoint midpoint. Opaque models and solid mine sprites retain depth
writes; translucent geometry and additive particles do not write depth. The common
pure-additive case keeps unsorted bindless batches. Scratch vectors are reserved
once and command ranges coalesce adjacent draws; no per-frame allocation or art
decoding is added. This is a Vulkan implementation, not the legacy callback graph.

`allegiance-render-check` is an explicit Wayland/GPU check, separate from headless
CTest. Actual readback pixels verify attenuation of a blue additive particle by a
nearer red alpha plane (188/5/188 behind versus 188/5/255 in front), two-layer
submission-order independence, line-center ordering, opaque occlusion and the
pure-additive fast path. Core and synchronization validation report no messages.

Hangar screens retain the station's original interior/alert ambience. Live flight
menus keep world audio; pause still pauses it. Incoming missiles targeting the
player's source ship share one warning loop, with pitch `0.75 + strongestLock/2`.
The shared `IncomingMissileWarning` fold replaces the identical WinTrek block in
its existing model traversal, adding no scan or allocation. Real IGC missile
creation/removal and station launch/dock tests verify warning selection, pitch,
stop, ambient transitions and absence of runtime waveform decoding.

Full authored missions 6 and 10 now complete all waves through actual IGC damage
and death events: 49 kills and 11 enemy hull variants each, respawn actions and the
final victory narration. This isolates lifecycle/wave coverage from flight skill;
it is not an input-driven arena victory. Input replays still cover missions 2–5
and mission 8's bomber-loss path. Repair success remains queued.

All 26 normal and ASan/UBSan/LSan checks passed for the new lifecycle, binding,
wave, ambient and compositing work. Before the final camera/audio refinements,
validated idle command and flight runs at 2560×1440/165 Hz recorded no misses:
600 command frames (median/p95 2.749/2.754 ms after the first 200) and 1,000 flight
frames (3.493/3.540 ms). Fresh final-build smoke/timing results follow below.

Still open: complete mission/team/roster flow, remaining HUD/input/terminal edge
coverage, repair success and input-driven arena walkthroughs, core ABI/time/RNG
and original-build state traces, full visual captures, and remaining audio/FMV
event coverage. Networking remains deliberately deferred. No Windows binary or
Windows performance run has been performed.

## Subsequent execution: core records, mission setup and player authority

Native static-core ABI checks cover 24 Windows i686/MSVC record layouts and 212
field offsets. Defined payload fields are normalized on core re-export; padding
is not treated as state. Compact-clock checks cover 458,752 rollover/delta cases.
The native simulation uses the Windows CRT seed/draw sequence within each Game
scope, preserving Windows's original RNG path. Frozen RNG comparisons and
interleaved real Conquest fingerprints exercise independent simulation streams.
These are native reference checks, not captures from an executing Windows client.
Dynamic records, time/authority synchronization and original-build replay traces
remain part of Q09/Q13.

All fifteen original map makers are exposed for two through six teams; original
validation excludes unsupported combinations. Mission rules include civilization
selection, resources, initial bases/miners, development paths, allied sight/ripcord,
objectives and original disabled-territory semantics. Per-team civilizations use
the selected pilot's team correctly. `Alliances.h` retains the original group
normalization/active-side/mutation policy: 143,360 frozen comparisons cover merged,
inactive, singleton and all-united groups. This is event work and adds no Windows
frame traversal or native-container conversion.

`PlayerEconomy.h` shares original donation resolution, payday integer remainders,
requests, commander voting and departing-player redirection. Native accounts
replace one shared wallet; hull/part transactions debit the actual pilot. Allied
rosters conceal balances and undiscovered locations. Departing pilots return
wallet/ship value to the side and transfer incoming donations to the replacement
leader. Initial local seats are real IGC pilots at original side stations before
the initial payday. The native lobby supports seat names, teams, readiness,
removal, cancellation and a configurable start countdown. `LobbyReady.h` preserves
minimum-player/force-ready short circuits at the original FedSrv event call site;
32,768 comparisons check its outputs and number/order of readiness queries.
Local control switching is explicitly an offline authority facility.

## Subsequent execution: preload, geometry, training and platform feedback

Original `IIgcSite::Preload` now warms static-core model, skin and effect resources
before gameplay. Vulkan skin variants reference one immutable mesh upload and
only vary material texture descriptors; retaining mesh owners makes pointer-keyed
geometry caches safe. GPU readback checks prove geometry counts remain unchanged
while a skin variant renders its own texture. This changes the native load path,
not WinTrek's renderer or per-frame work.

The translated training hangar now exposes the authored station hull/part catalog,
its technology and original saved-loadout purchases with free offline equipment.
Real return-to-base flight, collision-driven docking, refit and launch checks run
in missions 6 and 10. Mission 8's second replay conserves repair energy until the
bomber takes damage; genuine steering and nanite fire escort it through the
aleph/defenders to enemy-outpost destruction and victory narration. Missions 6
and 10 still have complete 49-kill/11-hull wave coverage through actual damage
and death events. Their optional automated combat pilot has not achieved an
input-driven victory and remains an open coverage gate.

That longer arena pilot found an original training crash: a projectile retains
its terminated launcher, whose collision state has already been released.
`TrainingMission::ShipDied` now checks the launcher's mission before position/radius
access and uses existing no-launcher framing if absent. A retained terminated
attacker regression exercises death/restore. The shared Windows correction adds
one validity check at death, with no new frame work.

Native force-feedback callbacks now retain original IDs, controlled-model filtering,
actual gun/missile/expendable triggers and local impact direction. A fixed three-bit
queue coalesces events. All nine installed RIFF FORC presets translate their
32-bit serialized fields to SDL waveforms and conditions, including gain, axes,
envelopes, phases and delayed starts. Device setup creates effects once; unsupported
banks use original fallback profiles or prioritized SDL rumble. The original bounce
switch's explosion-bank overlap is retained on force-feedback devices. Virtual
rumble verifies routing, priority, no idle updates, stop and disable; preset checks
verify known authored values and reject every truncation. Physical force-feedback
hardware remains unverified. Haptic subsystem failure is optional and cannot block
window/input startup. WinTrek's haptic backend is unchanged.

FFmpeg movie playback now freezes audio/video time on focus pause, holds the last
video frame for its authored duration, drains PCM on natural completion and wakes
bounded decode queues on repeated skip/teardown. Original Microsoft-intro PCM,
pause/resume, completion and repeated main-intro cancellation are tested through
OpenAL loopback. World voices pause while independent movie audio stays audible
under the master volume. Headless audio/movie checks select the null device so
extension probing cannot block on unavailable PulseAudio.

Remaining acceptance work: remaining HUD/input and crew/terminal combinations,
original-build state/event traces and complete packet/pointer ABI coverage, full cross-client
visual captures and remaining audio event coverage, multi-device/physical
haptic/VRR testing, and the full input arena walkthrough.
Remote joining, packets and transport remain deliberately deferred. No Windows
binary build or Windows performance run has been performed.


## Current execution: dynamic records, preferences, input and station travel

`native-dynamic-records` pins nineteen original Windows i686/MSVC dynamic layouts
and 144 field offsets. It re-exports 265 real objects across `training_1`, `_3`,
`_5`, `_6` and `_nan`, comparing defined payload bytes and identities after the
original import normalization. Frozen orientation/warp normalization, cluster
inactive export and BytePercentage's divisor 240 explain authored bytes that
legitimately change on original import. Padding and unused C-string tails are
excluded; the secondary asteroid name is retained. Selective imports found an
original pointer-advance bug: skipping a type failed to advance past its payload.
The shared importer now advances on every record. Six-byte header size fields use
`memcpy`, preserving the format without an unaligned int access. This is not a
complete packet ABI audit, an ARM payload-alignment certification, or an executing
Windows replay.

`SoundIds.h` and `ClientSoundEvents.h` share the original sound identifiers,
countdown/chat choices, station capture/destruction announcements and first-contact
policy. Frozen tests compare 8,162 countdown/chat cases; real scanner reacquisition
and station events check routing and deduplication. Native SFX and voice controls
now wrap their original sound-graph category nodes. Changing settings updates
active wrappers at the settings event and queued voices use the latest gain when
they start. Nested graph attenuation is preserved. No new per-frame category
polling or waveform decoding is introduced. Original defaults are -8 dB effects
and -13 dB voice; master gain also controls FMV. SDL feedback strength scales
rumble and applies device gain during setup; unsupported physical devices remain
an unverified gate.

Settings now import supported original JSON preferences without modifying their
source. A missing ordinary native profile may migrate Steam's `config.json`;
custom profiles remain isolated. Explicit `--import-config` and
`--import-inputmap` select their sources. The keyboard adapter parses saved numeric
`keyCommandMap` records atomically, translating Windows virtual keys, Any/Shift/
Control/Alt modifiers and multiple aliases into a compiled SDL lookup. The
original's distinct held and pressed modifier policies are now preserved:
`Any` ignores modifiers for held controls but occupies only the unmodified
press table. See the October 2026 keymap audit below.
Lookup tables are compiled on startup/Apply; sampling allocates nothing. All ten
cargo swaps are configurable. Unsupported original actions remain excluded rather
than being assigned a different behavior. Keyboard tests cover 72 modifier/held/
press cases, lock modifiers, installed target/zoom aliases, persistence, legacy
native profiles and malformed imports. Legacy DInput joystick device/axis maps still need a separate adapter;
mouse button/wheel map migration is covered in the subsequent input batch.

The original startup preferences now drive Microsoft credits followed by the
introduction, with persisted one-shot intro clearing. Movie skip returns through
the sequence to Home. The actual 180-frame Wayland/Vulkan startup check passes with
both preferences enabled in an isolated profile; audio/video focus pause and
completion remain covered separately. Audio, radar, grid and appearance settings
retain their draft through Back/Cancel/Apply. Cockpit/chase and command radar
remember separate detail levels; the original cycle is All→Default→Target→Minimal.
Appearance settings preserve original sky/poster/star/light/trail choices and the
single/bidirectional lighting ambient levels. Command view suppresses lens flare.
These are native presentation branches; WinTrek's renderer and haptic backend are
unchanged.

`StationTeleport.h` replaces the original FedSrv `FM_C_DOCKED` policy block and
serves local authority. Destination authorization requires an own/allied restart
station. A retained hull carries its crew; an unavailable hull releases children
at the origin, refunds value before equipment removal, changes to the civilization
pod and moves the pilot. Native cached/default loadout purchasing follows that
refund. A passenger may disembark and travel independently; an exhausted-life
ghost retains observation and cannot refit or launch. Authored lessons keep their
own station flow. The hangar exposes paginated destination choices; F8 toggles
teleport, and Shift+F8 opens the event log with the new default controls.

`native-teleport` compares 72 original authorization combinations and rejects a
non-restart station. Real integration cases cover own/allied/revoked/enemy travel,
retained identity/equipment/wallet, unavailable-hull replacement and credit
conservation, crew callback order, independent passengers, ghost travel and flight
refusal. The 500-frame validated Wayland check uses the original Split Base map,
selects a destination through mouse hit testing, checks station IDs (two bases can
share a name), toggles the destination list through SDL keyboard events and saves
a screenshot through Print Screen. Destination and hangar captures were inspected.
The normal and address/leak sanitizer suites pass all 35 checks at this checkpoint.
Additional appearance and combat-walkthrough work follows; no Windows build or
performance measurement is inferred.

The remaining default-keyboard rows are transport votes/game-state display,
legacy renderer LOD controls, inactive finish/skip commands and debug-only training
clock controls. Modern resolution-independent UI architecture remains intentional;
original renderer LOD sliders are not adopted. Native message paging retains a
bounded 512-message history, keeps a page anchored under new arrivals/eviction,
and restores newest messages after ten seconds. Grid visibility shares the original
preference/camera-transition predicate with WinTrek. The screenshot shortcut reads
back only the requested frame; it creates no normal frame-loop GPU readback.


## Current execution: appearance and original mouse/keyboard steering

The ten original appearance switches now control actual native output: skies,
planet posters, stars, ship strobes/trails, lens flare, single/bidirectional
lighting, center instruments, target instruments and message history. World
checks disable sky and posters independently without removing world geometry,
restore the same cached resources, check original star counts/seeds and ambient
levels, and hide retained trail ribbons immediately without destroying their
particle state. Disabling trails also stops new trail emission; re-enabling
resets endpoints. Lens flare is suppressed in command view, following the
original camera policy. Settings draft/back/apply and JSON migration are tested.

The input audit found that native main-loop arrow pitch signs were opposite to
WinTrek::SenseJoystick. `KeyboardSteering.h` now serves both call sites, preserving
original priority, roll-modifier behavior and short-circuit key-query order.
`MouseJoystick.h` shares MouseInputStreamImpl's acceleration/sensitivity and
JoystickImageImpl's accumulated circularly clamped steering. Windows retains its
own system acceleration thresholds and cursor/event adapter; no new objects,
allocation or list traversal are introduced there. Native Wayland motion selects
direct movement or accumulated virtual joystick steering. Virtual steering
starts centered, discards the first recenter event, holds turn rate while the
mouse is stationary, and clears on leaving flight. Global inversion precedes
keyboard overrides as in the original. Native low/high acceleration uses explicit
6/10-pixel thresholds; it does not claim to migrate Windows SPI mouse settings.

The steering indicator uses actual approved/FOV-limited IGC axes. Its original
position/strength semantics are presented with a scalable vector arrow instead
of the original 3D pointer mesh. `native-controls` compares 28,888 frozen original
keyboard/mouse/acceleration cases, including conflicting keys and exact query
order, and tests enable/disable/resume behavior and the real IGC snapshot.
`native-sessions` checks independent radar, grid, mouse and appearance drafts,
persistence and all 29 supported JSON preferences. Direct native steering remains
the default; importing original `VirtualJoystick` can select the original mode.
Further physical-device and executing-Windows evidence remains open.


## Current execution: mouse maps, zoom and training endings

The original `mouseButtonCommandMap` now migrates read-only with the keyboard map.
SDL middle/right indices are translated to the original logical order. Native
settings retain up to eight physical buttons, wheel directions and multiple
logical aliases; an invalid mouse row rolls back the whole keyboard/mouse import.
A missing mouse section preserves existing native assignments. The installed
default supplies five mouse rows in addition to 92 supported keyboard rows. It
uses guns on left, center targeting on right, missiles on middle and throttle on
the wheel. Custom maps may assign weapon cycling, camera zoom or other actions.
Joystick axis/device maps remain open and are not silently flattened.

`MouseBindings` compiles alias bitsets on startup/Apply. Held sampling uses eight
bounded physical-button checks without allocating or parsing settings. Menu and
focus transitions suppress buttons until release. Fractional wheel motion is
accumulated into detents; flipped direction and pulse actions use the same domain
input adapter. Command selection and menu hit testing retain their own pointer
controls. A new assignment screen preserves the draft through Back/Cancel/Apply.

The zoom audit found two native gaps: cockpit keys changed an unused distance,
and external zoom used a constant distance rate. `CameraZoom.h` now shares the
original cockpit-radian and proportional distance equations with ten WinTrek
call sites. Original key-query priority, Windows bounds and wheel interval stay
in their adapters. No new Windows allocations, traversals or virtual calls are
introduced. Native cockpit zoom returns to its configured wide FOV; command wheel
zoom uses the original fixed 0.1-second interval instead of multiplying by the
current frame duration. Frozen comparisons cover 6,030 FOV/external/command cases;
real camera checks verify projection/IGC view FOV and command detent geometry.

Training end-state handling no longer treats a debrief as success. Original
mission 6/10 deaths intentionally terminate after the five-second death camera.
The original destructor requests post-slides for both completion and failure; a
separate native flag carries that policy independently of result text. Real
damage tests verify both arena death endings, 49-kill event-driven endings and
explicit skip-post exit. Original mission difficulty and respawn behavior are
retained. Input-driven 49-kill arena victories remain an open coverage gate; the
scripted test pilot has not demonstrated one.

After the mouse-map/zoom/training-state changes, all 35 normal tests and all 35
address/leak sanitizer tests pass. The 1,450-frame validated Wayland mouse check
passes original center targeting, fractional wheel throttle, actual middle-button
missile launch, configured weapon cycle/cockpit zoom and command detent zoom. Its
configuration capture was inspected. One presentation miss occurred during this
interactive transition test; it is not an isolated flight pacing measurement.
The subsequent 1,120-frame validated Wayland check passes arena death, the five-second camera, debrief skip, returning to the training list, selecting the next original lesson through mouse input, skipping its intro and explicitly leaving back to training. Its menu capture was inspected; two misses occurred during those transitions. The final frontend exit edit also passed focused normal and address/leak session checks.

The preceding installed 3,000-frame validated flight held 165 Hz with zero misses.
After 500 warm-up records, sample-to-present median/p95/p99 were
3.289/4.309/4.349 ms and sample-to-submit 0.225/0.276/0.384 ms (2,498 feedback
samples). The installed executable matched the build, all six shader loads came
from its installation prefix, and its desktop entry validated. Actual Vulkan
pixels also pass skin/geometry reuse, alpha/additive interleaving, stable
line-center/depth order, opaque occlusion and the pure-additive fast path. These
application/presentation timestamps do not measure input-to-photon latency or
WinTrek performance. The latest frontend-only exit changes follow this checkpoint.


## Current execution: controller routes, chat recipients and terminal authority

Saved `buttonCommandMap` and both `numericValuesNew`/`numericValues` formats now
migrate atomically with keyboard and mouse mappings. The old axis format retains
its original 2/3 swap. Negative missing sources, including the installed default's
uninitialized negative sixth row, remain inert. The installed map imports 104
supported rows: 92 keyboard, five mouse, two controller buttons and five axes.
Unsupported transport/debug keys remain outside the native action set.

Native configuration exposes six independent axis/POV routes, device-slot identity
assignments, physical button aliases and four original command-hat directions.
It supports 16 devices, 64 physical buttons per device and 512 button routes. Saved
Windows value numbers remain available for explicit remapping: DirectInput driver
value order and Windows device enumeration are not guaranteed to match raw SDL
indices. Import provides a reviewable initial X/Y/Z/Rz/POV mapping, not a hardware
identity certificate. The original single-gamepad mode remains available.

Controller handles and deduplicated action bitsets compile only on Apply/hotplug.
Sampling reads six routes and distinct assigned buttons, without allocations or
settings searches. Eight-way POV values use an exact constant table rather than
per-frame trigonometry. Duplicate GUIDs retain separate live keys when another
controller disconnects. No-device, focus suppression, throttle takeover, route
removal and switching between raw mapped and standard gamepad modes are covered.
Virtual sticks exercise simultaneous axes, throttle, independent look/command
hats, aliases, diagonal tie-breaks and removal of either duplicate device.
`JoystickHat.h` shares the original scalar threshold chain with WinTrek; 8,193
frozen values verify exact boundaries. Windows still owns its existing four
boolean objects and update loop; the extraction adds no scan or allocation.
Physical axis order, forces and device identity after restart remain acceptance
checks on actual hardware.

Typed and quick chat now expose allied teams, friendly sector, ship crew and
commanders in addition to the original native five recipients. Friendly-sector
membership excludes docked pilots; all-sector includes them. Crew routing follows
the real source/child relationships, with the original no-crew team fallback.
Commander delivery follows handover; the original unimplemented admin route is
not treated as an ordinary broadcast.

The order audit also found missing delivery to inactive human seats and an
incorrect blanket AI-order broadcast. Native team/wing/friendly-sector/crew
commands now queue on eligible local human pilots, retain original default-order
resolution for individual recipients, and follow the original acceptance/expiry
path when control changes. Explicit AI orders retain own-team authority; public
Everyone/Allies/AllSector/Leaders broadcasts cannot bypass the original server
command gates. `ChatRouting.h` records those server predicates for local authority;
its tests include invalid channels, individual allied authority and cross-team
broadcast rejection. Original training's self-order restriction is retained.
These corrections do not implement transport or certify network interoperability.

Completed local sessions now leave `STAGE_STARTED` for `STAGE_OVER`. Native
operation boundaries reject subsequent refits/purchases, credit/request/donation
changes, leadership/wings/seats, orders, crew changes, ejection, travel and launch.
The completed roster/account UI keeps inspection but disables those actions.
Two real timed Conquest cases, docked and flying, verify the result, simulation
time, object counts, pilot identity, equipment value, credits, hull/fuel/ammo,
position, station, controls, donation links and command slots stay unchanged.
Text chat remains available. This is the native non-restarting results policy;
the original server's automatic next-match reset still belongs to transport/session
integration. The focused normal checks passed before the full verification checkpoint below.


### Verification checkpoint: controller/chat/terminal batch

The normal suite passed 35/35 in 135.33 seconds and AddressSanitizer/LeakSanitizer
passed 35/35 in 184.80 seconds. The rebuilt binary was installed under
`/tmp/freeallegiance-install`; its hash matched the build, and desktop-file
validation passed for `org.freeallegiance.Native.desktop`.

Five installed Wayland/Vulkan synchronization-validation runs passed: frontend
(1,600 frames), station travel (500), command gestures (750), original mouse input
(1,450), and training death/debrief/restart (1,120). Recorded pacing misses were
2/0/1/1/2 respectively, including transitions. The controller-map menu also
rendered correctly. No Vulkan validation errors were reported. Real Vulkan pixel
checks passed alpha/additive interleaving, stable depth order, opaque occlusion
and the additive fast path.

An isolated 3,000-frame 165 Hz run recorded zero misses. After 500 warmup frames,
2,498 presentation-feedback records gave sample-to-present median/p95/p99
2.124/2.744/2.747 ms, sample-to-submit 0.133/0.183/0.256 ms, and submit-to-ready
0.400/0.425/0.505 ms. That final capture was in the hangar, with zero world
triangles; it is a window/frontend pacing checkpoint, not flight-load evidence or
input-to-photon measurement.

The optional arena input pilot was corrected to keep fitted group fire enabled
and to rank guns by sustainable energy demand. Its two retry runs still ended
under the original death rule at six kills. The 49-kill event-driven mission
checks pass separately; input-driven victory remains unverified. These pilot-only
changes do not weaken the original missions or alter gameplay.

## Current execution: chat filtering and display completeness

The receive audit found missing original pilot mutes, public/unknown-chat filters,
known voice-to-text lookup and human quick-voice suppression. `ChatPreferences.h`
now supplies native policy, with 1,024 independent frozen original comparisons.
Real local-pilot tests verify HQ exceptions, personal messages, known and unknown
voice IDs, explicit spoken text, muted order rejection, inactive-seat delivery,
unmute restoration and own outgoing quick-chat preferences. Known catalog text
uses a startup-prepared sorted index; it does not scan/reparse the MDL in flight.
Mute storage is a bounded ID bitset. Roster updates change only the presentation
snapshot and revision, with no economy tick, including in completed sessions.

Native interface configuration exposes all four chat/display preferences and
pilot accounts expose mute/unmute. Apply/Cancel, results access, identity bounds,
source-preserving JSON migration and native persistence are covered. Migration
preserves the original effective startup inversion of the first two chat filters,
rather than silently treating saved booleans as their active values. There are
now 33 supported original JSON preferences. Lobby/admin transport filtering stays
within the deferred transport scope.

`ChatCensor.h` shares the original 113-word dictionary, case table and substitution
loop with WinTrek. Native prepares its bounded pointer table once and caches
censored display text separately from raw messages. WinTrek retains its existing
word table and allocation pattern, adding no frame scan or retained UI object.
Unsigned high-byte indexing defines the original signed-char out-of-bounds case;
ASCII substitutions stay unchanged. 13,337 frozen dictionary/case/overlap/disabled
and UTF-8 comparisons pass. A live toggle rebuilds history display without changing
raw text, message IDs or order authority. Windows compilation/performance remains
an independent unrun gate.

The native log now wraps long messages on UTF-8 boundaries, including words wider
than the column, and pages by visual rows. Sparse message/line IDs keep older pages
anchored through new messages and eviction. New messages append to prepared spans;
unchanged frames do not repeat layout work. Font/scale or censor changes reflow the
bounded history. The log uses one continuous backdrop and draws above radar/status
labels. Original history autoscroll uses a monotonic UI clock instead of paused
simulation time. `MessageLifetime.h` shares the original 7.5/30-second critical-tip
constant with WinTrek; ordinary message history stays persistent. Tests exercise
UTF-8/long-word preservation, blank lines, very narrow columns, sparse IDs and
lifetime boundaries. Native pane layout remains an intentional modern adapter.

The focused six-check suite passed in 20.47 seconds before the final layering and
clock correction. A 440-frame Wayland/Vulkan chat run then passed wrapping,
historical anchoring, live filtering, font resize and unchanged-frame layout reuse;
one transition miss was recorded, with no validation errors. Final full-suite and
installed flight/display verification follows this checkpoint.


### Verification checkpoint: chat display/filter batch

After the final backdrop and clock corrections, the full normal suite passed
35/35 in 134.96 seconds; AddressSanitizer/LeakSanitizer passed 35/35 in 184.30
seconds. The installed binary matched SHA-256
`4152cf5b5a985e8fa99ab0d2040ed54ac19762d2d0901b19dbf3e06bb54278b8`, and desktop-file
validation passed. The installed chat menu and 440-frame gameplay display test
passed Vulkan synchronization validation; screenshots of both were inspected.
The C++17 shared-header adapter type check passed, and the censor dictionary was
compared byte for byte with the pinned original source. This is not a Windows
client compilation or performance test.

An isolated installed 3,000-frame camera/boost/orbit run at 2560×1440 held 165 Hz
with zero misses and 4,903 world triangles in its final frame. After 500 warmup
frames, 2,498 feedback records gave sample-to-present median/p95/p99
3.327/4.344/4.384 ms, sample-to-submit 0.236/0.372/0.444 ms, and submit-to-ready
0.670/1.573/2.072 ms. The capture includes the ship, thruster/exhaust effects and
world geometry. These are application/presentation timestamps, not photon latency
or WinTrek performance. Subsequent sonic-chat changes affect event/catalog routing;
final verification of that batch follows below.

## Current execution: original sonic-chat identity and voice lists

A deeper check of `InitializeQuickChatMenu`, `TLookup<..., DoSetIndex>` and
`PlaySonicChat` found a substantive semantic mismatch: native commands had carried
recording/template IDs where the original carries the authored sonic-chat table
index. Several logical messages share a recording, so audio IDs cannot identify
chat text. The preceding receive tests using audio IDs did not establish this
part of equivalence; they are now corrected to pass original logical IDs.

Native now parses the complete `sonicChats` table: 310 logical IDs and 307 menu
commands in the installed data. Sparse holes, duplicate-index overwrites and the
last `SetIndex` on reused sonic objects retain original semantics. Incoming ID 4
(Defend fighter) and ID 5 (Defend capital ship) use the same recording but resolve
distinct text. Affirmative remains authored ID 15. Registration/fallback updates
only audio resource IDs, keeping logical IDs and voice text stable; this also
removes the stale text index that could arise after audio registration. Explicit
text with a direct sound ID remains a separate original receive path.

Voice lists retain all variants instead of rejecting multiple voices. Variant zero
is the original receive default; variant lookup is available without parsing or
allocating during delivery. Audio prepares voice recordings, including non-menu
entries and alternate variants, once at startup. Its tests verify playback does
not decode more waves. Unknown out-of-table IDs are silent when unfiltered, matching
`PlaySonicChat`; an empty registered voice list retains its direct-sound fallback.

A synthetic authored catalog verifies shared recordings with distinct texts,
sparse IDs, duplicate/reused-object indexes, non-menu entries, alternate/absent
variants, empty lists and audio-ID reassignment. Real-pilot delivery verifies
logical receive/send behavior. Installed IDs 4/5/15 are checked independently of
resource IDs. The focused normal audio/session/training/command/player checks
passed 5/5 in 23.15 seconds before the final canonical-ID and extended UI checks.

The final normal suite passed 35/35 in 134.55 seconds. A 2,200-frame Wayland/Vulkan
run passed logical affirmative playback, wrapping, paging, display toggles and
font changes. It also demonstrated actual ten-second autoscroll restoration while
IGC simulation time stayed frozen. One transition miss was recorded, with no
validation errors. These local semantic checks do not implement packet transport
or certify cross-client interoperability.

### Verification checkpoint: sonic identity batch

The final AddressSanitizer/LeakSanitizer suite passed 35/35 in 184.17 seconds.
The installed executable matched SHA-256
`cb0241f057beb00135d8d869b13b0a407f330b01298643809474a3478aecb807`, with a valid
desktop file. Its extended 2,200-frame chat check passed on Wayland with Vulkan
synchronization validation, including the real ten-second paused-simulation
timeout. The resulting screenshot was inspected.

An isolated 3,000-frame installed camera/boost/orbit flight rendered 4,903 world
triangles in its final frame at 2560×1440/165 Hz. It recorded one miss. After
500 warmup frames, 2,498 feedback records gave sample-to-present median/p95/p99
3.874/4.567/4.569 ms, sample-to-submit 0.233/0.336/0.449 ms, and submit-to-ready
0.671/1.567/2.096 ms. No Vulkan validation error was reported; the ship/world/
exhaust screenshot was inspected. These are application timestamps, not photon
latency or a Windows performance comparison.

## Current execution: chat entry, audiences and history

The next receive/compose audit found additional concrete differences: native
suppressed repeated chats within one second, retained 512 rather than 600 records,
omitted recipient labels, dropped unvoiced order text, and sent typed shell
commands as ordinary chat. A local echo also changed its channel to individual
echo, losing the original recipient and notification-cue semantics.

`ChatPresentation.h` now shares the original receive-history predicate, tint,
600-record bound and mute/unmute prefix parser with WinTrek/BaseClient. Windows
keeps its original container, allocation pattern and case-insensitive player
lookup. Native formats sender/recipient/verb/visible-sector text on arrival and
caches the side tint; unchanged HUD frames retain their existing layout spans.
Repeated chats bypass notification coalescing and keep separate serials. The
native event log deliberately combines notifications with chat within its bound.

Outgoing local echoes preserve their original channel and recipient. They do not
queue a remote order on the sender. Original selection rules exclude individual
self-orders and incoming broadcast commands from chat history while retaining
their command-state transitions. Local typed `!mute`/`!unmute` uses exact names
with ASCII case folding, ignores unknown names/commands and needs no recipient;
the original shell self-mute exception remains. Native configuration/roster
actions preserve their existing self-mute restriction.

Independent frozen branch comparisons and real IGC cases cover recipient labels,
side colors, identical arrivals, command descriptions, local/remote self-order
semantics, mute prefix/name edge cases, self-unmute and 600-entry eviction. The
real renderer's chat smoke adds audience labels, repeated arrivals and local
shell checks. Verification of this batch follows after compilation.

The chat audience/entry batch's focused session/training/command/player suite
passed 4/4 in 15.86 seconds. A C++14 adapter compiled and ran its shared parser
and receive policy without raising the Windows language standard. Final combined
verification includes the queued-request corrections below.

## Current execution: queued orders and financial requests

Tracing the whole `SetQueuedCommand` → `RejectQueuedCommand` →
`AcceptQueuedCommand` chain exposed a mistake in the earlier native timeout tests:
WinTrek calls `RejectQueuedCommand(false)` after fifteen seconds. It discards
sender/reply tracking while retaining the queued order, allowing late acceptance.
Native had deleted the command. The old test asserted native's behavior without
freezing this original branch; it was not evidence of timeout equivalence. Money
requests also used only native menus, persisted indefinitely and required the
whole requested balance, unlike original acceptance.

`QueuedRequests.h` shares original rejection callbacks, fifteen-second lifetime,
partial/zero-balance approval and rescue-tip selection with WinTrek. The Windows
callers keep their original state, transport and update loop. Native queues orders
and financial requests through the same per-pilot sender/deadline slot. Money
replaces an order and orders replace money; a changed sender replaces the old
request silently rather than sending an automatic negative. Explicit rejection
still sends the original negative, and approval sends affirmative even when the
approved transfer is zero. Native guards the original own-team financial route
and rejects truncated pilot/hull identifiers. Departures clear pending reply
identities. Training orders retain their original timeout exception.

Native accept/execute/reject keys now operate on money requests as well as orders.
Partial approval conserves balances and consumes its request once. Inactive local
seats retain these semantics when control changes or the timeout passes. Original
critical order, recovery and financial tips are restored; input prompt adaptation
runs only when text is posted. The queued-request HUD is independent of the center
HUD and has a cached key legend. Pilot-command menus update their request snapshot
before rebuilding and reuse unchanged views; financial approval is available
without the full balance, while completed sessions disable mutation.

Frozen original branches compare 108 rejection/timeout callback sequences and 36
approval limits. Eight recovery-tip cases also verify that the hull query retains
the original short-circuit behavior, including avoiding unrelated gunner hulls.
Real IGC tests cover partial/zero transfer, original reply sounds,
order/money replacement, late acceptance without stale acknowledgement, own-team
authority, inactive seats, request expiry and departure. Chat tests now include
the original extra critical tip/sound. The Wayland team smoke adds menu and actual
keyboard approval in flight with the center HUD disabled. Combined verification
is pending below; these checks still do not certify Windows runtime/performance
or transport interoperability.

The combined checkpoint passed the normal suite 35/35 in 134.95 seconds and
AddressSanitizer/LeakSanitizer 35/35 in 184.59 seconds. LeakSanitizer was rerun
outside the restricted sandbox after its thread inspection failed under sandbox
ptrace restrictions; this checkpoint uses the successful unrestricted run. The
installed 650-frame team check passed menu and real keyboard approval with zero
presentation misses and no Vulkan validation errors. Its screenshot was inspected
and includes a readable financial request/key legend with center gauges disabled.

The final receive-path review additionally corrected HQ tint (the original keeps
its full side color), the unknown-chat filter's exact text sentinel, and group
delivery. The local author's send-only `CHAT_GROUP_NOECHO` route remains silent;
FedSrv's per-pilot group delivery normalizes to an individual recipient. Native
tests now cover these original branch details. Final verification below includes
these follow-up corrections.


### Final installed checkpoint: chat and queued requests

The final receive-path follow-ups pass all 35 normal checks in 135.01 seconds
and all 35 AddressSanitizer/LeakSanitizer checks in 184.88 seconds. Both new
shared request/chat adapters compiled and ran under C++14 without changing the
Windows language standard. This is an adapter check, not a Windows binary build.
The installed executable and build match SHA256
`c6b6115e19cf8edbae77eccf036d2166363ab58697fe9a76c0b51cbc2fea961b`;
its desktop entry validates and the callback inventory matches regeneration.

The installed Wayland/Vulkan team check (650 frames) passed menu and actual
keyboard money approval with center gauges disabled, zero misses and 1,894 final
world triangles. The chat check (2,200 frames) passed original recipient labels,
repeat arrivals, mute/unmute, known logical voice identity, wrapping, history
anchoring and the real ten-second scroll timeout while simulation stayed paused;
it recorded one transition miss. Neither run reported Vulkan validation errors.
Their captures were inspected.

An isolated installed camera/boost/orbit flight check ran 3,000 frames at 165 Hz
with zero misses and 4,901 final world triangles. After 500 warmup frames, 2,498
feedback records gave sample-to-presentation median/p95/p99
3.255/4.068/4.109 ms and sample-to-submit 0.227/0.356/0.440 ms. The application
and presentation timestamps do not establish physical input-to-photon latency
or Windows performance. The flight capture was inspected; further HUD/lifecycle
work follows this checkpoint.


## Current execution: station loss, elimination and HUD reservations

The station-loss audit found that native evacuation launched exhausted-life
observers instead of moving them to another restart station. `StationEvacuation.h`
now shares the original FedSrv list-mutation and child-before-pilot evacuation
policy. Eighteen frozen original comparisons cover station-list ordering, zero
through two observers and missing/non-restart/restart alternatives. Real Split
Base destruction and capture verify living crew launch, observer relocation,
identity/equipment/credit retention and command-view inspection.

The last-base test then exposed a native passenger crash in `SetAmmo`: passengers
have no base hull. Native elimination also deleted human seats instead of applying
the original server's reset. `SideElimination.h` shares its hull guard, drone kill,
forced human death and `Reset(false)` sequence with FedSrv. Native retains human
IDs and accounts, clears requests, commands and donation links, and prevents
recursive elimination. Its station adapter refuses launch for exhausted observers
and inactive sides. No new WinTrek frame-loop work is introduced; shared server
callbacks inline at their existing events with no intermediate allocations.

Two-team terminal and continuing three-team cases cover crew death counters,
unequipped pods, preserved observer counters/accounts, cleared relationships and
world membership, and continued simulation for the surviving teams. Local control
cannot select an inactive seat; an active surviving seat remains controllable.
Inactive gameplay operations and incoming requests are rejected, while ordinary
chat and local mute preferences remain available. Frontend account/request actions
also follow side activity independently of whole-mission completion.

The side-activity gate applies to Conquest. Original narrated training deliberately
leaves its IGC sides inactive; this flag must not suppress authored controls,
commands or station launch. Training and menu regressions cover that distinction
and restoring active-seat account buttons without a money/roster change.

The real display test exposed an additional authority difference: native deferred
station termination, so two restart losses before the next step could each count
the other destroyed base. Both original station-death adapters terminate before
returning. Native now follows that order with an explicit retained IGC reference;
later callbacks cannot count, scan, collide with or relocate an observer to the
destroyed station. Seven real station-loss cases now include successive and
same-update two-team/three-team losses, plus capture after another loss. Duplicate
death/audio delivery is still suppressed. The eliminated snapshot retires former
hangar/equipment/control state while retaining original counters and identity.

An eliminated team has score and session-menu HUD controls. The native adapter
keeps the shared local authority running for other active teams, while disabling
the eliminated team's gameplay. This is separate from whole-mission completion;
it does not claim to reproduce original network GAME_OVER delivery or client-side
ResetMission, which cannot reset the continuing local authority. No Windows frame
loop was changed by this batch. The server helper extraction also preserves its
remaining per-team game-over notification branch.

The actual HUD chat capture showed a contact label overlapping the kill/death
counter. The existing fixed occupancy grid now reserves that counter, headers,
key legends, objectives, queued requests, message rows and contact pane. Inventory
reservation follows its actual equipment/cargo row count and hidden center/target
panes no longer reserve space. Bound captions, radar/camera labels and both help
columns are cached on configuration changes; unchanged HUD frames no longer build
those strings. Counter text changes only with its values, and passenger text uses
a stack buffer. A validated 440-frame Wayland chat check passes and its capture was
inspected. Final full-suite and installed verification follow this batch.

Further capture inspection found overlapping observer/objective/training captions
and messages. Their header now uses an arithmetic row layout and message paging is
updated once before reservation/drawing, so the occupancy grid and actual message
rows agree. The compact eliminated-team panel leaves recent chat visible and
omits unavailable research/production tips. The small sector map now shows the
current or hovered sector in its existing caption, rather than superimposing all
sector names on its markers. These native layout changes allocate no new frame
storage and do not introduce the original retained UI architecture.

The optional arena input driver still dies during the original waves. Its logs
show successful real weapon kills and repeated repair trips, but do not establish
that its tactics can win the scenario. No mission difficulty or gameplay policy
was weakened; full input-driven arena victory remains an acceptance gap alongside
Windows replay/performance/capture and physical-device checks.

### Installed verification: station loss and HUD

The final gameplay/frontend changes pass all 35 normal checks in 77.34 seconds and
all 35 AddressSanitizer/LeakSanitizer checks in 106.65 seconds. Subsequent changes
are native HUD layout only; both builds compile them and the real display checks
exercise them. The shared station adapters also compile and instantiate under
C++14; a Windows binary build remains unperformed.

The installed executable and build match SHA256
`af928fe78fc3af8a6e10e6e7d44f43dc8eba87084f874151b2336a2895c2a439`.
The desktop entry validates and the callback inventory matches regeneration.
The 260-frame elimination flow and its 110-frame HUD capture verify same-update
base losses, continuing authority, score/menu hit testing and readable messages.
The 2,200-frame chat check retains its real ten-second paused-simulation timeout,
UTF-8 wrapping, history anchors and unchanged-layout reuse; one transition miss.
The final 650-frame team check passes menu/key approval, handover and donations
with center gauges disabled and zero misses. Captures were inspected; none of
these display checks report Vulkan validation errors.

The final GPU elimination flow also passes AddressSanitizer with leak detection
disabled. Its separate unsuppressed LeakSanitizer run reports 512 bytes at
shutdown: one 256-byte allocation inside `libvulkan_radeon.so` and one inside
`libvulkan_lvp.so`, identified from recorded dynamic-library mappings after their
unload. That GPU process is **not** a passing leak check. Its first reporting run
also hit an `llvm-symbolizer` crash; the raw-address rerun retained the leak
diagnostics. No driver suppression or driver lifetime change was added to the
client. The full 35-check sanitizer suite above remains the passing result.

An isolated final installed boost/turn/camera flight runs 3,000 frames at 165 Hz,
with zero misses and 4,903 final world triangles. After 500 warmup frames, 2,498
feedback records give sample-to-presentation median/p95/p99 3.489/4.075/4.081 ms
and sample-to-submit 0.238/0.390/0.452 ms. Presentation intervals remain
6.061 ms median and 6.063 ms p99, with a 165.000 Hz mean. The capture was inspected.
These are application/compositor timestamps, not physical input-to-photon
measurements or a WinTrek performance result.

## Radar bracket transparency

Player testing identified opaque black rectangles beneath missile-lock graphics.
The installed `lockedbmp` and `plockbmp` assets have no alpha mask and leave their
embedded color-key flag false. Original `RadarImageImpl` nevertheless requests
`LoadSurface(..., true)` and draws expanded brackets with source-alpha blending;
`ImportImageFactory::Read` combines that caller hint with the asset flag.
The native loader previously omitted the caller hint.

`Assets::image(name, true)` now creates and caches a black-color-key variant
without mutating the unkeyed image used by other consumers. It preserves authored
RGB and nonblack alpha, and sets exact-black coverage to zero before GPU upload
and mip generation. The HUD requests this variant for full/partial missile locks,
selection, threat, flag and artifact brackets. Conversion occurs only at load
time; the existing ordered HUD batch and shader remain unchanged. No WinTrek
source, frame traversal, draw count or pipeline change is required.

Real Vulkan readback checks all six installed sprites at native and doubled size:
transparent centers preserve the world pixels exactly, and visible strokes still
draw. Unkeyed textures remain intact and variants reuse cached objects. Existing
alpha/additive ordering, depth occlusion, material reuse and additive fast-path
checks also pass. Both the normal and AddressSanitizer GPU checks pass; the latter
uses leak detection disabled because the separate Mesa shutdown leak investigation
above remains open. The two affected headless model/inventory checks pass in
5.53 seconds, and model loading also passes ASan/LSan separately. The full 35-check
suite was not repeated for this asset/HUD change.

The updated installed client runs a 380-frame Wayland combat scene with validation,
seed 1, zero misses and a fully locked target. Its capture was inspected: the
target remains visible through the lock graphics. The installed and build binaries
match SHA256 `edea8b83ee96f956ee31767df54e25633b75c005daff17a2319a26ede64fd3c0`.

## Native starfield perspective

Player testing identified stars moving like the outside of a sphere. The native
camera basis matched `glm::lookAtRH`, but its star projection reused the original
`StarImageImpl` hemisphere fold and orthographic screen coordinates. Those
coordinates omit perspective division and mirror the rear hemisphere into view,
compressing stars into a disc instead of viewing the sky from its center.

Native background drawing now uses `ProjectPerspectiveStar`: positive camera
forward depth, the scene's vertical field of view and aspect ratio, perspective
division, and frustum culling before division. Stars remain directions at infinity,
so eye translation produces no parallax. Leaving the frustum invalidates a star's
existing history slot; reentry starts a point without a streak from a behind-camera
or offscreen position. Authored seeds, directions, brightness and the shared
original motion-streak curve remain. The native loop reuses its existing instance
and history buffers; it adds no allocation, texture or rendering pass. WinTrek's
folded projection remains unchanged.

`native-effects` passes 34,992 independent scene-matrix comparisons over yaw,
pitch, roll, field of view and aspect ratio, plus horizon/rear rejection and
direction-scale invariance. Its 7,560 original folded projection/motion cases
still pass. The complete effects check passes normally in 12.11 seconds and under
ASan/LSan in 16.34 seconds. The full 35-check suite was not repeated for this
presentation-only change.

The real Vulkan check now constructs the actual HUD and exercises its background
pass: matrix agreement, rear/outside rejection, unchanged positions after eye
translation, rotation and FOV changes, clean reentry, visible star pixels, and
foreground occlusion. Existing compositing and all six color-keyed bracket checks
also pass. The same GPU checks pass AddressSanitizer with leak detection disabled;
the separate Mesa shutdown allocation investigation remains open.

The updated installed binary runs an isolated 3,000-frame boost/turn/view-change
flight at 165 Hz with zero misses and no validation errors. Its orbit-view capture
was inspected. After 500 warmup frames, 2,498 feedback records give
sample-to-presentation median/p95/p99 3.184/4.008/4.037 ms and sample-to-submit
0.198/0.258/0.421 ms. Presentation intervals are 6.061 ms median and 6.063 ms p99,
with a 164.999 Hz mean. These remain compositor/application timestamps, not
input-to-photon measurements or Windows performance results. Installed and build
binaries match SHA256 `24c9179204644f1cfee39bad09f9f557f36e3900b75a29e5f65fc8d8ab948a71`.

## Fleet scale and asteroid material checkpoint

The [fleet scale audit](native-scale.md) supersedes the earlier installed binary
checkpoint and records the removed frontend caps, safe ship-ID exhaustion,
native spatial-query/collision acceleration, instanced bindless submission,
growing presentation arenas and full logical fleet audio. WinTrek retains its
existing paths. Tests compare the original and accelerated policies on the same
live objects; long-run collision outcomes still depend on original heap-address
identities, so this is not a portable replay or networking compatibility claim.

White asteroids were an omitted inherited material, not missing art or a shape
change. `ThingGeo::Load` calls `Geo::FindTexture` and seeds the render context
before drawing. All seven original asteroid assets omit an explicit texture on
their middle LOD. Native `ModelData` now records the corresponding default image
through MDL texture/group/transform/LOD nodes. Model upload applies it to bare
meshes, while preserving explicit mesh textures and skins. No per-frame material
search or new render pass is required, and no original Windows code changes.

All 36 headless integration tests pass in normal, optimized and ASan/LSan builds.
The Vulkan checks pass normally and under ASan (GPU leak detection disabled as
above), including every asteroid at four projected detail levels compared to
an explicitly textured reference and a visible white-material control. The
corrected actual-client asteroid capture was inspected. An isolated 1,000-frame
2,049-ship idle control holds 165 displayed frames/s with zero pacing misses;
sustained combat remains CPU limited. The scale audit records the separate dense,
spread and idle measurements, remaining work and installed executable hash.


## Native multiplayer implementation (2026-10-01)

The native executable now connects to a separate `allegiance-server` authority.
This uses the original IGC simulation and the shared ClientCore policies already
used by native offline play. It does not speak the existing DirectPlay/FedMessaging
wire protocol. WinTrek and FedSrv remain on their existing protocol and code paths.
No Windows binary build or Windows performance result is claimed.

`network_transport.*` is the replaceable GameNetworkingSockets adapter, pinned to
upstream commit `0abe0ed1fe318c197b43326d4fbb03b04517c7fb`. Its interface deals only
in connections, events and byte spans. `network_session.*` owns authentication,
command/input queues, simulation cadence, snapshots, clock sampling, prediction
history, interpolation and reconnection. `network_game.inl` bridges flat network
state to original IGC objects and invokes the existing native gameplay methods
as the authenticated pilot. The authority, replica and offline roles are explicit;
replicas never decide collisions, damage, economy, victory or sector transitions.

There is **no wire schema version or generic field serializer**. Build and game
content SHA-256 identities must match before a pilot is allocated. Records are
little-endian, fixed-width, pointer-free, standard-layout memory images. Original
pointer-free IGC exports supply object creation data; missile, projectile and
chaff creation pointers are resolved from local identities. Ship parts have one
flat state array rather than a duplicate array in the old ship export. Match
identity, entity serial, transition epoch and firing identity distinguish rounds,
object reuse, discontinuities and predicted projectile adoption; none is a wire
format version. The build fingerprint is deliberately conservative about compiler,
architecture and floating-point options. Different builds do not negotiate layouts.

The authority runs at 60 Hz and publishes at 20 Hz. Inputs contain controls, never
a claimed pose or hit. The newest input includes two predecessors. Reliable,
sequenced gameplay requests have bounded, retained receipts so reconnect/retry
cannot repeat a purchase or donation. Held controls become neutral after 250 ms
without input. A 128-bit resume token preserves the pilot, wallet and receipt/input
history for a 30-second disconnected grace period. Mission hosting migrates among
connected peers; team commander identity is a separate original game concept.

Local flight and turret prediction execute original ship movement, weapon cooldown,
resource use, missile guidance and projectile motion. A snapshot restores authority
state and replays unacknowledged controls with effects suppressed. Firing identities
adopt confirmed shots rather than drawing duplicates. Remote ships use Hermite
position interpolation and quaternion orientation interpolation, with bounded
extrapolation and an adaptive 50–200 ms buffer. Small local presentation corrections
decay over 60 ms. Canonical poses are restored before further prediction; remote
interpolation and fractional local display extrapolation cannot enter IGC gameplay.
Epoch changes snap across launch, docking, hull, parent and sector transitions.

Projectile collisions run on the authoritative timeline. There is no hitscan-style
rewind. Historical launch-time catch-up, replay capture and wider WAN/jitter testing
remain explicit follow-up work; this implementation is not a claim that all latency
policy questions are settled. The current input timestamp drives queue scheduling,
not permission to submit a historical hit.

Visibility is enforced while extracting state, before transmission. Hidden enemy
ship poses never enter the replica. Known static objects retain their last observed
state. Public capture announcements update remembered station ownership without
refreshing unseen damage or upgrades. Enemy cargo, resource quantities, orders and
research are omitted. Visible missiles can exist without revealing their launcher.
Warnings and personal sound/camera notices are routed to their original audiences,
including pilots on sides other than the authority's currently selected actor.

World extraction is shared per side and viewed sector within a publication; each
pilot still receives its own status, wallet/request data and notification audience.
Acknowledged baselines are byte-XOR deltas compressed with LZ4. History has count
and byte limits. Large publications use 8 KiB unreliable fragments and bounded
reassembly, because this pinned GNS implementation makes oversized individual
unreliable messages reliable. Bootstrap uses a reliable bulk lane; control and
state have independent priority lanes. Regular state publication waits for the
bootstrap acknowledgement and observes the state lane's pending-byte/queue-time
budget. A skipped publication never changes simulation cadence. Positional deltas
can become less efficient when object arrays change; keyed delta encoding remains
an optimization opportunity.

The lobby supports team choice, host map/rule configuration, readiness, countdown,
joining, flight and hangar transitions, results and a new round without reconnecting.
Gameplay requests cover equipment/loadouts, reload/drop/swap, orders, targeting,
wing assignments, commander handoff, research, money/donations/requests, boarding,
turrets/promotion, station teleport, ripcord, recovery and chat/quick chat. The
server's normal configurable mission is original-core Conquest, including its
alternative victory rules. Offline practice and narrated training remain available.

Two lifecycle issues found by multiplayer are fixed only in the native build:
static core types now remove themselves through the final live core consumer,
rather than a destroyed initial mission; a departing pilot promotes a remaining
crew member before removing its ship. Original Windows object layouts, vtables,
lookup paths and connected packet handling remain unchanged by the native guards.
Native missile IDs now have separate authority/prediction namespaces and an O(1)
registry; wraparound skips live missiles and cannot turn an authority ID into a
prediction ID. The projectile check exercises 65,536 allocations with live missiles.
Camera resets, warp/ripcord effects, hangar/carrier notifications, research, station
loss and flag announcements route to independent pilots and crew. Configuration
and gameplay commands retain their shared sequence across reconnect; discarded
unprocessed input restarts at the authority's receipt.

Public discovery, Club/account authentication, persistent squads/ranks, original
Windows wire interoperability, deterministic cross-platform replay and production
hosting services remain outside the implemented native game session. Native
multiplayer gameplay integration is broader than the tested combinations; see
`allegiance-network-check` and the evidence below before treating parity as certified.


### Multiplayer verification checkpoint

The final optimized build passes all 36 existing headless integration tests
(42.40 seconds), plus all seven real-localhost-UDP network cases. The four
functional network cases also pass Clang AddressSanitizer and LeakSanitizer with
`detect_leaks=1:halt_on_error=1`. The GNS dependency itself is an uninstrumented
pinned release build. C and C++ compilers must use the same sanitizer runtime;
this machine's previous mixed GCC/Clang CMake cache pulled in both runtimes until
reconfigured with Clang for both languages.

Functional cases cover exact-build/content rejection before allocation, invalid
and live-respawn requests, once-only donations, money requests, research/buckets,
researched hull purchases, paid cargo refits and saved loadouts; team chat and
orders; two independent pilots launching, flying, returning through the original
navigation/collision path and receiving docking notices; sector travel/epoch
changes; enemy visibility, retained bases and private research; boarding, turret
fire/prediction, promotion, death/pods/recovery; lobby map/rules, teams, readiness,
countdown, mixed retained control sequences across reconnect, victory and the
next round; gun/missile prediction, authoritative missile identity/adoption and
visible missiles from an undisclosed launcher.

| Optimized localhost case | Server step/publication p99 | Other observations |
|---|---:|---|
| Normal two-pilot gameplay | 0.168 ms | Maximum continuous flight correction 0.358/0.860 m |
| Crew/turret lifecycle | 0.153 ms | Independent crew inputs, promotion and recovery pass |
| Lobby/reconnect/results | 0.148 ms | Configuration/action ordering and new-round bootstrap pass |
| Projectiles | 0.151 ms | Identity wraparound and hidden-launcher checks pass |
| 5% loss, 50 ms each direction, 5% reorder | 0.285 ms | RTT during impairment 100.4/116.5 ms; maximum correction 1.325/1.108 m |
| 1,500 visible lightly moving ships, 5% loss, 20 ms each direction | 2.538 ms | 610.1 KiB raw state; fragmented/delta publications continue |
| 100 connections and independent IGC replicas | 5.464 ms | Three-second steady-state publication check; all input histories bounded |

Timings include `Server::step`, simulation and scheduled extraction/compression/
sending, and exclude `Server::poll`. The fleet is unarmed, without autopilot,
moving at 10 m/s; the 100-pilot case uses docked humans. These are separate tests,
not a combined combat/WAN/100-rendering-client benchmark. Both clients and the
server run on this machine. The projectile fixture deliberately relocates its
shooter by about 4 km, so its approximately 4,353 m correction statistic is not
normal predicted-flight error. The impaired case's final printed RTT is measured
after removing impairment; the table uses the separately sampled impaired RTT.

A separate authority and the actual Wayland/Vulkan/OpenAL native executable pass
the default lobby -> ready -> start/countdown -> hangar -> launch smoke. The client
renders 2,500 frames at the reported 165 Hz refresh with two pacing misses, no
Vulkan validation error and a final sample-to-present measurement of 2.232 ms.
It receives 310 snapshots, reports 6.0 ms final RTT and 6.865 m maximum correction
in flight among the original collidable asteroids. The cockpit/HUD/asteroid capture
was visually inspected, and the server continues running after client departure.
This is application/compositor timing, not input-to-photon evidence. The installed
art has an existing missing `vo_ma_woopie.wav` voice asset; the check remains able
to play with that asset absent.

The updated client and headless authority are installed together under
`/tmp/freeallegiance-install`. No complete Windows build, original-server
interoperability, prolonged WAN combat certification or deterministic replay
result is claimed by these checks.


## Multiplayer presentation correction (2026-10-02)

A new real-UDP presentation regression reproduces the reported stationary-base/
ship judder at 165 rendered frames/s, 60 prediction ticks/s and 20 publications/s.
The frontend's extra pre-step poll is included. Before the fix, the cockpit camera
held on 279 of 495 measured frames; its position differed from the displayed ship's
cockpit by up to 0.444336 m, and frame-to-frame translation p50/p99 was 0/3.838379 m.
Previous tests stepped/rendered at the simulation rate and did not catch this.

Three presentation errors are corrected:

- Camera caching and camera integration now use a monotonically advancing render
  clock for replicas. The canonical simulation clock can repeat between rendered
  frames or be replaced during reconciliation; neither can hold or reverse the
  camera clock. Offline/authority cameras retain their simulation clock.
- Reconciliation compares restored canonical poses. The last `GameState` position
  already included the previous display correction and fractional extrapolation;
  accumulating that value fed the existing correction back into itself at every
  publication. Position and source/turret attitude errors now decay only in
  presentation, over the existing 60 ms time constant. Normal predicted movement
  and input rotation do not pass through this correction filter. Transition epochs
  clear corrections, and large attitude discontinuities snap.
- Remote interpolation advances a continuous timeline. Arrival timing and adaptive
  buffer changes steer its rate within +/-10%, rather than replacing the timeline
  at every packet. The existing interpolation buffer and 100 ms extrapolation
  bound remain. Parent-hull rotation is also fractionally projected for crew eye
  offsets, with the turret's independent rotation retained.

Every displayed position/orientation remains reversible and is restored before
prediction or another authority snapshot. Original IGC state, input cadence,
projectile simulation and wire records are unchanged by this correction. No
WinTrek path changes.

The optimized `--case presentation` check measures 495 frames after warmup in
both localhost and 5% loss/20 ms each direction/5% reorder conditions. Both have
zero held camera/remote frames and no camera/displayed-cockpit mismatch. Camera
translation p50/p99 is 0.524902/0.585938 m locally and 0.437012/0.477539 m impaired;
remote translation is 0.614746/0.633301 and 0.605957/0.610352 m respectively. Camera
angular-step p50/p99 is 0.000416516/0.000440545 locally and
0.000417147/0.000441002 impaired (forward-vector difference, approximately radians
at these small angles). The test checks translation, rotation and remote-clock
continuity, not just connectivity or average FPS.

All 36 existing headless integration tests pass (42.05 seconds). Gameplay, crew,
lobby/reconnect, projectile and high-refresh presentation cases pass AddressSanitizer
and LeakSanitizer with leak detection enabled. These results do not certify every
network presentation condition; they add coverage for the original failure and
its snapshot/rotation variants.

The full optimized UDP suite also passes all eight cases, including impaired
gameplay, the 1,500-ship publication fixture and 100 independent client replicas.
Server step/publication p99 is 2.959 ms for the fleet and 5.303 ms for the peers;
the same fixture and timing limitations documented above apply.

A separate authority and the actual native client complete 3,000 Wayland/Vulkan
frames at 2560x1440 and 165 Hz on RADV STRIX_HALO. The client receives 371 snapshots,
reports 6.1 ms final RTT, 0.106 m maximum correction and two pacing misses. In a
steady-flight interval, 1,449 consecutive camera-position intervals have zero held
frames; translation-step p50/p99 is 0.624641/0.665925 m. Presentation feedback in
the steady interval has sample-to-present p50/p95/p99 of
2.117745/2.211090/2.215177 ms, with present-interval p50/p99 of 6.061/6.064 ms.
These are application/compositor measurements, not input-to-photon measurements.
The flight screenshot was inspected, and the authority remains running after
client departure. Both updated executables are installed together under
`/tmp/freeallegiance-install`; existing sessions must restart both ends to use the
matching new build fingerprint.

### Display-target timing

The native frontend now passes the acquired frame's expected presentation time
from the frame pacer to the replica. This is `SSchedule::deadline` in the shared
`CLOCK_MONOTONIC` domain: the predicted display time, rather than the potentially
earlier `SSchedule::target` used to request presentation from the compositor.
Local ship/crew attitude, camera and owned-projectile projection include the
sample-to-display lead. Projection can cross a simulation-tick boundary; it does
not produce extra inputs or simulate future resource use, collisions or shots.

Remote interpolation evaluates its continuous sample-time clock plus that lead,
with the existing remote buffering delay and extrapolation bound retained. The
additional lead is never accumulated into the clock or persistent reconciliation
corrections. Corrections are temporarily decayed to the display target, and the
replica camera advances by successive evaluated display times. Until pacing has
a schedule, or if its target has already passed, rendering uses sample time.
Presentation-only local projection is bounded to 100 ms after stalls. Native
offline simulation and WinTrek are unchanged.

`--scale-csv` records the pose sample/target timestamps and presentation horizons,
alongside camera positions; `--timing-csv` retains display feedback and the frame's
pacer deadline. The 165 Hz real-UDP regression now supplies a 2 ms display lead
in both localhost and impaired phases. Separate frozen-input/authority probes
change the lead through 8 ms, 24 ms, zero and 24 ms again, checking target-time
evaluation without accumulated lead, projection beyond one tick, canonical pose
restoration, unknown/late target fallback and the stall bound. All five default
UDP cases and all 36 headless integration tests pass after this change.

The presentation regression also passes AddressSanitizer/LeakSanitizer with leak
detection enabled. Its localhost and impaired phases each have zero camera and
remote-ship holds in 495 measured intervals and no camera/displayed-cockpit pose
or orientation mismatch. The changing-target and restoration probes pass too.

The actual Wayland/Vulkan client completes another 3,000 frames at 165 Hz,
receiving 371 snapshots with 6.1 ms final RTT, 0.106 m maximum correction and two
pacing misses. Every one of 1,450 measured pose targets exactly matches the
corresponding frame's deadline in the display-feedback trace, and every reported
lead equals that target minus its pose sample time. Lead p50/p99 is
2.179780/2.297630 ms. Across 1,449 consecutive camera intervals there are zero
holds; translation-step p50/p99 is 0.625020/0.666408 m. Sample-to-present
p50/p95/p99 is 2.182816/2.266727/2.304015 ms, and presentation intervals p50/p99
are 6.061/6.063 ms. These remain compositor/application measurements. The flight
capture is visually inspected, and the authority survives client departure.
The updated client and server are installed together under
`/tmp/freeallegiance-install`.

### Network flight input and original afterburner behavior

The multiplayer flight-input audit found that display-frame samples were
discarded whenever the 60 Hz input accumulator did not produce a tick. A wheel
or controller throttle-change hint could therefore be lost while its new analog
value remained visible, leaving autopilot in control. A held boost key eventually
reached a tick and released autopilot. Short fire, boost, movement and expendable
presses had the same sampling gap.

The replica now retains these digital samples in a single bitmask until an input
tick consumes them, using the latest analog axes/throttle. Inactive input, lobby,
reconnect and round changes clear the mask. The same canonical input drives local
prediction and transmission. Authority also retains presses when several queued
records are consumed in one tick; throttle-change hints are consumed once rather
than remaining in held state. This adds no wire fields or per-frame allocations
and changes no WinTrek path.

Prediction previously rounded each 1/60-second interval to 17 milliseconds, while
original authority IGC quantizes the absolute endpoint and alternates 16/17 ms.
Prediction now uses that same endpoint. A comparison starting from identical
canonical snapshots across all six phases reports zero velocity/fuel error and
forward-vector error below 0.000000003. Fitted afterburner power, fuel consumption,
camera motion and particles continue to use original IGC/ClientCore logic.

The installed `static_core` Conquest stock Scout has no fitted afterburner. IGC's
boost button still requests full ordinary forward thrust on such a ship, with
no afterburner power, fuel consumption or boost camera/particles. The regression
checks this behavior (17.989 m/s after one second from rest, fuel unchanged at
5), and separately uses the original PCore014 enhanced fighter with Booster 1
to verify actual boost. At half a second its authority/client power is
0.483/0.500 and fuel is 16.853/16.842 from 17; the replica is slightly ahead by
an input tick. Power returns to zero after release. Vector lock deliberately
preserves velocity when throttle changes, unless overridden by thrust buttons.
No artificial booster was added to the Scout or to the original data.

This audit also found that a second practice-session peer could terminate the
first pilot because replacement of the initial stand-in was keyed to the absence
of an economy account. Practice pilots have no such accounts. Replacement now
uses the actual first-join condition, preserving existing seats.

`allegiance-network-check --case flight` exercises real UDP joins, replicated
afterburner purchasing, throttle acceleration/braking, power/fuel/camera/particle
effects, vector-lock toggles, autopilot cancellation by a throttle hint between
ticks, short boost/fire presses and inactive-input clearing. The fire check
retains original IGC's delayed first shot. Authority is moved into empty space
for the motion fixture; its resulting large position correction is an intentional
test teleport, not a flight-convergence measurement.

All 36 existing headless tests and all nine optimized UDP cases pass, including
impaired gameplay, high-refresh presentation, 1,500-ship publication and 100
independent peers. The latter two server step/publication p99 values are
2.431/5.978 ms in this run; these are the existing fixture measurements, not a
controlled performance comparison. The 165 Hz localhost/impaired presentation
checks retain zero camera/remote holds in 495 intervals each.

The actual native client completes 2,200 networked Wayland/Vulkan frames at
2560x1440 and 165 Hz in Free Flight, exercising fitted boost and camera modes.
It receives 294 snapshots, reports 6.1 ms final RTT, 0.000 m maximum correction
and two pacing misses. The chase-view capture visibly shows the booster exhaust
and depleted fuel inventory indicator; the authority survives client departure.
These checks cover the reported flight-control paths, not every remaining
multiplayer-equivalence condition.

All six default UDP cases also pass AddressSanitizer and LeakSanitizer with leak
detection enabled. The rebuilt client and authority are installed together under
`/tmp/freeallegiance-install`, with installed hashes matching the release build.
Both running processes must restart to use the matching updated fingerprint.

### Outbound traffic audit (2026-10-02)

All game message producers are in `network_session.cpp`; the GNS adapter in
`network_transport.cpp` adds no game-state messages of its own. `networkFrame`,
`recordEvent`, addressed notices and the replica action sink supply their content.
The audit covers initial join, resumed sessions, round changes, steady gameplay,
paused lobbies, input, actions, effects and fragmented publications.

| Producer / records | Finding and treatment |
| --- | --- |
| Hello | One reliable build/content/name/token handshake per connection. Retained; the digests are needed and compress poorly. |
| Welcome | Full session/pilot/resume metadata remains reliable, but its native record is LZ4-compressed when that saves bytes. |
| Input | Retains 60 Hz sampling and up to two prior unacknowledged records for loss recovery. Compresses the complete bundle without removing any tick, number, button edge, analog value or acknowledgment. |
| Action / Configuration | Complete pointer-free native records remain ordered, reliable and idempotent. Compression removes unused text/loadout padding and repeated session values; resumed pending actions use the same path. Explicit reserved fields cover trailing padding in the frequently changing records. |
| Receipt | Small accepted/rejected action records and duplicate-request receipts are retained. Omitting them could break resume/idempotency. |
| Ping / Pong | Retained at two probes per second for clock/RTT and snapshot acknowledgments when no input is being sent. Their small records do not justify another compression envelope. |
| Snapshot / Delta | Reconstructs the same complete canonical frame from an acknowledged baseline, with sparse changed-word XOR ranges and record relocations keyed by object/part/pilot/team/bucket/event identity. Unchanged geometry, names, loadouts, technology, scores and resources occupy no changed-word payload. |
| Creation payloads | Asteroid/station exports duplicated the live position and orientation in `Entity`. Their wire copies of those fields are now invariant; the receiver restores the exact entity pose before creating the original IGC object. Models, textures, rotation parameters and all other creation data remain available. |
| Projectile and part deadlines | Gun/missile/chaff expiry, missile activation, weapon cooldown and launcher readiness use the original stored `Time::clock()` values. Time passage alone no longer changes expiry/activation records, and no float-seconds conversion can alter these millisecond deadlines. |
| Ship shield / booster power | The part record is already the source used by replica IGC. Removed the duplicate changing values in ship entities; stations retain their own shield state. |
| Notices / effects | Each peer tracks the notice/event watermarks in successfully sent snapshots. Only acknowledging one of those snapshots advances its delivery watermarks. Subsequent publications omit delivered entries; lost/unacknowledged publications still carry them within the existing retention windows. Reconnect retains delivery watermarks while clearing delta bases; a new match resets both. |
| Idle lobby | Unchanged paused state uses a 500 ms heartbeat. Readiness, host/team/session changes, chat and countdown changes bypass suppression. Active gameplay retains 20 Hz publication. |
| Fragment | Retains bounded independently assembled chunks when needed. The complete compressed publication is fragmented, so byte reductions also reduce fragment/header count. Missing fragments or baselines are never treated as valid state. |
| Disconnect | Uses the existing transport close/resume semantics. `World`, `Snapshot` and `Bye` enum entries are not additional outbound message streams. |

Previously, every logical snapshot contained a stationary outpost's coordinates.
Positional XOR/LZ4 generally turned those into a run of zeros, rather than sending
literal coordinates each time. However, an LZ4 zero run still has a size-dependent
cost, and insertion/removal could misalign all subsequent object/part/payload
bytes. Sparse ranges now omit unchanged coordinate words altogether. Relocations
preserve their acknowledged basis across object creation, retirement, reordered
storage, visibility changes and ID reuse. Decoder bounds cover copy and XOR
ranges, expanded sizes, native record sizes and trailing data. Stable arrays do
not allocate identity indexes; dense/no-baseline frames retain a dense fallback.

Asteroid and rotating-station orientation is still authoritative state sampled
at 20 Hz, but is transmitted once rather than also changing the creation export.
IGC advances spin incrementally using floating-point `PostRotate` and
renormalization, gated by visibility. A tick-indexed initial orientation and spin
recipe with explicit discontinuities would permit further reduction. Blindly
assuming exact local reconstruction would change collision/docking geometry or
the semantics of remembered unseen objects. This pass does not replace those
angles with an approximate predictor or introduce floating-point dependencies
into the lossless codec. Moving ships/missile guidance, health, visibility,
capture, resource and technology changes also remain authoritative.

Deadline auditing additionally corrected stale-age replica chaff: it now restores
the original expiry instead of restarting its lifetime at receipt. The receiver
also applies positive authoritative radius changes, including chaff growth.
These are native adapter changes; WinTrek's simulation/rendering paths are
unchanged.

The new `--case traffic` fixture verifies cold bootstrap and byte-exact full-frame
reconstruction, object insertion/removal/reordering, changing roster/economy/effect
arrays, variable payload lengths and unaligned tails, skipped publications,
unknown baselines, invalid relocation bounds, compressed actions/configuration/
welcome/input, idle heartbeat, readiness responsiveness, acknowledged notices/
sounds and reconnect. The projectile fixture also compares exact native expiry,
activation, cooldown and load deadlines on an independent replica and checks an
already-aged chaff's expiry/radius. The sparse codec uses integer/byte operations;
it does not rely on another machine producing identical transcendental results.

Measured application-message bytes in the optimized fixtures:

| Fixture | Previous positional XOR/LZ4 | New lossless codec |
| --- | ---: | ---: |
| Stable 1,024-object frame, 516,352 raw bytes | 2,094 bytes | 78 bytes |
| Same scene with insertion/removal/reordering | 47,740 bytes | 376 bytes |
| Target action | 472 bytes | 72 bytes |
| Three-record input recovery bundle | 200 bytes | 113 bytes |

The first two rows compare both codecs on identical synthetic records, including
their message headers. They demonstrate static-state and storage-churn costs,
not a universal reduction for moving fleets. The real UDP fixture's first client
received 641.1 KiB instead of 1,004.6 KiB in the ordinary gameplay case (about
36% less), 107.5 instead of 324.3 KiB in the crew case (67% less), and 36.1
instead of 358.1 KiB across lobby/round transitions (90% less). Snapshot counts
vary slightly with asynchronous delivery; these are application bytes, not
Ethernet/IP/GNS overhead. A separately measured two-second quiet lobby produced
three publications and 446 received application bytes; readiness changes still
bypassed the heartbeat and arrived within the test's 200 ms bound.

Validation on the final sources:

- All ten optimized UDP scenarios pass, including 5% loss/reordering, the
  1,500-visible-ship fixture and 100 independently authenticated peer replicas.
  Server p99 was 2.207 ms and 6.028 ms respectively in those two fixtures; these
  runs are functional scale checks, not controlled CPU comparisons.
- All seven default UDP scenarios pass AddressSanitizer and LeakSanitizer with
  leak detection enabled; all 36 CTest regression cases pass.
- The 165 Hz presentation checks report zero held camera/remote frames in both
  the local and impaired-link runs, with zero camera pose/angle mismatch.
- The actual Wayland/Vulkan client completed 2,200 frames at 2560x1440/165 Hz,
  receiving 240.0 KiB across 252 snapshots, with no reported local correction.
  The last sample-to-present measurement was 5.030 ms, with one pacing miss.
  The captured chase view shows the fitted fighter's booster exhaust, HUD and
  textured asteroids; the authority survived client departure.

The matching rebuilt client and authority are installed together under
`/tmp/freeallegiance-install`; their installed hashes match the release outputs.
Both processes must restart to use the updated build fingerprint.

### Compile-time 10 Hz publication comparison (2026-10-02)

The maintained FedSrv normally runs its mission loop at 20 Hz, but
`SendShipUpdates` skips alternate cycles. Its regular ship-update publication
rate is therefore 10 Hz. This is distinct from client uploads (150 ms intervals),
change-triggered messages and occasional other periodic updates.

Native publication is now compiled at 10 Hz through `ALLEGIANCE_SNAPSHOT_HZ` in
`network_records.hpp`, with a separate comparison build compiled at 20 Hz.
Simulation and input remain at 60 Hz. There is no runtime rate option or rate
negotiation field; the existing exact build fingerprint prevents mixing builds.
The same compile-time interval sizes the client's interpolation buffer: its
baseline changes from 50 ms to 100 ms, with the existing arrival-jitter allowance
and continuous display-target interpolation. This raises the remote-state delay
baseline by 50 ms without reducing the predicted local ship's update rate.

The `--case cadence` fixture measures steady real-time windows after bootstrap,
launch and fleet creation have drained. Both builds use the same PCore014
FreeFlight world (one sector, one station, 25 asteroids), two pilot seats, seed
47 and 60 Hz authority/input timing. Per-client received application traffic:

| Window | 20 Hz | 10 Hz | Reduction |
| --- | ---: | ---: | ---: |
| Docked in the sector, 3 seconds | 16.393 KiB/s | 8.249 KiB/s | 49.7% |
| Two cruising pilots, 5 seconds | 16.827 KiB/s | 8.467 KiB/s | 49.7% |
| Two continuously firing pilots, 3 seconds | 36.080 KiB/s | 21.009 KiB/s | 41.8% |
| 100 additional coasting ships, 5 seconds | 40.929 KiB/s | 20.526 KiB/s | 49.8% |

The firing case includes projectile/effect creation and retirement, whose costs
do not halve simply by publishing fewer frames. Upstream remained approximately
6.2-6.5 KiB/s because 60 Hz input/recovery bundles are unchanged. These figures
exclude IP/GNS framing and connection bursts; CPU samples were recorded alongside
other checks and are not a controlled performance comparison. The docked case
still includes changing asteroid orientation, illustrating the remaining spin
recipe opportunity. An unchanged lobby already uses its 500 ms heartbeat, so
the rate change offers little reduction there.

All ten existing UDP cases pass at 10 Hz, including impaired links, 1,500 visible
ships and 100 peer replicas. The 165 Hz presentation case reports zero held
camera/remote frames out of 495 measured frames, and zero camera pose/angle
mismatch, in both local and 5% loss / 20 ms per-direction / reordering phases.
Throttle, braking, booster fuel/camera/particles, projectile deadlines, crew,
sector transitions, lobby/reconnect and byte-exact delta tests also pass. All seven
default UDP cases pass AddressSanitizer/LeakSanitizer with leak detection enabled;
all 36 CTest regression cases pass. The actual Wayland/Vulkan run completed 2,200 frames
at 2560x1440/165 Hz, receiving 128.2 KiB over 134 snapshots with no reported local
correction; its final sample-to-present was 4.814 ms with two pacing misses.

This frequency comparison does not equate state coverage: original FedSrv sends
a small rotating selection of quantized ship corrections, whereas native frames
still cover all relevant objects with lossless authority state. Native remains
more bandwidth-intensive than that capped original stream in crowded sectors.

The matched 10 Hz client and authority are installed under
`/tmp/freeallegiance-install`, with installed hashes matching the release outputs.
Restart both processes to use the rebuilt fingerprint.

### Construction completion snapshot crash (2026-10-02)

The installed server's October 2, 09:00:19 EDT core dump (PID 2568457) identifies
`CbuildingEffectIGC::Export` called from native frame publication. At the start of
the shrink phase, original IGC consumes the constructor and replaces its asteroid
with a station. The construction shell remains for another ten seconds. Its
original exporter assumes a live constructor when copying the team color; native
snapshots export the shell throughout that final phase and dereferenced the cleared
builder pointer. A live UDP Light Outpost regression reproduced the same segfault
before the fix.

Native building effects now retain their creation color independently of the
constructor. Replica import also accepts the original `NA` asteroid marker for
completed shells, loading their visual surfaces without collision participation.
Snapshot application detaches a surviving, completed shell before retiring either
the builder or the asteroid: both original termination paths otherwise cancel an
attached shell. This follows WinTrek's quiet asteroid removal and preserves the
original ten-second fade. Destruction still cancels a build when its shell is
absent from the authority frame. The wire record remains the original pointer-free
`DataBuildingEffectExport`; no protocol layout or publication rate changed.

The shared IGC additions are guarded by `ALLEGIANCE_NATIVE`; Windows object layout
and lifecycle/export paths are unchanged. Completion uses the existing IGC
construction and ClientCore station creation code. The regression skips the long
production timer except its final second, then runs the real constructor travel,
drilling, building and shrink phases with live UDP snapshots.

The same fixture exposed a second native lifecycle fault when destroying a site
during construction. Shell termination queues the builder's death, while asteroid
termination clears ships' command targets immediately. The deferred builder
termination still had its construction flag set and assumed its plan target was
an asteroid. The native site now clears drilling/building flags before removing a
destroyed asteroid from a builder's commands. Constructor death still follows the
original IGC teardown; no WinTrek or FedSrv path changes.

`--case construction` passes AddressSanitizer and LeakSanitizer with leak
detection enabled. It exercises real funded Light Outpost and Supremacy Center
production, travel and drilling, forty-second effects, builder/asteroid
consumption, continuous shell identity on both existing peers, late joins during
the final fade, both render surfaces, collision retirement, and cancellation by
site destruction. Its generated Conquest fixture supplies all three technology
rock types in the home sector and discovers them explicitly; long mining and
production waits are excluded from this lifecycle check. All seven existing UDP
cases and all 36 CTest regressions also pass after both fixes.

The optimized release construction check also passes. Matching rebuilt client
and server binaries are installed under `/tmp/freeallegiance-install`, with
installed hashes matching the release outputs. Restart both processes to use the
new build fingerprint; publication remains compile-time 10 Hz.

### Compact canonical input and traffic audit (2026-10-02)

The client now applies cockpit slew scaling and raw-device autopilot dead-zone
policy locally, then square-root compands the four analog commands before its
first prediction step. Prediction, retained input history, replay and authority
all consume the same decoded bytes. Device dead zone and camera FOV are no longer
upstream fields; an explicit manual-override action bit carries the dead-zone
result. Original IGC continues to generate autopilot commands independently of
client display preferences. Offline native controls and WinTrek are unchanged.

Yaw, pitch and roll use `round(sign(x) * sqrt(abs(x)) * 127)` in a signed byte,
with -128 reserved and rejected. Throttle uses the requested
`round(sqrt(x) * 255)` in an unsigned byte. IGC's signed throttle storage describes
0–100% throttle: its center is 50%, rather than neutral thrust. Decoding squares
the normalized magnitude and restores sign for rotational axes. Zero rotation,
stopped throttle and all full-scale endpoints remain exact. An exhaustive
million-interval sweep measured worst absolute errors of 0.007858 for signed axes
and 0.003917 for throttle. Within 1% of zero, those bounds fall to 0.000790 and
0.000396. The first nonzero magnitudes are 1/127² and 1/255² respectively; every
valid canonical byte survives decode/re-encode unchanged.

A queued input is now a pointer-free 16-byte native record: 32-bit sample number
and authority tick, action mask, and four analog bytes. A 16-byte bundle prefix
contains signature, kind, match and publication receipt once. The newest and two
prior unacknowledged samples remain present for loss recovery; three samples
occupy 64 bytes, versus the former 200 raw / approximately 113 compressed bytes.
Modulo counters expand around full-width clocks and receipts, with the existing
256-input and half-second future bounds. Publication receipts also expand around
the full-width publication counter. Exact build/content matching still gates all
gameplay records; no version negotiation or custom record serialization was added.

Docked clients stop generating/predicting flight commands and acknowledge each
received publication with a 16-byte receipt-only bundle. Lobby clients use the
same receipt path. Clock probes, reliable UI actions, countdowns and presentation
continue. The server clears docked held controls, so launching starts neutral
until a new flight command arrives. Receipt-only traffic cannot refresh the
quarter-second input watchdog. Flying clients, including inactive seats, retain
60 Hz command history and prediction; snapshots remain compile-time 10 Hz.

The allocation/computation pass replaces input packet vectors and LZ4 with a
fixed-size stack bundle, avoids companding analog axes merely to sample display
frame button presses, inserts received samples into their ordered queue without
sorting the entire queue for each packet, reuses the authority control buffer,
and samples the watchdog clock once per authority step. Predicted projectile
scratch storage also retains capacity while releasing all IGC references after
each tick. Accepted-send accounting now includes handshakes, clock probes,
receipts and reconnect replay on both ends, previously omitted from the traffic
counters.

Matched real-time 10 Hz runs with the installed PCore014 assets measured the
following application traffic for one client. These are observations from local
UDP windows, rather than estimates of transport/IP overhead.

| Scenario | Previous upstream KiB/s | Current upstream KiB/s | Current downstream KiB/s |
| --- | ---: | ---: | ---: |
| Docked sector | 6.420 | 0.266 | 8.190 |
| Two cruising pilots | 6.449 | 3.703 | 8.468 |
| Two firing pilots | 6.514 | 3.703 | 21.009 |
| 100 additional coasting ships | 6.451 | 3.703 | 20.530 |

Upstream falls approximately 43% in flight and 96% while docked despite the new
counters including clock traffic. Previous counters omitted approximately 112
bytes/s of steady outgoing clock probes. Downstream state coverage and publication
cadence are preserved; the small docked difference comes from window/publication
boundaries and receipt timing.

The second producer audit also rechecked the remaining message families. Reliable
Action/Configure records already compress their unused fields (the target-action
fixture remains 472 to 72 bytes); resends are receipt-driven and idempotent.
Welcome is connection/round scoped and compressed. Ping/Pong continue at 2 Hz to
maintain the presentation clock during idle periods. Snapshots retain per-peer
visibility and acknowledged object-aligned sparse deltas; acknowledged effects
leave later frames, static positions do not create motion deltas, unchanged
lobbies use their bounded heartbeat, and projectile/launcher deadlines remain
absolute instead of generating remaining-time deltas. Fragment messages are
restricted to oversized frames. Asteroid attitude still changes through the
original incremental IGC update and visibility/lifecycle paths; replacing it with
a spin recipe needs an explicit phase/clock contract and reconstruction checks.
Suppressing its attitude corrections by assumption would lose exact authority
agreement, so that remains a separate optimization.

All nine default UDP cases pass, together with the impaired, 1,500-ship,
100-connection and cadence cases and all 36 CTest checks. The new inputs case
passes AddressSanitizer/LeakSanitizer with two deliberately lost sends,
duplicated/reordered recovery packets, stale-match rejection, receipt-only
watchdog traffic, invalid signed bytes/action bits/sample numbers/future ticks,
docked suppression, raw-device dead-zone thresholds and reconnect. The flight
and projectile cases also pass both sanitizers after scratch-buffer reuse. The
165 Hz presentation regression retains zero held camera/remote frames under
loss, delay and reordering, with display-target interpolation unchanged.

The rebuilt native Wayland/Vulkan client also completes a 2,200-frame connected
flight/camera smoke run at 2560×1440 and 165 Hz, with the authority healthy after
client departure. It reports 0.797 m maximum continuous correction, one pacing
miss and a final 4.805 ms sample-to-present interval. Matching client and server
binaries are installed under `/tmp/freeallegiance-install`; installed SHA-256
hashes match the release outputs. Restart both ends to use this input layout and
build fingerprint.

### Client waypoint retirement crash (2026-10-02)

The October 2, 11:33:28 EDT installed client coredump (PID 2610100, connecting to
127.0.0.1:27500) gives the exact failure path:
`CbuoyIGC::Terminate -> CshipIGC::SetCommand -> Game::applyNetworkFrame ->
Client::reconcile -> Client::message -> Client::poll -> Client::step`.
The buoy's mission pointer is null at the faulting dereference. The ship object
in the calling SetCommand frame is named `Miner3`, matching the reported nearby
mining activity. The crashed executable, core and debugger traces are preserved
as `/tmp/allegiance-client-2610100-binary`,
`/tmp/allegiance-client-2610100.core` and
`/tmp/allegiance-client-2610100-{trace,caller}.log`.

Snapshot application explicitly terminated disappearing buoys before replacing
ship commands which still consumed them. Original IGC tracks command consumers
separately from reference counts: when the last command releases a buoy it calls
Terminate itself. A TRef kept the old object allocated, but did not keep its
mission alive. Updating the command therefore terminated the already-detached
buoy a second time. The isolated waypoint regression reproduced this precise
null dereference under AddressSanitizer before the fix, including the original
CbuoyIGC::Terminate and CshipIGC::SetCommand source locations.

Replicated buoys now have one snapshot-owned consumer. Disappearing-buoy command
references are cleared across all ships in one pass before dropping snapshot
ownership; original IGC then terminates each buoy on its final release. Retained
buoys stay live while command users change, including same-publication handoffs
and markers with no locally visible ship consumers. Replica destruction releases
this ownership before normal mission teardown. This repairs the ownership/order
contract without changing CbuoyIGC's normal termination or consumer implementation.

The audit found two related gaps. Native buoy export now fills the existing
buoyID and visibility fields, which the original map-template exporter omitted.
IDs other than zero and private AI waypoints therefore import correctly. Native
ship records now include the internal plan slot as well as queued, accepted and
current commands. Four six-byte Command records replace three eight-byte records;
the command block remains 24 bytes and Entity has no size increase. Team-owned
command buoys are included even outside the viewed sector so remote pilot/miner
plans can resolve, while hostile command targets remain withheld. Unchanged
relationships skip SetCommand calls, preserving internal waypoint state and
avoiding unnecessary consumer/reset operations. Authority state bits are applied
after command and crew setters, whose normal side effects can clear drilling;
the construction regression verifies drilling/building state is preserved.

All changes to shared buoy export are guarded by ALLEGIANCE_NATIVE. WinTrek and
FedSrv keep their existing export fields, command handling, consumer lifetime and
object layout. Snapshot publication remains compile-time 10 Hz, input bundles
remain 64 bytes with three recovery samples, and incompatible builds remain
rejected by the existing fingerprint check.

The new default `--case waypoints` covers the exact failing retirement sequence,
all four command slots, nonzero identity and public/private visibility, marker
ownership without local ship commands, repeated unchanged snapshots, same-frame
consumer handoff, plan-only retirement, buoy ID reuse, remote-sector plans and
teardown. Its live UDP portion creates four original miners at two He3 asteroids,
launches the pilot toward an installed-core Adv Supremacy station, repeatedly
creates/retires private miner detours, leaves the pilot more than 1 km from docking
and checks reconnect. Both release and ASan/LSan runs observe 431 ticks with real
IGC mining active; the optimized case reports 0.171 ms authority p99. Its large
reported position correction is the fixture's deliberate relocation before the
navigation check, not a steady-flight correction measurement.

All ten default UDP cases and the impaired, 1,500-ship and 100-connection cases
pass after the fix, as do all 36 CTest checks. The waypoint and construction cases
also pass AddressSanitizer and LeakSanitizer, including mission teardown and
construction-site destruction. The 1,500-ship raw frame remains 610.1 KiB, and the
165 Hz display-target presentation regression retains zero camera/remote held
frames with loss, delay and reordering.

Matching fixed client/server binaries are installed under
`/tmp/freeallegiance-install`, with installed SHA-256 hashes matching release
outputs. A connected native Wayland/Vulkan flight/camera smoke run completes
2,200 frames at 2560×1440 and 165 Hz; the authority remains healthy after client
departure. Its final sample-to-present interval is 4.917 ms, with two pacing misses
and 0.865 m maximum continuous position correction. Restart both client and
server to use the corrected command layout and build fingerprint.


## Native model lifetime audit — 2026-10-02

The audit covers the native authority and replica, their original IGC models,
shared client policies, and the adapters that retain or borrow their data.
`TRef` retains object memory; it does **not** retain mission membership, a
ThingSite, a hit test, parts, or an attached hull. A detached model must never be
used as a live simulation or presentation object. Buoys also require balanced
consumer ownership independently of reference ownership.

| Surface | Result and ownership rule |
| --- | --- |
| Ship commands and navigation waypoints | Native ship detachment now releases every buoy consumer, clearing each slot before a synchronous final-consumer callback. Snapshot retirement clears commands and ripcord references while targets are live. Unchanged snapshots restore the autopilot flag without rebuilding a stale plan. |
| Explicit buoy removal and large fleets | Native buoy termination rejects reentry and already-detached objects. Its consumer count is widened to accommodate more than 32,767 simultaneous command consumers. Snapshot ownership remains separate from ship ownership. |
| Replica identity map and cascading removal | Import checks mission membership before reusing a retained model. Rocks can terminate their shells; the next publication recreates live models instead of writing through their disposed sites. Construction effects are created after asteroid dependencies and checked immediately after self-expiring updates. |
| Docking, boarding, hull and part replacement | Docking relationships wait for all destination stations. Canonical ship motion is restored after crew setters replace hit tests. Parts are restored after docking deactivates equipment. Original IGC owns child, parent, turret, mount and station references; crew/carrier/death regression cases exercise their teardown. |
| Selection and inactive human actors | Termination clears current and saved actor targets, departing actor state, missile camera references and ripcord destinations. Activating an actor cannot restore a detached selection. |
| Aleph destination cache | A retained endpoint validates cached destination membership, drops a retired endpoint and resolves a newly revealed endpoint of the same object ID. Native pathfinding, goal completion, pursuit and pending bomb effects tolerate an unavailable remote endpoint. |
| Prediction and reversible presentation | Presentation poses retain their models, are restored before snapshot replacement, and only write to live models. Prediction work-list entries are rechecked after earlier updates may have retired other entries. No presentation correction enters canonical simulation. |
| Projectiles, missiles, mines and probes | Launcher ownership intentionally preserves historical attribution. Original guidance/scanning checks cluster membership before reading a retained target's motion. Shot-key removal now verifies the retiring object's pointer, so an older lifetime cannot erase its replacement. Deferred deaths skip models already retired through another IGC path. |
| Network events and histories | An event with a replication serial resolves that serial, never a new lifetime that merely reused its object ID. Detached models cannot receive ThingSite effects. Transport receive buffers, delta baselines, fragmented records and input/action histories own their bytes; they do not retain IGC pointers. |
| Visibility, collision and station observations | Scanner registration invalidates the native spatial indexes; visibility caches retain scanner objects and validate side/range before reuse. Removed HitTests retain their models until collision cleanup, so a nonzero reference count alone does not indicate a stale gameplay owner. Station history survives termination callbacks until the ThingSite is destroyed; upgrades retain it across site reloads. |
| Mission teardown and retained ships | Native detachment clears waypoint, ripcord, last-missile, mine-launcher, requested-cluster, mining-cluster and base-data references before core destruction. Site-owned damage trackers, replica consumer leases, presentation poses and prediction work lists are released in their valid owner order. |
| Render assets and submitted GPU work | Model/frame/light spans borrow cached assets retained by renderer/model owners. Resolved MDL aliases now own separate frame/light metadata, sharing every mesh and texture, so enriching an alias cannot invalidate an already-published handle. Geometry and textures remain owned through GPU completion; per-frame buffers use fences, and swapchain destruction/device shutdown wait for outstanding work. |
| Audio and FMV | Following sound sources retain a shared emitter rather than an IGC pointer; site destruction deactivates it. Audio handles carry generations. Movie destruction wakes and joins the decoder before freeing FFmpeg/OpenAL resources; queued/current video frames own their images. |
| Frontend, training and network session callbacks | Frontend views copy game strings and rows rather than retaining GameState pointers. Training goals are destroyed while their core exists. Client action callbacks are disconnected before client destruction or round replacement; clients are destroyed before their factory-owned Games. Transport callbacks dispatch only to registered live owners. |

Replica retirement clears model relationships in one pass over ship command
slots and ripcord destinations. Its scoped retirement set lets individual
termination callbacks skip that already-completed work; unexpected cascaded
removals still perform their own cleanup. This avoids a ship-list scan for each
ordinary snapshot removal. No per-render-frame owner scans were added. Changes
to original IGC ship/buoy/aleph/navigation code are native-only; the WinTrek/FedSrv
paths and layouts remain unchanged.

`allegiance-model-lifetime-check` (`native-model-lifetimes` in CTest) adds actual
IGC regressions for departing command consumers, explicit buoy removal, 65,536
consumer leases, a disappearing autopilot target during presentation, reordered
construction dependencies, cascading local removal, expiry during import and
late effects, aleph endpoint ID reuse and navigation with missing endpoints,
saved actor/ripcord targets, new docking destinations, station upgrades, replaced
shot keys, duplicate deferred death, asset alias metadata replacement, and eight
authority/replica teardowns with external model references retained.
The UDP waypoint/construction/crew cases additionally exercise real consumers,
miners, constructors, gunner promotion, reconnect and late joins.

Validation of the final build:

- All 37 CTest checks pass in both release and AddressSanitizer/LeakSanitizer
  builds; no sanitizer failures or leaks were reported.
- All ten default UDP cases pass. The waypoint and construction cases also pass
  under ASan/LSan. The construction sanitizer fixture was rerun after the
  concurrent release fixture initially occupied its fixed test port.
- The 1,500-ship realtime fixture passes with 5% loss and 20 ms simulated latency
  in each direction: 2.634 ms server p99, at the existing 10 Hz snapshot cadence.
  This is a smoke performance observation, not an isolated before/after benchmark.
- A separate authority/client Wayland session completes 2,200 frames at
  2560×1440/165 Hz with Vulkan synchronization validation enabled and no validation
  errors. The final sample-to-present interval is 5.185 ms, two pacing misses,
  and maximum continuous position correction 0.803 m; the authority remains
  healthy after the client leaves.
- The release client and server are installed together in
  `/tmp/freeallegiance-install`; installed hashes match the tested build outputs.
  Restart both processes to use the new exact-build fingerprint. Previous
  installed binaries are retained in `/tmp/allegiance-lifetime-before-native`
  and `/tmp/allegiance-lifetime-before-server` for any old-process core analysis.

## Command view, drones and production audit — 2026-10-02

This pass follows the reported network-play problems through WinTrek input and
investment policies, FedSrv command/production callbacks, the original IGC drone
state machine, and the native authority, replica and menus. The original IGC AI
remains authoritative; no replacement mining or constructor AI was introduced.

| Surface and original implementation | Native result |
| --- | --- |
| F2 camera movement; WinTrek command-camera input and `CshipIGC::SetCluster` | Fixed a replica import that called `SetCluster` on every unchanged publication. This setter deliberately performs arrival/teleport callbacks, clears controls/ripcord state and recenters the camera even for the same sector. Imports now call it only when the relationship changes. Continuous pan and zoom survive publications while piloting and viewing another sector. |
| Command-camera keyboard, lateral thrust keys and selection drag | Keyboard steering and lateral-thrust keys operate the command camera without also steering the pilot's ship. Lateral keys contribute the original half-speed pan. Selection dragging suppresses camera movement, roll and keyboard zoom. Forward/backward thrust, boost and physical joystick flight axes retain their existing meanings. |
| Ship relationship/state replication | Side, station and parent setters already guard equal relationships. Command-slot restoration and autopilot restoration retain the preceding lifetime fixes. Sector changes still execute their genuine arrival callbacks; docking, motion, parts and state restoration retain their required ordering. |
| Drone identity; `WinTrek/loadout.cpp::CanInvest` and `PartListItem::CountDronesOfType` | Fixed missing purchased-drone-type identity on replicas. Authority and client now count the same purchased type, including upgraded/docked drones. The existing type-specific 16-bit entity definition slot also serves ships; the entity record does not grow. The production menu shows count/limit and disables an at-limit purchase. Retirement and reconnect restore the correct capacity. |
| Maximum drones; `MissionParams::nMaxDronesPerTeam` | Four per purchased drone type is the original default, not an additional native restriction. Original mission validation permits zero through ten; the existing native lobby setting uses that range. Both four- and ten-miner matches are tested. These rules are preserved for parity. |
| Production origin; `CbucketIGC::ForceComplete`, FedSrv `StationTypeCompleted`/`DroneTypeCompleted` | The original bucket chooses the first side station that can buy the item. The purchaser's occupied station is not an input to the original donation message. Native retains that rule; actual drone and constructor bucket completion while the purchaser occupies another compatible base verifies it. Local-base production would be an intentional rule change. |
| Constructor blueprint, name, launch and build authorization | Constructors retain their ordered station type and now use its builder name. Native completion follows the original station launch and default-order setup; construction still requires an explicit accepted build order. A newly purchased constructor never starts building automatically. |
| Production and order notifications; FedSrv completion and accepted-order handlers | Restored miner requests for a He3 asteroid, layer requests for minefield/tower locations, blueprint-specific constructor requests, and affirmative drone order acknowledgments through the existing visibility/audience-aware chat machinery. |
| Drone upgrades; FedSrv `UpgradeDrones`, `SideDevelopmentTechChange`, `BuildStation` and `UpgradeShip` | The native upgrade pass now also runs after station construction, matching FedSrv. Docked drones use the existing shared hull/part upgrade policy. Development and docking upgrades remain covered by their existing paths. |
| Mining selection and resource accounting; `PickDefaultOrder`, `PlotShipMove`, `FindTarget`, asteroid/cluster update | Original capacity, mining-rate attributes, least-targeted selection, known-sector navigation, threat retreat and asteroid regeneration are retained. Selection rejects rocks with insufficient ore relative to their reservations. Depletion causes waiting/reselection; replenishing a known rock resumes automatic mining without replacing orders. No arbitrary authority-side mining stall was reproduced. |
| Multiple miners at one rock; `CshipIGC::PlotShipMove` | Original arbitration permits one friendly miner to work a rock, selecting the one with least cargo. Other friendly miners return/unload; opposing miners can prevent mining. Four miners sharing two rocks all complete useful mining/unload cycles over live UDP without forced resets or injected replacement orders. |
| Unloading and resuming; miner plan/default-order logic and `CshipIGC::Update` | Original nearest eligible own-base selection already prefers the tested local garrison/refinery to the original remote garrison. Garrison docking observes the original ten-second dwell. A refinery with teleport-unload capability unloads without docking. Both run for twenty simulation minutes and repeatedly resume mining. Accepted orders can remain a mining reservation while the current plan is unloading; the native unit list now reports the actual plan or docked state. |
| Player-directed mining sector; FedSrv accepted orders | The existing shared `RememberMinerOrder` policy and consumer-owned queued buoy retain the ordered sector across unload trips. Ordinary new accepted commands clear stay-docked and set the original retreat cooldown. |
| Return/stay-docked/resume; FedSrv `FM_CS_ORDER_CHANGE` | Added the missing explicit action through `ClientCore::SetDroneStandby`, native authority validation and the unit menu. Return finds the nearest repair base, preferring a shipyard for carriers; it clears the remembered mining sector. Stay-docked prevents automatic relaunch; resume invokes original default-order selection. Its flag is replicated in an unused entity flag bit. Hold in space remains a separate accepted order. |
| Research completion, income and stockpiles | Native development, hull/part completion, payday and mining-income callbacks were compared with FedSrv. Existing shared policies cover them. Stockpile access/removal is latent original scaffolding, rather than evidence for a new native gameplay subsystem. |
| Mine/drain notifications and drone server-private wrappers | Native visibility-filtered entity resources convey ore changes, including exhaustion; original IGC owns ore-seen updates. FedSrv's mine/drain transport notifications and server-private `CFSDrone` wrapper do not imply another missing mining state machine. |

No WinTrek/FedSrv call sites or original IGC logic were changed in this pass.
The new shared standby helper is unused by Windows builds. Drone counts are
computed once per production row when economy state is refreshed; no additional
per-render-frame ship scan or entity bytes were introduced.

`native-drone-completeness` exercises continuous network command-camera movement,
original four/ten-drone limits, replica identity/counts and replacement capacity,
actual bucket production origin, constructor blueprint/name/manual authorization,
depletion/recovery notifications, standby/resume, and twenty-minute garrison and
refinery runs. The latter deliver six and seven loads respectively. The live UDP
`drones` case adds two clients, remote command-view movement, rejection of an
over-limit purchase without spending money, four-miner/two-rock contention,
standby/resume and reconnect. Legitimate ore/competition/threat/docking pauses
remain original behavior; an unexplained stop in a particular saved match still
needs that match's state to distinguish it from those conditions.

Validation of this pass:

- All 38 CTest checks pass in release and ASan/LSan builds. The sanitizer suite
  runs outside the tracing sandbox, which prevents LeakSanitizer inspection.
- All eleven default live UDP cases pass. The new drone case also passes under
  ASan/LSan without memory errors or leaks. Its release server p99 is 0.278 ms;
  each of the four miners completes at least one delivery during 400 simulation
  seconds, without resetting its AI. This small fixture is not a fleet benchmark.
- The native Wayland client completes 2,200 frames at 2560×1440/165 Hz with Vulkan
  synchronization validation enabled and no validation errors. Final
  sample-to-present is 4.929 ms with two pacing misses; the authority remains
  healthy after departure.
- Matching release client/server binaries are installed in
  `/tmp/freeallegiance-install`, with hashes matching the tested outputs. Restart
  both for their new exact-build identity. Previous installed binaries are kept
  in `/tmp/allegiance-drone-before-native` and
  `/tmp/allegiance-drone-before-server` for old-process core analysis.

## Network battle scale and deterministic effects — 2026-10-02

The native authority now accepts `--scale-test AI_PER_TEAM`. It reuses the
standalone encounter arrangement: opposing blue-noise discs, 72 m minimum
separation, spawn planes at ±1,400 m, and equal numbers of Interceptors,
Enhanced Fighters, Advanced Stealth Fighters and Scouts. The diagnostic waits
for the first client before advancing combat. The server's pilot starts docked;
F2 opens a client's command view directly from the hangar. Original IGC AI,
collision, weapons and practice replacement policies still run on the authority.

At 1,024 ships per team the busy encounter exceeds the server's 60 Hz budget.
The diagnostic logs overload and continues every fixed 1/60-second simulation
step, slowing its gameplay clock under load. It does not skip steps, change
collision rules, or claim real-time performance at this size. The ordinary
server's cadence guard is unchanged. Native-only profiling identifies dodge
queries and the original collision KD tree as substantial remaining CPU costs.

Large publications previously stalled around 50 applied snapshots. GNS's
default 512 KiB send queue rejected later fragments of multi-MiB publications;
the attempted partial publications kept wasting bandwidth against the old
acknowledged baseline. The adapter now allows a bounded complete publication
burst, with send/receive allocation caps of four maximum frames and a 32 MiB/s
congestion-controlled ceiling. Large state fragments use unreliable NoNagle;
short input/status messages retain NoDelay. Publication admission still requires
a nearly empty state lane. Snapshot history retains the acknowledged basis and
at least four frames, even when a crowded frame exceeds the ordinary 8 MiB
history budget. These are allocation limits, not preallocated buffers.

Unguided projectiles publish an invariant pose/time/velocity/spin anchor and
their original expiration deadline. The client evaluates the trajectory at the
frame's target presentation time; authoritative collision/removal and locally
predicted shot confirmation remain intact. Missile guidance still receives
authoritative state corrections. Original projectile ObjectIDs are invalid;
the parity check uses their native shot tokens to find the original objects.

Treasures (dropped equipment, cash, powerups and flags/artifacts) retain their
original birth position/velocity/time, 10 m/s² deceleration, clamped stopping
distance and arbitrary-axis tumble. Only the initial trajectory contributes
motion bytes to deltas. Amount, visibility and expiration changes, pickup and
removal remain authoritative. Import now also restores amount/treasure identity
and extended expiration deadlines for existing treasures. Decal flags do not
receive a model-orientation tumble. Other debris rendered locally from effects
does not become a new network entity.

The trajectory fields reuse existing type-specific entity slots; entity record
size is unchanged. Additional original-core anchor state/accessors are guarded
by `ALLEGIANCE_NATIVE`, leaving WinTrek/FedSrv layout and update loops unchanged.
Canonical import uses the original millisecond IGC clock, while rendering
evaluates a continuous presentation time. The new UDP `drops` case checks
byte-invariant motion through drift and rest, original arbitrary-axis rotation,
late import, amount/deadline changes, collection and expiration. The existing
`projectiles` case also checks invariant wire motion and original pose/spin.

Release validation passes all 38 CTest cases and the projectile, drops, traffic,
presentation and impaired-network UDP cases. The presentation case retains zero
camera or remote pose holds at 165 Hz with 10 Hz publications under packet loss,
delay and reordering.
Projectile, drops and model-lifetime tests also pass under ASan/LSan without
errors or leaks. Matching binaries are installed at
`/tmp/freeallegiance-install`; the previous installed pair is preserved as
`/tmp/allegiance-server-battle-before-native` and
`/tmp/allegiance-server-battle-before-server`.

The full Wayland/Vulkan network diagnostic completes 1,800 frames with
synchronization validation enabled, applies 359 snapshots with zero missing
baselines, and receives 207.3 MiB of application data. The authority completes
40 simulation seconds and remains alive after the client departs. Busy frames
remain CPU-limited; this validates joining and continued replication, not
comfortable rendering or bandwidth at this scale. Logs and the screenshot are
under `/tmp/allegiance-server-battle-check*`.
The four consecutive ten-second simulation windows report server p99
15.544/58.632/36.649/58.936 ms. Steady trajectory records reduce motion deltas;
dense projectile creation/removal and the rest of the battle still
need further work to reduce this volume and client processing cost.


## 512-per-team authority performance and effect lifetime — 2026-10-03

The measured workload is the same mixed-hull battle, seed 1, with 512 AI on each
side, a docked command observer 750 m from the encounter, 60 Hz simulation and
10 Hz publications. Measurements include a real Wayland/Vulkan client at
2560×1440 on the 165 Hz display. Earlier measurements kept the connected client
in the hangar; the final measurements use the live F2 command view. The skirmish
replacement policy remains
active; runs continue past the first engagement rather than measuring only the
initial orderly approach.

Measured critical-path server work (polling, simulation and capture):

| Build / interval | Worst ten-second-window p99 | Worst tick | Deadline misses |
| --- | ---: | ---: | ---: |
| Before, first 120 simulation seconds | 23.952 ms | not recorded | 671 / 7,200 |
| Intermediate optimizations, 180 simulation seconds | 14.145 ms | 16.511 ms | 0 / 10,800 |
| PGO / whole-program vtables / manual pinning, 300 seconds | 11.467 ms | 17.507 ms | 2 / 18,000 |
| Pools / automatic pinning / filtered targets, 300 seconds | 11.597 ms | 19.597 ms | 1 / 18,000 |
| Grouped rays / compact construction / filtered visibility, command view, 300 seconds | 10.086 ms | 18.645 ms | 2 / 18,000 |
| Dense model traversal, command view, 300 seconds | 9.219 ms | 14.510 ms | 0 / 18,000 |
| Dense model traversal, fresh command-view repeat, 600 seconds | 9.863 ms | 13.905 ms | 0 / 36,000 |

The 60 Hz budget is 16.667 ms. These are live localhost measurements on this
machine with the renderer active, not isolated headless simulation numbers or a
hardware-independent guarantee. Intermediate zero-miss runs did not establish
repeatable headroom; subsequent intermediate repeats still exposed rare
expensive ticks. The final dense-traversal build completed two fresh runs totaling
54,000 ticks and fifteen minutes without an over-budget tick or deadline miss.
Simulation-only worst-window p99 was 7.790 / 9.250 ms for the two runs. The
maximum critical-path tick left 2.157 ms within the budget. The renderer remained
active, but its display deadlines are separate: the clients recorded 72 / 133
display misses over 26,632 / 51,426 rendered frames on the 165 Hz display.
Detailed window measurements and validation are retained in local run reports;
generated benchmark output is not tracked.
Logs are
`/tmp/allegiance-512-live-before-server.log` and
`/tmp/allegiance-512-live-opt8-server.log`,
`/tmp/allegiance-512-live-wpd-server.log`, and
`/tmp/allegiance-512-live-filtered-server.log`,
`/tmp/allegiance-512-live-visibility-command-server.log`,
`/tmp/allegiance-512-live-dense-command-server.log`, and
`/tmp/allegiance-512-live-dense-repeat-server.log`.

Native-only optimizations preserve the original query and decision policies:

- Dodge snapshots ignore changes to objects they never index (projectiles and
  buoys), cache their SIMD query constants, and combine internal bounds from
  children. A Morton ordering builds those bounds without repeated median
  partitioning or scans at each depth, separating ships from large stationary
  obstacles. Exact leaf tests and candidate results
  retain the original linked-list order. Queries walk the depth-first node array
  using subtree skip offsets, and padded bounds load directly into SIMD
  registers. Leaf relative motion reuses the packed query constants. The
  exact acceleration-dependent horizons are computed lazily once per visited
  radius per query when the snapshot has at most 64 distinct radii; more varied
  snapshots retain the original calculation. Candidate membership uses a bitset
  indexed by the original list ordinal; ascending bit iteration replaces sorting
  every query's hits. Arithmetic, conservative margins and final order are
  unchanged. Bounds of positions four seconds ahead preserve the correlation
  between position and velocity. Construction converts those envelopes into
  secant slopes; queries with horizons up to four seconds use them in the same
  swept bounds test, with outward rounding margins. Longer queries retain
  independent velocity extrema. Exact leaf tests remain unchanged. Extra policy
  comparisons cover correlated/opposing motion at large coordinates and both
  sides of the horizon boundary.
- The collision KD tree retains its original target-bearing partitions and
  reverse leaf order. Packed overlap tests and contiguous caches of static and
  moving node/leaf bounds reduce scattered reads. Moving caches rebuild each
  frame; static caches rebuild when the original partition changes. Internal
  bounds use the existing sorted endpoint extremes rather than rescanning the
  same objects at every depth. The query cache excludes null-ID cast-only
  HitTests as targets: these could never pass the original query-ID ordering
  test. Their casts still run. Cached bounds enclose the remaining targets,
  allowing empty or projectile-only subtrees to be skipped while retaining the
  original pivots and eligible leaf ordering. Each cached leaf's bounds are
  clipped to its ancestor partition intervals; this preserves the original
  strict pivot eligibility while tightening the subtree bounds. Point casts
  on pivots and sphere bounds touching either side are checked against the
  original traversal. Queries walk the depth-first cache with explicit strict
  pivot gating and a local stack of pending high children. Exceptional depths
  continue through another invocation, preserving order without a depth limit.
  Clipped leaf bounds tighten internal bounds. Unary target-bearing branches
  collapse into their child; the child's clipped bounds retain the omitted
  strict ancestor test. Cached nodes occupy 44 bytes.
  Dead objects are released only
  after the cache has been invalidated.
- KD construction snapshots canonical bounds into contiguous value storage,
  with partition/target flags in a separate dense byte array. Endpoint filtering
  reads those byte flags without fetching the bounds record. Partition membership
  derives from a single binary search of the sorted pivot-axis endpoints, then
  marking the qualifying low/high endpoints. Original membership order is
  retained in construction. The root retains its original endpoint pointer
  sort, then encodes that order as 32-bit root-local indices plus the high/low
  endpoint bit. Children filter compact HitTest indices and endpoint tokens;
  the original pivot calculation reads the same bit-exact endpoint values from
  the bounds snapshot. Native 64-bit endpoints use existing alignment padding
  for the root-local index. Children no longer allocate unused endpoint pointer
  arrays or duplicate HitTest pointer arrays: their per-capacity-slot array
  storage falls from 56 to 28 bytes. Leaf pointer resolution uses the root's
  single pointer table. The original pivot search, child membership, reversed object order and endpoint
  permutation are retained. The policy fixture reconstructs pointers from
  tokens, checks the root sort and bit-exact values, and checks every reachable
  partition against the original filter predicates and pointer order. Windows
  construction continues using its original fields and allocations. Native
  construction buffers allocate uninitialized storage on capacity growth; each
  used index and endpoint token is written before it is read. Child membership
  is counted from qualifying endpoints before allocation. Capacity follows the
  child's membership rather than its parent's size, and leaves initially
  allocate membership indices alone. Endpoint arrays are added if a node later
  needs to subdivide; retained capacity remains reusable across frames.
- Construction stops subdividing a branch once its original membership filter
  contains only null-ID cast-only objects. No query can pass the original
  `query.id < target.id` test against those objects, so such descendants would
  return no targets. Their casts still run, and partitions containing targets
  retain their original pivots, endpoint permutation and collision order. The
  fixture checks empty-target branches and repeated transitions back to branches
  with eligible targets.
- Endpoint insertion keeps the original pointer permutation and high-before-low
  tie rule. A short local search handles coherent motion; a binary search handles
  endpoints appended far from their sorted positions, and an overlapping block
  move replaces the scalar pointer shift. The original long-sort algorithm and
  the root's choice between sorts are unchanged. A frozen original insertion
  fixture compares complete pointer order across 120 coherent, appended and
  tied layouts. Windows sorting remains unchanged.
- Moving HitTests produce swept boxes without writing unused KD endpoint
  values. Tree updates extract bit-exact endpoint values alongside the existing
  root-local index snapshot, only for indexed objects. Cast-only projectiles
  therefore avoid six unnecessary endpoint stores every simulation step;
  static endpoint behavior and Windows updates are unchanged.
- Native ray casts stage their packed boxes once for both trees. Node traversal
  groups rays and compares four boxes at a time, reusing node bounds. Candidate
  chains preserve depth-first leaf order for each ray. Candidates are applied
  in original cast order, with static before moving for each cast, retaining
  last-test/no-hit suppression and exact collision queue insertion order.
  Scratch retains capacity and clears gameplay pointers before processing
  collisions. The original-policy fixture compares original, scalar and grouped
  paths both before and after sorting; four-lane packets also test strict pivot
  contacts. The launcher's no-hit exclusion is checked before storing a
  candidate, avoiding candidate-chain work for pairs already excluded by the
  original policy. Windows retains its original per-ray traversal.
- Sector update, bounding-box, ray-cast and final-motion loops call the sealed
  native projectile factory type directly, allowing its existing methods and
  getters to inline. List ordering, expiry, blast creation and all other model
  dispatch remain unchanged; Windows loops retain their original virtual calls.
- Native update, bounding-box and final-motion passes use a dense sector index
  with cached model types. Append order reversed matches the original model
  list. Stable integer handles map removals to tombstones before references
  are released; compaction between updates preserves order and repairs a dense
  handle-to-position table without touching model objects or hash entries.
  Updates capture the initial upper index, so models added during that pass
  remain excluded as in the original list traversal. Retirement of another
  model safely clears its pending slot. Existing lists and their ownership
  remain available to IGC consumers. Policy checks compare complete index
  order, types and handles before and after retirement and compaction; lifetime
  checks exercise reloads, sector transfers, expiry and teardown. Windows keeps
  its original traversal.
- Native concrete ship, hull-adapter, projectile, missile and treasure classes
  are sealed, allowing the compiler to inline their existing virtual getters.
  The factory has no subclasses of these types. Windows class declarations
  remain extensible and unchanged. Clang release builds with LTO also use
  whole-program vtable optimization and private visibility for this closed,
  statically linked core and native game callback implementation; external
  platform and transport libraries remain outside this optimization scope.
  ThinLTO inputs consistently enable split LTO units.
- Native projectile, point HitTest and ThingSite allocation uses per-thread
  pools, retaining peak storage until the owning simulation thread exits.
  Constructors, collision IDs, reference counting and destruction are unchanged.
  Headers lie outside object storage. ASan builds poison released payloads and
  quarantine them before reuse; the allocator check exercises growth, alignment,
  simultaneous live objects, reuse and thread teardown. Windows allocators are
  unchanged.
- Authority radar labels are computed on demand. Original visibility,
  remembered observations, simulation state and other frontend state still
  update; callers requesting contacts receive the complete list.
- Ordinary visibility scans walk the existing pickable list backwards and
  select side-tracked models. This is the original model-list subset in the
  original order, avoiding walks and no-op visibility calls for untracked
  projectiles. The policy fixture compares both ordered subsets. A sector
  retains full original scans if a tracked model is unpickable, changes its
  tracking attributes, or reloads while still in that sector. Windows retains
  its original scan.
- Ship capture uses a contiguous equipment read plan, invalidated by native
  add/remove/mount revisions. Cached metadata and original part order are reused;
  amounts, mounting fractions, firing/loading clocks, shields and afterburner
  power are read live. Cached pointers are checked only after the revision has
  been validated. Enemy private resources, controls, command targets and
  launcher state are excluded before extraction. A frozen former capture checks
  complete part bytes/order and private-field results before and after cargo
  moves, detachment and reattachment, with direct/deferred packet equality.
- Snapshot capture reuses contiguous record buffers, reads the concrete
  projectile state without redundant visibility/live-pose queries, and exports
  ship identity without first exporting and discarding a second equipment list.
- Projectile capture uses a 104-byte pointer-free DTO. The publication worker
  restores the generic entity defaults and original list positions, using the
  reused frame buffer. The UDP projectile check verifies that direct and deferred
  capture yield byte-identical complete packets. No wire layout changes.
- Captured projectile values live in a contiguous, revision-checked cache.
  Native setters for velocity, rotation, radius, signature, side, sector, expiry
  and shot token invalidate the record. Physics motion preserves the original
  trajectory anchor; launcher identity is read live. Slots are allocated on
  first publication and recycled when the ThingSite retires, without storing
  gameplay pointers. Publication epochs live beside these dense records,
  avoiding reads/writes of scattered presentation objects. Ordinals and live
  launcher IDs are filled in the cached value before its single copy into the
  frame. Slot reuse resets the epoch and sector history; trajectory refresh
  preserves the epoch until a relationship changes. The UDP projectile fixture
  exercises mutations, epoch stability and sector departure/arrival, verifies
  byte-identical direct/deferred packets, then checks stable trajectories again.
- An authority-owned worker builds packets and computes lossless deltas/LZ4 from
  immutable pointer-free captures. Gameplay, transport, acknowledgments and
  peer histories remain on the authority thread. One outstanding job per peer
  bounds the queue. Reliable bootstrap remains synchronous. Completed captures
  retain their original simulation tick and input acknowledgment.
- Flat relocation indices replace node-allocated hash maps; record order and
  reconstructed bytes are unchanged.
- Snapshot capture filters acknowledged notices and events before copying them
  into the frame. The original event queue already has increasing serials;
  one pass selects common and pilot-specific effects in that order instead of
  copying and sorting recent acknowledged history. Shared captures cache world
  data, and each peer selects effects using its own watermarks. The effects
  fixture compares complete packet bytes against the former post-capture
  filtering, with full, partial and zero acknowledgments through the same cache.
  Omission counters remain available; no wire records change. The headless
  publication benchmark now acknowledges each completed frame, matching its
  loss-free local receiver rather than repeatedly capturing stale effects.
- The target-machine release build uses Clang IR profile-guided optimization
  with the existing thin LTO and strict floating-point options. Its training
  includes 10,800 fixed authority ticks of mixed 512-per-team combat with
  publication, the complete default UDP suite, original-policy comparisons,
  and 3,600 native visual smoke-test frames. This is compiler code generation,
  not a gameplay change; normal builds without the profile remain supported.
- Automatic Linux core placement adapts the author's 0 A.D. implementation,
  incorporated under the repository's MIT license with permission. It discovers
  allowed CPUs, SMT siblings, last-level caches and preferred-core rankings.
  The server simulation and client render thread use separate physical cores
  and, where available, separate cache domains. On domains with varied preferred
  rankings and at least four fast cores, simulation uses the middle-ranked core
  within 85% of the best ranking, leaving the most preferred cores for bursty
  desktop work. Uniform or unavailable rankings retain the last-core tie break.
  Publication has a third core;
  driver/network/movie workers receive masks excluding their SMT siblings and
  preferring the render cache when there is enough space. Temporary unpinning
  prevents new threads
  from inheriting a critical thread's single-CPU mask. Small or unknown
  topologies and failed affinity calls leave scheduling unchanged; existing
  cpuset/taskset restrictions are respected. This changes only these processes'
  affinity, not host scheduling or power settings.

The original-policy check compares both paths on identical objects: 142,080
Dodge decisions, 9,600 path controls, 19,200 first-spotter queries and 7,895 ordered
collisions, including snapshot invalidation/mutations. It passes. The layouts
cover both shared fleet radii and independently randomized radii. Whole-battle
fingerprints are a separate diagnostic: original moving-object HitTest IDs are
object addresses, so allocation changes can alter which participant receives
first collision handling. This preexisting rule is retained; independently
allocated full battles are not claimed to produce bit-identical trajectories.

A collision lifetime hazard was found during these comparisons. Projectiles and
missiles can outlive their launcher's replaced hull. Native HitTest exclusions
now retain and release the excluded HitTest, preventing address reuse from
excluding an unrelated ship. The lifetime check fires a projectile, replaces
its launcher's hull, and verifies the retained exclusion before replacing and
releasing it. WinTrek/FedSrv ownership and layouts are unchanged by these
native-guarded changes.

Network explosion aging now runs on the replica's presentation clock, including
when docked or in command view. It formerly ran only during local flight
prediction, freezing effects for a docked observer. The UDP `effects` case
observes an explosion from the hangar command view, verifies its expiry, and
checks that a later publication does not replay the acknowledged explosion.
Snapshot import also refreshes frontend flags after applying camera events,
so simultaneous peer publications immediately expose the launch/death camera.
Autopilot engagement now captures the last raw device sample immediately on
import, preserving the manual-override threshold before byte quantization
instead of absorbing the first post-publication joystick movement as neutral.

The reported damage everywhere after combat is the original crowded-sector
penalty. CclusterIGC charges for the living ships in the sector, and CshipIGC
applies that cost to human ships independently of their distance from other
ships. Hundreds of surviving friendly AI therefore still trigger it. The
penalty and its constants are deliberately preserved for equivalence; future
scale tuning is a separate task.


Validation of the final dense-traversal build: all 40 release CTest cases pass.
The expanded original-policy and model-lifetime checks pass under ASan/LSan,
including dense order/handle/type validation, retirement and compaction. All
thirteen default real-UDP cases plus the impaired-network case pass. Presentation
has zero camera/remote holds at 165 Hz with 10 Hz snapshots, including 5% loss,
20 ms delay in each direction and 5% reordering.

### Quiet-sector damage and authoritative health (2026-10-03)

The reported ambient damage with one flying pilot was a replica bug, distinct
from the deliberately preserved crowded-sector penalty. `CclusterIGC::m_fCost`
was not initialized at construction. Original mission updates normally assign
it before ship updates; replicas import sectors and predict their own ship
without running those mission updates. Local `CshipIGC::Update` nevertheless
read that uninitialized cost and applied environmental damage. Replica death
callbacks correctly refused to decide a death, leaving a zero-health ship
that could still fly.

The real-UDP regression reproduced authority armor/shield of 1.000/1.000 while
the replica displayed 0.000/0.000. Its sector cost was 192,937,984. Construction
now initializes cost to zero. Native forward prediction and reconciliation
replay still update movement, equipment, energy, fuel and shield regeneration,
but do not apply crowding/out-of-bounds damage or expire lifepods. Native
replica `ReceiveDamage` also rejects damage and repair decisions before changing
health. Offline and authoritative rules retain their original behavior; the
prediction guards compile out of WinTrek.

Out-of-bounds and crowded-sector warnings now travel in two unused entity flag
bits and persist across local prediction. The wire record does not grow.
Authoritative crowding costs, population thresholds and damage amounts remain
unchanged. The constructor initialization also makes pre-update cost queries
well-defined on every platform.

`allegiance-network-check --case health`, registered as `native-network-health`,
checks a single flying pilot with no hostile ships: ten seconds of quiet flight
before and after warp travel, both forward and replayed prediction, rejection
of local damage, genuine authoritative environmental damage and warning
clearing, asteroid collision damage, hull repair, fatal collision/ejection,
pod death/recovery, and a healthy relaunch. Its moderate-speed rock impact leaves
the scout at 0.551 hull. It also verifies an intentional original rule:
lifepods bounce off asteroids without taking collision damage. Fatal damage
from other sources still kills a pod and returns the pilot to a restart station.
The health case passes in release and under ASan/LSan.

All 40 existing release CTest cases and the new health CTest pass. All fourteen
default real-UDP cases and the separate impaired-network case pass; the existing
model-lifetime regression also passes under ASan/LSan. The rebuilt client and
server are installed together in `/tmp/freeallegiance-install`, with installed
hashes matching the release outputs. Restart both processes for the new build
fingerprint.

### Original keymap interpretation and throttle regression (2026-10-03)

The throttle report is consistent with unintended vector lock. The saved
prototype native profile binds vector lock to Left Control with `Any`. The old
native interpreter dispatched that press through every modifier table, so
starting a Control chord also toggled vector lock. Original `shipIGC.cpp`
preserves world velocity while vector lock is enabled. Its booster button
temporarily overrides that rule even on a stock scout without an afterburner;
it does not clear the lock or consume nonexistent booster fuel. Normal throttle
and turns work without the booster when the lock is off.

The audit follows `TrekInput::LoadMap`, `TranslateKeyMessage`,
`HandleKeyMessage`, `SetFocus`, the window message loop and WinTrek's key filter
and camera actions. No WinTrek source or hot path changes are needed.

| Layer | Original policy and native correction |
| --- | --- |
| Modifier tables | Shift, Control and Alt select one exact press table. There is no fallback through an unmodified binding. Caps/Num Lock do not select a different modifier table. |
| `Any` | Held Boolean inputs ignore modifiers. Press lookup puts `Any` only in the unmodified table, exactly as original `LoadMap` does. The former shared held/press interpretation was wrong. |
| Down, repeat and release | Every accepted key-down, including OS repeat, may dispatch a command. Releases clear held state and never dispatch a press action. The provisional modifier-tap/vector-lock exception was removed. An explicitly modifier-bound action follows ordinary immediate key-down dispatch. |
| Native key editor | Capturing a standalone modifier retains `Any` for a continuous control, but uses its exact Shift/Control/Alt table for a press action. This prevents the editor from creating an unreachable modifier-bound action while keeping held booster/roll/etc. behavior intact. |
| Held state and focus | Gameplay tracks accepted SDL key events, rather than sampling physical keys whose presses were consumed by menus or chat. Focus loss, help and modal menus clear that state. Left/right generic modifier aliases contribute to the same held modifier mask; releasing a modifier re-evaluates an exact held chord without another key-down. |
| Original command identity | Primary bindings and aliases retain their authored TrekKey through import, lookup and settings persistence. Command/nav aliases therefore reach training as the original identifiers instead of being rewritten to the port's preferred equivalent. Command-view dispatch records that actual identifier when entering or leaving the view. |
| Native convenience defaults | Imports reserve the authored press slots, including unsupported original commands. HUD scale/HUD visibility/FMV convenience defaults cannot shadow original throttle-up, miner targeting or command-view bindings. |
| Camera dispatch | Shift/Alt no longer reinterpret a resolved command as a different camera action. Command perspective is its own mapped command. External/chase camera keys toggle back to cockpit as in WinTrek, and combat-camera keys return docked pilots to the hangar. |
| Global key filter | Message-page bindings remain available while composing chat or using a game menu. Key capture retains ownership of its input. |
| Defaults and persistence | New ordinary profiles prefer installed `inputmap1.mdl`, with fallback to the shipped default. Isolated custom profiles and Restore defaults use `defaultinputmap.mdl`. Existing saved native custom bindings are retained. Reload does not append duplicate default keyboard/mouse/controller aliases. Invalid duplicate original press slots are rejected atomically. |

The shipped map uses L for vector lock, Space for booster, F2 for cockpit, F3
for command view and Shift+F3 for command perspective. Existing saved prototype
profiles can have different physical keys; repairing interpretation does not
silently replace those preferences. Restoring defaults or explicitly importing
an original map selects the original arrangement. Imports remain limited to
the saved numeric map format and supported native logical actions.

`native-keyboard` compares both installed maps against a frozen independent
WinTrek reference across 1,312 key/modifier press and held cases, plus all 72
single-binding modifier policies. It covers repeats, releases, focus/modal
transitions, both sides of generic modifiers, authored command identities,
keypad Enter, collision reservation, persistence and atomic failures.
`native-sessions` verifies that Restore defaults returns the actual shipped map
without replacing a custom map when the frontend is merely constructed.

The extended real Wayland `--mouse-input-smoke-test` also exercises L down and
repeat, inert releases and modified L, Control+period, Control+mouse selection,
F3, Shift+F3 and F2 through the actual SDL dispatcher. It passes alongside the
existing mouse fire, targeting, fractional wheel throttle and camera zoom checks.

`native-network-throttle` launches an original stock Scout through real UDP,
without manually resetting launch controls/autopilot/vector lock. Full throttle
alone redirects travel after actual sideways and backward yaw turns. Zero and
partial throttle work on authority and replica. Explicit vector lock preserves
world velocity; the booster button temporarily overrides it without fuel use,
and release retains the lock. A second lock toggle restores normal heading
correction. Reconnect also preserves the intended controls. Long turns are
isolated from collisions and boundary damage in the fixture only.

All 42 release CTest cases pass. The keyboard, settings/rebinding UI and network throttle checks pass
under ASan/LSan. The rebuilt client and server are installed together in
`/tmp/freeallegiance-install`; installed hashes match the release outputs.
