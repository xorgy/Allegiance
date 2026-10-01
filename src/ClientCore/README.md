# Shared client behavior

These headers hold original Allegiance client behavior used by WinTrek and the
native client. They are not a replacement simulation or rendering architecture.
IGC continues to own flight, weapons, collision, damage and equipment objects.

| Header | Original source / responsibility |
|---|---|
| `KeyboardSteering.h`, `MouseJoystick.h`, `MouseInput.h` | Original keyboard axis signs/priority, mouse acceleration, circular virtual-joystick accumulation, mouse toggle/pane precedence and mouse/physical-axis selection |
| `GlobalKeyFilter.h` | WinTrek translated global-key dispatch/consumption, used by the native host after popup/key-capture priority and before ordinary screen input; WinTrek retains its existing handler |
| `HelpMotion.h` | Original HelpPosition half-second linear opening/closing and reversal, used by the native independent Help layer; WinTrek retains its existing image/filter handlers |
| `Joystick.h`, `JoystickHat.h`, `Throttle.h` | Original joystick/virtual-hat response, throttle steps/presets and repeat timing |
| `CameraMotion.h` | WinTrek camera interpolation, shake, chase history and target-preview orientation |
| `PlayerCommands.h`, `Targeting.h` | WinTrek queued/current/accepted orders, target speed and original target searches |
| `WeaponSelection.h`, `EquipmentStatus.h` | BaseClient gun cycling, WinTrek single/group fire and mounting/arming states |
| `WeaponIndicator.h`, `AimIndicator.h` | WinTrek damage/healing target rules, lead relays, aim colors and angular sizing |
| `RadarModel.h`, `RadarPresentation.h` | Original treasure usefulness and per-object icon/label/statistics/range policy |
| `CameraFraming.h` | WinTrek target/missile tracking, flyby/orbit, command framing and launch/death geometry |
| `CameraZoom.h` | WinTrek cockpit field-of-view and proportional external/command zoom equations |
| `StationEvacuation.h`, `SideElimination.h` | FedSrv station-loss crew launch/observer relocation and hull-safe ship cleanup, forced deaths and human identity retention |
| `StationTeleport.h` | FedSrv own/allied restart authorization, retained crew and unavailable-hull refund/pod transition |
| `SoundIds.h`, `ClientSoundEvents.h` | Original sound IDs, countdown/chat/station announcements and contact acquisition policy |
| `ShipCrew.h` | FedSrv equipped/unoccupied turret selection, disembark and WinTrek turret zoom |
| `ShieldFlare.h` | Original curved/elliptical shield-hit orientation |
| `AlephEffects.h` | Original funnel vertices, LOD, UV motion and warp-bomb star swelling |
| `ExplosionEffects.h` | Authored explosion profiles, image sequences, stagger, scatter and shockwave fade |
| `SurfaceEffects.h` | Mine-field density/brightness and hull-impact frames/scale |
| `ParticleEffects.h` | Original particle appearance, trajectories, teleport emission and mining bolt subdivision |
| `ShipSound.h` | ShipSoundSite engine response, incoming-missile/weapon/warning/countdown policy, impact/explosion and drone-alert selection |
| `CombatActions.h` | BaseClient missile launch vectors and offline chaff/mine/probe creation |
| `Inventory.h` | BaseClient transfers, mount swaps, manual reload and WinTrek cargo traversal/selection |
| `Visibility.h` | WinTrek/FedSrv side sight, retained discovery and scanner registration |
| `ShipLifecycle.h` | BaseClient/FedSrv salvage and pods; carrier/ripcord/resupply, restart stations and boarding authority |
| `CarrierLoadout.h` | FedSrv free-equipment/technology rearm and BaseClient offline cached refit |
| `Loadout.h` | BaseClient saved equipment successor/similar-part purchases and upgraded-hull defaults |
| `LoadoutPresentation.h`, `LoadoutMetrics.h` | Original raw equipment statistics and loadout gauges calibrated from the active MDL assets |
| `MissionEconomy.h` | BaseClient purchases/default cargo; FedSrv development, upgrades and construction |
| `MissionRules.h` | FedSrv station/territory/kill victory, timeout tie-breaks, elimination, alliances and live/ghost player counting |
| `ShipOrders.h` | FedSrv default orders, miner command memory and sector-arrival command cleanup |
| `PlayerEconomy.h` | FedSrv individual paydays, donations, money requests, commander voting and departing accounts |
| `GameListPresentation.h` | Original Games List time labels, creation name limits and default callsign/rank prefix |
| `CoreCatalog.h` | Original `cores.txt` count/basename format and case-insensitive supported-core lookup, used by native creation and discovery |
| `ObservedCredits.h` | Original PlayerInfo partial allied money semantics, with cumulative native report baselines and account generations |
| `Alliances.h` | Original active-side alliance normalization and group mutations |
| `LobbyReady.h` | Original team readiness, minimum-player eligibility and lazy force-ready handling |
| `ClusterWarnings.h` | Original asset-warning transitions and team/wing alert selection |
| `FlagLifecycle.h` | FedSrv flag/artifact drop, respawn and scoring |
| `CommandPicking.h` | WinTrek rectangle selection, command-height picking and sector grid spans and camera/preference visibility |
| `ModelLod.h` | Original strict projected-radius mesh selection |
| `Strobe.h` | Authored model light flash envelope and bounded phase advance |
| `StarField.h` | Original folded star projection, motion streaks and private Windows seed sequence |
| `QuickChat.h` | Original quick-chat target resolution and command semantics |
| `ChatRouting.h` | Original server broadcast-command and same/allied-side recipient gates, used by native local authority |
| `ChatPreferences.h` | WinTrek pilot mute/public/unknown discard and human quick-voice filtering, used by native delivery |
| `ChatCensor.h` | Original display dictionary and substitution loop, shared with WinTrek's existing prepared table |
| `ChatPresentation.h` | Original history selection/capacity, side tint and local mute/unmute command parsing |
| `QueuedRequests.h` | Original order/money rejection, reply ordering, timeout lifetime, partial approval and rescue-tip selection |
| `MessageLifetime.h` | Original critical-tip duration, including the longer training lifetime |
| `FlightFeedback.h` | Original rescue tips and ripcord order selection/activation captions |
| `CombatFeedback.h` | Original kill/ejection roles, drone-loss cues, station-loss captions and flag-pickup feedback |
| `WarpTransit.h` | WinTrekClient offline aleph exit kinematics |

Keep authority and platform adapters at the call sites. In particular, the combat
helpers do not send packets, decide online ownership or replace server logic.
Windows keeps its existing message definitions and writes directly into its
existing records. Reload callers retain empty-part termination, transport and AI
resupply policy. Aleph callers retain sector changes, sound/trail/BB updates and
player status. Audio adapters retain sound backend and world-spatial policy.

Extraction must preserve random draw order, time conventions, object identity,
reference lifetimes and original equipment/command semantics. Document deliberate
corrections separately. Do not convert Windows objects into native containers,
introduce a new virtual framework, allocate intermediate packet arrays or add
frame-wide work to WinTrek. Inline templates inject RNG or caller record types
without a runtime callback allocation. Native rendering/UI can consume shared
state while retaining Vulkan batching, modern fonts and its own layout.

See [the audit and queue](../../docs/native-equivalence.md) for verified behavior
and missing responsibilities. `native-actions` compares the new action helpers
with frozen original excerpts under `src/native/test_reference`; do not update the
reference to match production changes. Real native session checks then verify the
callbacks are actually reached by firing, depletion, docking and sector transit.
A Linux test pass does not establish Windows binary/performance equivalence.


`FlightControls.h` shares WinTrek autopilot-axis takeover, manual weapon and
expendable bits, cockpit FOV slew limits and client-only state masks. Native
training compiles the authored action/condition library and adapts its platform
services in `native/training_host.*`; it does not replace the missions with new
scripts. `CameraFraming.h` additionally shares command-view orientation, plane
panning and displacement bounds. `CarrierLoadout.h` includes both cached and
uncached original offline rearm paths, alongside the server rearm policy.

`Alliances.h`, `PlayerEconomy.h` and `LobbyReady.h` replace the original FedSrv
policy blocks at their event call sites. They retain original list order and
short-circuit rules; they add no per-frame Windows traversal or allocation.
Native local-seat creation, frontend countdowns, SDL haptics, Vulkan resource
caches and FFmpeg remain in native adapters. The native Windows-CRT simulation
RNG adapter is selected only by `ALLEGIANCE_NATIVE`; the Windows RNG path is
unchanged. Windows build/performance and original-build replay evidence remain
separate acceptance gates.

`StationTeleport.h` runs only at the original docking/teleport event. The Windows
adapter retains acknowledgement and money packet order, status updates and its
existing child/part lists; lambdas inline without heap storage. Sound event helpers
similarly remain at existing event sites. Dynamic-record import fixes preserve the
six-byte header format and advance across excluded records. These changes do not
introduce native rendering/input dependencies into WinTrek or FedSrv.
