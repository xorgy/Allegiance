# Standalone native frontend

Running `allegiance-native` now opens the main menu without constructing a
mission or capturing the mouse. All menu drawing shares the HUD's font atlas,
bindless textures and ordered quad batch. The default interface uses the original
main menu, training, games, team lobby, hangar, loadout, options and console
artwork. Authored MDL graphs compile to cached native control/draw records;
the legacy engine's pane/rendering architecture and D3D are not required.

See the [fleet scaling audit](native-scale.md) for large AI counts, renderer
batching, test commands and measured performance limits.

## Session flow

The default main menu opens the original training selector or original Internet/
LAN game lists. Create Game selects a local game or a discovered authority and
one of its supported cores. Original team setup, ready/countdown, briefing,
hangar, loadout and launch use the existing IGC and authoritative network state.
Training runs the original narrated mission scripts and authored slide images.
The original mapped keys and each widget's handlers control keyboard input;
classic buttons do not acquire an added Tab/arrows/Enter navigation scheme.

The earlier native interface remains optional with `--modern-ui`. Its separate
scenario/exercise flow is described below; it is not the default interface.

## Optional native scenarios

Home → Scenarios → Local lobby → Countdown → Hangar → Launch → Flight → Dock → Hangar.
The two custom guided exercises start in flight. The session menu's
Return to base selects a friendly landing station and engages IGC's autopilot;
manual steering cancels it. Docking automatically opens the hangar, and Launch
uses the station's actual launch slots. Equipment and hull replacement require
a docked ship. R after destruction recovers to a friendly hangar; it does not
respawn a live ship in place. Lifepods, rescue, pod abandonment, ripcord and refit preserve the player identity.
Completed exercises and limited skirmishes open a results screen. Leaving tears
down the mission and stops its audio; a new session can be started in the same
process. Escape opens the session menu, Tab/arrows navigate menu controls, Enter
activates them, and mouse hit testing uses framebuffer-aware layout coordinates.

The six scenario choices are combat practice, flight training, weapons training,
free flight, team skirmish and Conquest. The scenario training choices are **separate native exercises**,
not ports of the original narrated `src/training` mission scripts. Their objective
progress comes from simulation state: movement, controls, ammunition, kills and
docking. The four installed maps (`training_1`, `_3`, `_5`, `_6`) are usable;
the latter three contain multiple sectors. Practice uses `PCore014`; Conquest uses `static_core`, generated maps and original civilization/economy rules.

Local lobbies select a map and starting hull. Practice/skirmish configure enemy
and allied wingmen, three AI skill levels and a team kill limit. Click a wingman
count, type its replacement and press Enter; Escape cancels. The old eight/seven
wingman caps and 32-seat cap are removed. Initial ships and local seats share the
original 32,768 nonnegative ship identities; impossible totals are rejected.
Ship identities are recycled after retirement rather than wrapping negative. All configured local pilot seats must be ready before Start. The countdown defaults to 15 seconds and can be configured or cancelled. Team scores come from actual kills and end
limited sessions. This optional local lobby has AI units and local pilot seats.
Remote sessions use the original multiplayer lobby. Public directory discovery
is supported as described below; authenticated Club accounts remain unavailable.

## Server discovery

Internet and LAN lists use the original artwork and controls. LAN discovery is
automatic. Saved endpoints in `servers.txt` beside the native profile can name
either authorities or public directories. Directory replies expand to verified
authorities; names, rules, occupancy, supported cores and hosted missions still
come from each authority's normal encrypted, build/data-gated browse query.
Find Player and Create Game use the same discovered authority set.
Listings identify a hosted mission independently of its endpoint: saved/LAN/
directory aliases coalesce, selections follow refreshed routes, and selecting a
mission does not permit joining a different mission that later reuses its port.
Endpoints accept DNS hostnames, numeric IPv4 or bracketed IPv6 addresses.
Hostname lookup runs asynchronously; alternate resolved addresses are raced
when the preferred route is unreachable. Joining uses the verified numeric
endpoint and mission identity. Direct `--connect HOST:PORT` is supported too.

A community directory can be hosted independently of the game/data files:

```sh
build/linux-release/src/native/allegiance-directory --port 27599
build/linux-release/src/native/allegiance-server --directory DIRECTORY_HOST:27599
```

Clients add `DIRECTORY_HOST:27599` to their existing `servers.txt`; no new screen
or control is added. Multiple `--directory` arguments register an authority with
independent directories. Registration uses the observed source IP and the actual
game listener port, verifies that listener, renews its lease and retries outages
without interrupting gameplay. Operators still need a reachable game UDP port.
Each directory encodes its immutable endpoint list once per membership change.
Explicit saved/LAN endpoints remain usable if a public registration is withdrawn.
No public service hostname is hardcoded; this implements the self-hosted service,
not a deployed community service.

## Equipment and gameplay

Practice selects six fixture hulls. Conquest and original narrated training list the original station's available hulls and replace mounted equipment
with parts accepted by IGC's equipment-type, technology and hull-mount checks. Practice sessions grant equipment freely. Conquest charges actual hull/part costs,
checks station technology, funds original research/production buckets, and runs
original miners and constructors. Hull changes
restore standard equipment on the same ship and preserve its station. Empty
supported equipment slots remain selectable after depletion. Cargo reserves opens
all five slots; stock ammo, fuel, launchers or spare equipment, or clear a slot.
Original shared reload rules consume reserves automatically on depletion, including
mounting delays and alternate launcher types. The HUD separates cargo from active
equipment and shows reserve quantities; reload sounds are preloaded. In-flight
selection, swaps, dropping stacks and manual reload use original policy. Ten
persistent loadout banks preserve original hull/part purchase order and file
format; Conquest applies station and budget checks, while narrated training applies its authored station technology with free offline equipment.

## Configuration

Settings cover borderless fullscreen/windowed mode, HUD size, field of view,
master/effects/voice audio gains, feedback strength, mouse sensitivity/inversion,
particle detail, frame timing, radar and appearance preferences, startup movies
and 168 keyboard/controller actions. Rebinding supports Shift/Control/Alt chords,
exchanges conflicting assignments and clears aliases for the reassigned action. The HUD help and equipment shortcuts follow
the saved bindings. Callsigns can be edited from Home.

New profiles use the installed original input map (`inputmap1.mdl`, falling back
to `defaultinputmap.mdl`); isolated `--config` profiles and Restore defaults use
the shipped default map. Existing native custom bindings are retained. In the
shipped map, L toggles vector lock, Space holds the booster, F2 selects cockpit,
F3 toggles command view, and Shift+F3 changes command perspective. Native
convenience defaults cannot shadow commands in an imported original map.
`Any` has the original separate meanings: modifier-independent held controls,
but an unmodified-only entry for press actions. Presses include key repeat;
releases only clear held state. Menus and focus loss clear gameplay held keys.

Apply writes an atomic replacement of
`$XDG_CONFIG_HOME/freeallegiance/native.conf`, falling back to
`~/.config/freeallegiance/native.conf`. Cancel leaves applied settings intact.
`--config FILE` selects an isolated profile, including for automated tests.
Gameplay changes to HUD size using +/− remain temporary until configuration is
applied. The compositor may override requested window dimensions.

HUD font sizes are calibrated to the original bitmap fonts' visible capitals,
rather than their padded line heights. Labels and headings are roughly 20–25%
smaller than the previous native sizes, without shrinking panel artwork or
icons. FreeType still rasterizes at the displayed pixel size and reuses the
resident coverage atlas.

Original preferences can be migrated with `--import-config FILE`. Supported
fields include fullscreen, inversion, mouse sensitivity, particle density,
joystick response/dead zone, virtual mouse mode/indicator/acceleration, feedback
enable/gain, SFX/voice gain, callsign,
startup movies, radar detail, sector grid, throttle repeat delay and ten appearance
switches. A first ordinary startup can read Steam's `config.json`; an isolated
`--config` does not migrate it implicitly. Original files are never rewritten.
`--import-inputmap FILE` imports saved numeric keyboard, mouse and controller maps,
preserving modifier chords, aliases and device ordinals. Both original controller
axis formats are accepted, including the old roll/throttle index swap. The
installed default imports 104 supported mappings. The installed default mouse map uses left-button guns,
middle-button missiles, right-button center targeting and wheel throttle.

Joystick / gamepad → Multiple controllers / original input maps provides six
independent routes: yaw, pitch, roll, throttle, look direction and command hat.
Each route selects a device slot and a physical SDL axis or hat, with inversion.
Device assignments can pin slots to connected controller identities. Button routes
support simultaneous devices and multiple actions on a button; negative imported
button indices select the original command-hat directions. The look hat remains
independent. Up to 16 device slots, 64 buttons per device and 512 routes are retained.
Mappings compile on configuration or hotplug, without parsing or allocating in
frame sampling. Single-controller mode retains standard SDL gamepad mappings.

Review device and axis assignments after a Windows import. The initial mapping
uses X/Y/Z/Rz/POV as axis 1/2/3/4 and hat 1; actual Linux axis order depends on the
hardware, and Windows device ordinals need not match Linux enumeration. The saved
Windows value number remains visible while remapping. Duplicate controller GUIDs
get separate identities that remain stable when another device disconnects during
the session; identical hardware can still require review after a restart.

Audio / feedback changes live sound category gains. Interface preferences sets
separate cockpit/chase and command radar detail and independent pilot/command
grid preferences. Appearance
preferences toggles skies, planets, stars, ship lights/trails, lens flare,
bidirectional lighting and center/target/message instruments. In the classic UI,
the mouse option enables or disables the original accumulated virtual joystick;
keyboard and physical joystick controls remain usable with the cursor visible.
Space dismisses Team/Game State/Teleport panes and enables mouse steering, releases
a fresh Investment pane's pointer ownership, or toggles mouse steering otherwise.
The free cursor selects friendly command recipients and issues orders from pilot
and command views. Inversion, acceleration and a scalable turn indicator retain
the original behavior. Virtual joystick holds the turn rate until the mouse moves
back toward its center; opening menus clears that steering. The optional modern
UI also supports direct relative mouse movement.
Button and wheel assignments can be rebound to any supported action. Buttons held
across a menu or focus transition remain suppressed until released. Wheel motion
accumulates fractional detents; zoom assignments change the cockpit field of view
or proportional external-camera distance. Cockpit zoom bottoms at 5 degrees and
returns to the configured field of view. Command-view wheel zoom keeps its original
20 percent per-detent geometry and is independent of frame time. Startup movies
plays credits before the intro and clears the intro preference after that start.

F8 opens station teleport while docked; Shift+F8 toggles the event log.
Page Up/Down pages messages in the shipped map, including while composing chat.
Older native profiles may retain Control+Page Up/Down. The list retains 512 messages and returns to the
newest page after ten seconds. Control+period toggles the grid and height lines
in the active view. Pilot and command preferences are saved separately; command
view defaults to enabled. Print Screen saves a PPM under the selected profile's
`screenshots` directory. GPU readback happens only for requested screenshots.

## Installation and diagnostic entry points

```sh
cmake --install build/linux --prefix "$HOME/.local"
```

This installs the executable, six SPIR-V shaders, and a desktop entry. The
executable first searches for shaders beside its installation prefix and falls
back to the build directory for development. Game artwork is not redistributed;
Steam's default Artwork directory is discovered automatically, or use `--artwork`.

`--practice` bypasses the menus. `--scenario 0` through `--scenario 5` start the
listed scenarios directly. `--menu home|scenarios|lobby|teams|lobby-pilots|settings|controls|controller|controller-map|audio|interface|chat-preferences|visuals|mouse|mouse-buttons|movies|online`
opens a screen for visual inspection. `--frames N --screenshot FILE` captures the
last frame without capturing the mouse. `--frontend-smoke-test` exercises menu
hit testing, settings, lobby launch, hull/equipment and cargo selection, leaving/restarting,
and keyboard configuration. Use a temporary `--config` path for that test.
`--docking-smoke-test` runs an actual flight out of a station, autopilot return,
automatic hangar transition, refit and relaunch under the Wayland frontend.

The session integration test loads every map and verifies configuration round
trips, bindings, ready-state behavior, AI teams, compatible equipment replacement,
flight exercise completion, a skirmish win, and bounded particle lifetimes. It
also compares exhaust density at 30 and 165 simulation updates per second.
All six supported hulls are exercised through launch, boosted departure, return,
dock and refit. A spatial check compares the MDL/CVH attachment coordinate
conventions and rotated model transforms against the original code.
The expanded session test passes AddressSanitizer and LeakSanitizer. Mission
teardown now releases the original simulation's autopilot waypoint targets,
which could otherwise keep mutually targeting AI ships alive across sessions.

## Shared camera and thruster behavior

`src/ClientCore/CameraMotion.h` contains the camera state and math extracted from
WinTrek's `CameraControl` and `ComputeExternalChase`. Both clients call it; the
Windows class retains its engine/target-geometry adapter, while the native client
feeds the result to Vulkan and the OpenAL listener. It uses the original Vector,
Orientation, Quaternion and Slerp code, without bringing in D3D or the pane system.

The native client now has actual-afterburner-power cockpit shake, collision shake
from IGC's `PlayVisualEffect(effectJiggle)` callback, the original decay curve,
cosine/slerp camera transitions (two seconds into chase, one into cockpit),
velocity/length/zoom-dependent chase placement and turn-rate lag, head look, and
near-aleph FOV compression. The HUD model preview uses the shared target-relative
camera orientation, including roll. Cockpit steering is immediate. Shake does not change
the simulated ship orientation, and native camera noise has its own random stream.
Paused frames retain the same camera sample. Launch, recovery and sector jumps
reset chase history. The selected FOV setting remains the baseline for zoom.

V switches cockpit/chase; hold keypad 4/6 for left/right, 2 for rear, and 7/9/1/3
for diagonal views. Releasing restores forward view. Page Up/Down change chase
distance. The ship stays visible while a return-to-cockpit animation finishes.
Motion debris now samples the rendered camera, retaining world positions and
constant angular size as FOV changes.

The old chase ring could walk into uninitialized history or divide by repeated
timestamps, and its partial-interval integration was incorrect. The shared
implementation uses bounded initialized history and trapezoidal prefix integrals
with a binary search, avoiding a per-frame walk through the delay window. It also
fixes animation cancellation assigning an unused quaternion instead of restoring
the destination orientation. These are intentional corrections, not attempts to
reproduce those defects.

`src/ClientCore/ShipSound.h` shares ShipSoundSite's thrust/turn response, slew limits,
gain curves and pitch calculations. OpenAL now plays the player's authored cabin,
main engine, maneuvering, afterburner and side-thruster loops, selects interior or
exterior assets with the camera mode, and uses the model's engine attachment in
external view. Identical source parameters are cached to avoid redundant OpenAL
updates. This replaces the former single synthetic throttle-controlled engine loop.

`native-camera` checks the shared math, 30/165/1000 Hz chase histories including
wraparound/repeated ticks, and actual IGC boost/release, collision, view transition,
head-look and sound behavior. `--camera-smoke-test --validation` exercises these
paths through Wayland/Vulkan/OpenAL. The Linux code and shared headers have been
built and tested; the changed Windows adapters have not been Windows-build tested.

At the initial camera checkpoint, all eight CTest checks passed, and the integration checks were clean under
AddressSanitizer/LeakSanitizer. A 6,000-frame camera/boost/audio exercise with
Vulkan synchronization validation on the local 165 Hz display completed without
validation errors or pacer misses. The full GPU sanitizer exercise still reports
the previously observed two 256-byte graphics-driver shutdown allocations; no
address errors were reported.

Launch/death cinematic views, command/target/missile/orbit cameras, turret
occupancy, controller hats, other ships' continuous audio and the original sound
graph are integrated. Remaining equivalence gates are tracked in the active audit.

## Effect rendering

World effects use 72-byte quad instances in persistent per-frame buffers, one
instanced draw after opaque geometry, depth testing without depth writes, and
additive compositing. Original projectile/decal textures retain directional
width and animated-strip frames; original explosion strips run at 20 fps.
Model attachment frames now supply exhaust, engine glows and trails. Smoke/fire,
damage smoke, shield-hit sprites, explosion bursts and fading trail segments use
a reusable pool with a 1,024 / 2,048 / 4,096-entry minimum by detail setting,
growing its budget with the fleet (16 / 32 / 64 entries per ship). Draw buffers
grow as required; the former 8,192-particle draw cutoff is removed. Emitters run
on elapsed simulation time. Exhaust and damage emission use projected detail
for distant ships while preserving the original 60 Hz cadence nearby. Effects are sector-filtered; projectile art is
preloaded with equipment to avoid first-shot uploads.

The original `ThingGeo` artwork half-turn is applied to meshes and attachment
frames, while camera, flight controls and collision data stay in IGC coordinates.
Exhaust follows the world-space backward thrust vector. Trails retain the wing
axis at each endpoint, including roll, using the same 72-byte draw instance;
they no longer rotate to face the camera. Nearby motion debris uses the original
texture and world-space positions, distance-based emission, and a two-second
lifetime. Turning the camera cannot rotate or create that debris. Sector jumps
reset emitter history. Cockpit view uses the hull's original cockpit offset.

The spatial audit found authored mesh/CVH radius differences (Neptune's `ss27`
is 30.6818 versus 29.5942). The native client preserves the original separate
radius normalization, so this is not a claim of pixel-exact collision surfaces.

After these changes, a 10,000-frame combat run on the local 165 Hz Wayland
display (excluding 1,800 warmup frames) reported 164.959 Hz mean presentation,
1.692 ms median / 4.039 ms p95 sample-to-presentation, and 0.039 ms median
sample-to-submit. There were two new pacer misses in 8,198 measured
presentations. This measures compositor feedback, not physical input-to-photon
latency. The effects remain one bounded instance batch with no additional draw
call for ribbons or motion debris.

Curved shield lenses, typed explosions and shockwaves, teleport emission,
construction shells and mining bolts now use shared original equations. Alpha
models and additive particles share stable depth ordering when needed; a
pure-additive pass avoids sorting. The renderer reserves its scratch storage.
Conquest and narrated training continue simulation while ordinary live menus are
open; the explicit pause screen freezes the local session. Contacts and world
art obey original side visibility. Full visual equivalence and terminal-event
coverage remain under the active audit. Native multiplayer now uses a separate original-IGC authority; its parity and latency policies remain under integration audit.

## Completeness audit

See [the source-backed audit and execution queue](native-equivalence.md) and its
[generated callback inventory](native-callback-audit.md). Shared launch, offline
aleph transit and inventory behavior now have original-reference comparisons and
actual IGC depletion/transit tests. These also found a native pack-definition ABI
bug, now fixed without altering the Windows structures. Visibility, lifecycle and
mission/economy and terminal-state coverage remain active acceptance gates.


Crew / turrets opens the friendly pilot roster. Equipped, unoccupied turrets can
be boarded while both players are docked; an aboard gunner can cycle seats in
flight. The pilot controls launch, docking and loadout and can promote a gunner.
Disembarking is available after docking. Mouse/steering controls slew the turret;
throttle changes its field of view and slew rate. Gunfire uses the parent ship's
original mounted weapon and ammunition. The Conquest lobby can create additional real local pilot seats; `--crew-smoke-test` creates a second local pilot for the integrated
crew exercise. This is not a substitute for the deferred remote-player protocol.

V toggles cockpit/chase; Alt+V cycles external, target, flyby, orbit, station
tracking and missile modes. Alt+arrows and roll keys rotate an external camera;
Page Up/Down adjusts its distance. The geometry is shared with WinTrek.

## Cargo and saved loadouts

B opens cargo/equipment during flight; G selects the next distinct stack, R
manually reloads reserves, and Delete drops the selected stack as salvage.
Click a cargo HUD row to select it. The menu mounts or swaps compatible reserves,
unmounts into empty cargo, and retains the original equipment arming delay.
R still recovers to the hangar after destruction. These bindings are configurable.

Hangar → Saved loadouts provides ten banks. Each bank keeps one setup per hull;
it selects the current hull's setup if present, otherwise the bank's most recently
saved hull. Save replaces that hull's setup; Fit trades the current equipment and
buys the saved setup using available station technology and credits. Unavailable
parts use the original predecessor/successor rules; an unavailable or unaffordable
hull falls back to a purchasable default. Practice equipment remains free.
Clear bank removes its saved setups.

Files are `loadouts_<core>.dat` beside the selected `native.conf`, using the actual
mission core (for example `loadouts_PCore014.dat` or `loadouts_ac_20.dat`).
Their binary format matches original Windows loadout files, which
can be copied to this location for the same static core. Malformed files are
reported without silently discarding or overwriting them. `--config FILE` also
isolates the loadout directory for tests.

`--cargo-smoke-test` exercises actual menu selection, gun swapping, manual reload
and salvage drop. `--loadout-smoke-test` saves a bank, changes hull, fits the saved
ship and reopens its file. Use a temporary configuration directory for either.

### Local mission goals and scores

Conquest lobby **Mission goals and rules** configures conquest, territory, flags,
artifacts, kills, prosperity and time limits. Its economy page controls starting
credits, He3, initial miners and friendly fire. Editing rules clears readiness.
Original IGC map objects, production and collisions drive these objectives; the
HUD shows carried objectives and progress. Pause and results offer team scores.
Finished missions remain in the results/menu flow until leaving the session.

Carrier landings now use original server resupply in Conquest and original offline
cached refits in practice. Conquest replenishes free equipment under team technology
and retains existing quantities of paid equipment. Practice restores the cached
outfit. Both launch from original authored carrier geometry after repair.


Pilot commands (O or Pause → Pilot commands) show queued, accepted and current
orders. Insert accepts, Home accepts and executes, End rejects, U selects the
accepted order, F11 clears it and Y matches target speed. Selecting a different
target preserves the accepted order. All targeting categories from WinTrek are
available in keyboard configuration; Tab cycles similar targets, comma chooses
the nearest, period the previous, L uses the reticle and F12 targets an attacker.
Role, base, rescue, objective and aleph searches can also be assigned keys.


Configuration → Joystick and gamepad provides device, axes, inversion, dead zone,
response, hat, trigger and button assignment controls. The default gamepad uses
the left stick to turn, right-stick X to roll, triggers for guns/missiles, the
D-pad to look, South to target, West to cycle guns, North to boost, East for vector
lock, and Start for the menu. A flight stick defaults to axes 1–3 for turning/roll,
axis 4 for throttle, hat 1 for look, and its first buttons for guns, missiles,
boost, cycling, vector lock, targeting, autopilot and chaff. Change these mappings
for the device as needed. Save configuration to apply edits.

Gun keys 1–4 select a gun; pressing the selected gun again groups the weapons.
0 toggles group fire while retaining the selected gun for lead. Next/previous gun
commands can be bound to a key or controller button. Throttle presets are 5–8;
W/S use the original throttle steps and repeat rate, configurable on the keyboard
bindings screen.


## Original narrated training and command view

Home → Original narrated training opens the seven authored missions. The menu
plays the original introduction, runs its original IGC world/actions/conditions,
and plays the debrief on natural completion. Any key/click advances a slide;
Escape skips the sequence. Pause freezes the lesson clock and audio playback.
`--training 2` starts directly in a lesson; add `--training-intro` to include slides.

Command view supports left-click and rectangle selection, Shift-click group
editing, group orders, the order panel, right-click orders, and right-drag height
for a point in space. Click a sector in the map to change
view; F6 opens team orders and Take Me To. Arrow keys or screen edges pan,
plus/minus zoom, and roll bindings rotate the plane. F2 enters command view;
Shift-F2 switches its orientation during training. Original training restrictions
remain in effect. Lessons 2, 3, 4 and 5 have input-driven complete walkthroughs. Both repair-lesson outcomes are exercised through original mission behavior, including an input-driven escort victory. Arena/free-flight wave completion is checked separately through real IGC damage/death events; a full input-driven arena victory is still an optional diagnostic.


## Authored sector visuals and remaining lives

Sector skies, planet posters, seeded stars with motion streaks, lens flares,
authored model strobes, projected-size LOD and original primary/alternate/ambient
lighting are now rendered. Command view includes the original sector plane grid,
north marker, object height lines and waypoint height feedback.
Stars use the scene's perspective and camera rotation, with no translation
parallax or rear-hemisphere mirroring. Newly visible stars start without stale
motion streaks.

Conquest mission rules include deaths allowed. An exhausted pilot retains the
original ghost state and can observe/command while another teammate survives;
launch, refit, loadout restoration and boarding are blocked. Losing the last
live pilot eliminates that Conquest side. Allied stations rearm and relaunch
crewed ships; an allied station rescues a pod to its last own station or an own
restart fallback. This preserves identities and gunner relationships.

`--command-smoke-test` exercises rectangle group selection and a raised waypoint
through actual SDL events. `allegiance-asset-audit` checks the installed `cores.txt`
catalog, both default cores, all supported
local maps and every mapmaker sky/planet variant. The suite contains 35 headless checks; completion and compatibility claims remain governed by
[the active audit](native-equivalence.md).


Interface and camera bindings are now configurable alongside flight actions:
168 keyboard/controller actions cover HUD panes, typed/quick chat, commander
views, research, look, zoom and camera modes. The `X` beside a binding clears it;
assigning another action's key exchanges the bindings. Cleared bindings persist.
F10 targets friendlies in flight and does not quit from menus. Use the menu's
Quit control or close the window. Training help/inventory prompts follow remaps.

Station launches and carrier rearm show the original three-second tracking view.
Death shows the original five-second frozen view before pod control or training
restoration. Hangars play original station ambience, and incoming missiles use
the original strongest-lock warning pitch. Launch/cinematic/training sound
perspective follows the camera actually displayed.

`build/linux/src/native/allegiance-render-check` performs explicit Vulkan readback
checks for alpha/additive ordering, depth occlusion and the additive fast path,
plus transparent full/partial missile locks and other expanded radar brackets
at native and doubled size.
It requires a live Wayland/Vulkan session and runs separately from headless CTest.
Authored arena/free-flight checks cover every wave through real damage events;
this verifies their lifecycle and victory cues, not an input-driven combat win.

Typed and quick chat offer all pilots, team, sector, wing, selected pilot,
allied teams, friendly sector, ship crew and commanders. Tab cycles recipients.
Friendly-sector delivery reaches flying pilots; sector chat includes docked
pilots. Ship chat falls back to the team when there is no crew. Commander delivery
follows leadership handover. Team, wing and friendly-sector commands reach local
human seats and await acceptance; AI commands use individual recipients. Public
broadcast channels retain the original server's restriction on commands.

Orders and money requests share the original queued-request controls. Accept or
execute approves a money request using up to the available balance; reject sends
the original negative reply. A new money request replaces a queued order and a
new order replaces the request. A different sender replaces it silently. After
fifteen seconds, money requests disappear and orders lose their acknowledgement
sender while retaining their target/command for late acceptance. This also holds
for inactive local seats. Departing identities lose pending reply tracking.
Financial approval remains available with a partial or zero balance, including
through the pilot-command and roster menus; completed sessions disable it.

Original critical order/recovery tips and financial prompts are restored. The
queued-request prompt is independent of the center HUD and works in command view;
its key legend is cached when bindings change. Posted prompts use current keys,
with distinct approve and execute wording. `--team-smoke-test` covers menu and
keyboard financial approval in flight with the center HUD disabled.

Sonic-chat IDs retain the original `sonicChats` table values, independently of
native audio-template IDs. Distinct messages can share one recording while keeping
their own text. The complete 310-ID table is available, including entries outside
the 307 menu commands. Voice-variant lists are preserved; the receive path uses
variant zero, as the original client does. Audio registration and missing-wave
fallbacks do not renumber logical chat IDs. Unknown out-of-table sonic IDs display
“Unknown chat” silently when their filter is disabled; explicit text with an audio
ID keeps the separate original direct-sound path.

Interface configuration includes chat preferences. Filtering all-pilot messages
discards them; filtering human quick-comms voice retains its text and orders and
uses the normal notification cue. Unknown human voice IDs can be filtered.
Headquarters and explicit non-quick spoken text retain the original exceptions.
Pilot accounts expose Mute/Unmute independently of commander authority, including
at results; muting discards that pilot's text, voice and orders. Mutes last for the
local session, and a departing identity does not retain a mute for a reused ID.
Typed `!mute pilot` and `!unmute pilot` commands use the original exact-name,
case-insensitive lookup and execute locally, independently of chat recipient.
Unknown leading-`!` commands are consumed locally, as in WinTrek. The shell
retains the original self-mute exception; the roster action excludes self-mute.

Chat labels include sender and recipient, including authored wing/team names.
Unvoiced orders describe their verb, target and visible sector. Outgoing echoes
retain the original audience and sound cue without queueing a remote order on
the sender. History uses WinTrek's selection predicate and 600-record capacity;
rapid identical chats keep independent records. Native's event log intentionally
combines chat with local notifications. Original side tint is cached on arrival.

The optional display censor uses the original dictionary and substitutions. Raw
messages and routing names remain intact. Apply updates displayed history without
changing its identities; the original JSON migration now covers 33 preferences.
For `FilterChatsToAll` and `FilterQuickComms`, migration preserves WinTrek's actual
startup toggles: a saved false enables filtering, while saved true leaves it off.
The native profile saves effective values directly.

Long messages wrap at UTF-8 boundaries instead of clipping. The event log pages
by rendered lines, retaining its anchor as messages arrive. Layouts append only
new records and reflow on font/scale or censor changes; unchanged frames reuse
resident spans. Paging returns to newest after ten seconds of monotonic UI time,
independently of paused simulation. Critical tips use the original 7.5-second
lifetime or 30 seconds in training; ordinary text/history persists until replaced
or paged away. `--chat-smoke-test` exercises wrapping, historical anchors, live
censor changes, font resize and unchanged-frame layout reuse on the real renderer.

Completed local sessions stop at the original `STAGE_OVER` mission stage. Team
rosters and accounts remain inspectable, with credit, leadership and crew actions
disabled. Gameplay operations also reject changes at the domain boundary, including
refits, orders, travel and launch. Text chat remains available.

## Local teams, pilots and accounts

Conquest supports all fifteen original generated-map types and two through six
teams. Invalid original map/team combinations are rejected. Teams selects each
civilization and alliance group. An alliance must include at least two teams
and leave an opponent; invalid or all-united groups remain independent, following
original FedSrv policy. Mission goals and rules also configure resources,
starting stations, development paths, allied sight/ripcord, treasures and the
start countdown. Territory 100 is the original disabled sentinel.

Pilot seats adds, names, assigns, readies and removes offline pilot identities.
They exist at their side's station before the original initial payday. In-game
Team pilots opens accounts, credit transfers, automatic donations, money
requests, wings and commander handover. A local seat can be controlled from its
account page. The commander can remove another local pilot; its account and
ship value return to the side, incoming donations move to the replacement leader,
and original integer payday remainders remain in the side pool. Allied rosters
retain private credit balances and visibility-filtered locations. These local
controls exercise real IGC relationships; they do not implement remote joining.

Controller configuration includes force feedback / rumble. The native backend
translates all nine installed `fire.ffe`, `bounce.ffe` and `explode.ffe` effects
into SDL waveforms/conditions, retaining timing, envelopes, phases and delays.
Devices use supported effects or the original fallback profiles; vibration-only
controllers receive prioritized gun/impact/explosion cues. Focus loss, menus,
disable and device teardown stop feedback. Physical force-feedback hardware has
not been verified on this machine; virtual rumble and preset translation are tested.

Movies pause on focus loss and preserve their final frame duration through audio
completion. The FFmpeg worker has bounded queues and wakes on skip/teardown.
World audio is suspended while movie PCM plays under the configured master volume.
`--seed N` reproduces a local simulation seed. The optional long arena pilot
replay is `allegiance-training-check ARTWORK 6 --arena-input` (or mission 10);
it remains an incomplete automated combat walkthrough.


## Station teleport

Teleport to a base lists your team's and allied restart stations with their sector
names, marking the current station and whether the ship can be retained. A retained
ship carries its gunners. If the destination cannot support the hull, original
server policy releases the crew at the origin and refunds the ship; the native
client then purchases the cached/default equipment permitted there. Passengers
can disembark and travel independently. Exhausted-life observers retain command
view; teleport cannot grant another life. Authored narrated lessons retain their
own station flow.

`--teleport-smoke-test` exercises travel on the original generated Split Base map,
checks identity/loadout/credits and station IDs, toggles the UI by keyboard, and
captures a frame with Print Screen. Use an isolated `--config` profile. The
startup movie smoke similarly expects `startup_movies 1 1` in its isolated profile.


## Authored training endings and input diagnostics

The original training destructor selects its debrief unless the mission explicitly
skips it. That policy applies to both goal completion and arena death. The native
result text therefore says "Training ended" (or "Training ended: ship destroyed")
and a separate state flag controls the debrief. Arena death retains the original
five-second frozen camera and does not grant a new life. Debrief completion and
explicit lesson exit return to the original training menu, where another lesson
can start in the same process.

`--mouse-input-smoke-test` loads the installed original default map in memory and
sends SDL button/wheel events through targeting, throttle and missile launching.
It then tests custom weapon-cycle/zoom assignments and opens the mouse assignment
screen. `--training-terminal-smoke-test` exercises arena death, debrief skipping,
training-menu hit testing, a fresh basic-flight introduction/lesson and explicit
exit. Use isolated profiles for both.


Station capture/destruction launches living pilots together with their crew;
exhausted observers move to another own restart base. Losing the final restart
base applies original human/crew reset and retains pilot identities and accounts.
In continuing multi-team matches, an eliminated team's accounts are inspectable
and local mute preferences remain editable, while gameplay/request actions are
disabled. The surviving teams continue their original simulation.

The eliminated-team HUD offers team scores and the session menu. Those views keep
match completion separate from the controlled team's elimination; score inspection,
configuration and leaving remain available. Inactive teams cannot refit, launch,
return to a base or approve requests. Original narrated training uses a different
side lifecycle and retains its authored controls.

`--elimination-smoke-test` loses both own Split Base restart stations before a
simulation update in a three-team Conquest session. It verifies observer/crew
cleanup, continuing authority, HUD score/menu hit testing and disabled flight
actions. Use an isolated `--config` profile.


## Native multiplayer

Start the authority in one terminal:

```sh
build/linux-release/src/native/allegiance-server --port 27500
```

Connect in another terminal, or use **Multiplayer** from the main menu:

```sh
build/linux-release/src/native/allegiance-native --connect 127.0.0.1:27500 --team 1
```

The first connected pilot hosts the mission lobby. Choose teams, open Mission
setup to change the map/civilizations/alliances/rules, mark each pilot ready, then
Start match. After the countdown, select equipment in the hangar and Launch.
Team numbers on the command line and in the menus start at one. Use an actual
server IP instead of `127.0.0.1` on another machine; numeric IPv4 or bracketed IPv6
addresses are accepted. Automatic LAN discovery is available; hostnames are not
yet resolved.

The original Games List's **New Game** dialog also creates missions on a selected
native authority. To add a server to that list without joining its entry mission:

```sh
build/linux-release/src/native/allegiance-native --server 127.0.0.1:27500
```

Select the server and supported core, enter a name and choose OK. Both lists
start without a selection, as in WinTrek; Return in Create Game does not press OK.
The client
joins the created mission as its owner. The explicit Local game row still
creates an offline mission. Each authority hosts up to sixteen missions,
including its entry mission, and uses available UDP ports from its entry port
through the next fifteen game slots, skipping the reserved LAN discovery port
27501. Allow ports 27500–27516 for the default setup; a port near 65535 reduces
the available capacity.
Authorities advertise the cores listed in their installed `cores.txt`; selecting
a core creates the actual mission with that core's civilizations and equipment.
The local row also offers the default `static_core`. Optional `allegiance.txt`
`[Cores]` names, `[OfficialCores]` and `[OfficialServers]` badges use the original
UI rules. Official-server addresses identify the host independently of the
native mission listener port.
Unclaimed creations expire after
thirty seconds; empty used missions retire after reconnect-grace seats depart.
Public global directory registration is still pending.

Client and server must be built together from the same source/options and use
matching installed `.igc`, `.mdl` and `.cvh` data. Their exact identities are checked
before joining. The default Steam Artwork directory is detected; both executables
also accept `--artwork PATH`. This connects native clients to the new native
server, not to original Windows FedSrv servers.

The server defaults to original-core Conquest with two teams. `--teams`, `--seed`,
`--countdown` and `--artwork` configure its initial session. `--auto-start` skips
the readiness lobby for diagnostics. State publication is currently compiled at
10 Hz, with simulation and input retained at 60 Hz. The compile-time
`ALLEGIANCE_SNAPSHOT_HZ` definition in `network_records.hpp` sets publication and
the client's baseline interpolation interval together. Host settings can be changed in the lobby,
and the host can create another round from the results screen. Lobby team changes
recreate all replicas with the updated match; host migration and team commander
handoff are distinct. Brief connection failures attempt to resume the same pilot
and account for up to 30 seconds. Explicit Leave retires the pilot.

The server can host the existing mixed-hull combat diagnostic:

```sh
build/linux-release/src/native/allegiance-server --scale-test 512 --port 27522
build/linux-release/src/native/allegiance-native --connect 127.0.0.1:27522 --team 1
```

Each side starts with 128 Interceptors, Enhanced Fighters, Advanced Stealth
Fighters and Scouts, using the existing opposing blue-noise spawn discs. Combat
waits for the first client, then uses the server's normal fixed 1/60-second
simulation steps. At peak load this diagnostic logs and sheds wall-time backlog
without skipping simulation steps or terminating the match. The initial seat is
docked; F2 opens command view directly from the
hangar, without introducing a player ship for the wingmen to follow. This is an
open-ended skirmish with the existing practice AI replacements, not a Conquest
match. Ordinary server startup and readiness lobbies are unchanged.

GameNetworkingSockets is found as an installed CMake package or fetched at the
pinned commit. Building that dependency also needs its normal OpenSSL, protobuf
and LZ4 development dependencies. The transport adapter is isolated from gameplay
and replication. The [equivalence audit](native-equivalence.md#native-multiplayer-implementation-2026-10-01)
records the architecture, verification and remaining limits.

Network integration checks use real localhost UDP:

```sh
build/linux-release/src/native/allegiance-network-check
build/linux-release/src/native/allegiance-network-check --impaired --scale
build/linux-release/src/native/allegiance-network-check --case cadence
```

The longer run adds loss/delay/reordering, a 1,500-ship publication and 100 separate
connections/replicas. `--case normal|crews|lobby|projectiles|drops|effects|presentation|flight|inputs|waypoints|drones|traffic|construction|impaired|fleet|peers|cadence`
selects one case. The flight case checks throttle/braking, vector lock,
afterburner power/fuel/camera/particles, short input presses between simulation
ticks, and authority/prediction clock equivalence. It also checks the installed
Conquest stock Scout: no fitted afterburner means the boost key requests full
ordinary engine thrust without fuel consumption or afterburner effects.
The inputs case checks square-root byte precision over the complete control
ranges, exact canonical replay, modulo counter wrap, client-side cockpit/dead-zone
policy, docked input suppression and reconnect. Its raw UDP pilot deliberately
loses two sends, duplicates/reorders recovery bundles, sends a stale match,
continues receipts without input, and supplies malformed commands to check bounds
and the lost-input watchdog.
The waypoints case reproduces snapshot retirement of a commanded buoy, checks all
four command slots, identity/visibility, shared ownership, ID reuse and off-screen
plan targets, then runs four miners competing for two He3 rocks while the pilot
autopilots to an advanced Supremacy. It repeatedly replaces private detours and
checks reconnect and cleanup. The construction case also checks that command
import preserves authoritative drilling/building state.
The traffic case checks byte-exact sparse/identity-aligned deltas and complete
record compression, bounded idle-lobby publication, acknowledgment-based effect
retirement and reconnect. The projectile case also checks the exact original
millisecond deadlines and already-aged countermeasure state. The effects case
checks that explosions age and expire while the pilot is docked in command view,
and that later snapshots do not restart acknowledged effects.
The construction case funds and produces Light Outpost and Supremacy constructors,
issues build orders, and follows their original drilling and forty-second shell
animations through completion. It checks existing and late-joining replicas during
the final fade, and verifies that destroying the site cancels construction.
The presentation case runs a 165 Hz camera against 60 Hz
prediction and the compiled publication rate, both locally and under
loss/delay/reordering. The cadence case measures matched real-time
windows in a docked sector, with two cruising/firing pilots, and with 100 extra
coasting ships. It excludes bootstrap, launch and fleet-creation bursts from its
per-second measurements.
It evaluates poses for a display target 2 ms after input sampling and checks
8/24 ms target changes, fallback and reversible projection separately. The real
frontend uses the pacer's expected display time; it falls back to sample time
until feedback establishes a schedule. Remote buffering still applies relative
to that display time. `--scale-csv` includes pose sample/target timestamps and
the local/remote presentation horizons for inspecting this timing path.
These checks do not use the renderer. For an automatic real
Wayland/Vulkan lobby-to-flight check, start a server with `--countdown 1`, then run:

```sh
build/linux-release/src/native/allegiance-native --connect 127.0.0.1:27500 \
  --network-smoke-test --frames 2500 --config /tmp/allegiance-network.json \
  --screenshot /tmp/allegiance-network.ppm
```
