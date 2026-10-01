# Original interface restoration

The original interface is the current target. Its authored artwork and MDL
descriptions compile into native controls and ordered Vulkan quads. This work
does not reintroduce the D3D/Pane engine or alter WinTrek's rendering path.
The experimental native frontend requires `--modern-ui`; it is never an
implicit fallback from the default game. Secondary commands whose authored
panels are not yet restored temporarily use the classic bitmap popup primitive.
Those adapters preserve gameplay access, but are not evidence of exact panel
equivalence; their authored counterparts remain in the queue below.

The classic interface follows SDL's per-window display scale automatically on
Linux, Windows and macOS. It uses the nearest whole-number scale, with midpoint
ties rounded down: 125% and 150% use 1×, 175% and 200% use 2×, and 250% stays
at 2×. Changing displays updates the scale and pointer transforms together.
`--classic-ui-scale N` overrides this with an integer 1–8; `auto` restores the
platform choice. The saved `classic_ui_scale` value is 0 for automatic scaling.
Window size alone never enlarges the UI. The requested whole-number scale
falls back to the largest integer that fits the authored canvas.
Small drawables crop rather than fractionally shrink the artwork. Menus center
on physical pixels, while flight panels retain their screen anchors. Bitmap
art, fonts and slides use nearest sampling. Linear world materials and nearest
UI descriptors share their GPU images and uploads. The authored Help keyboard
image retains its own 1300×700 canvas.

## Current evidence

* Main, training and hangar use their original backgrounds, button strips,
  positions and bitmap fonts at their authored 800×600 size by default.
  Black-key transparency, enabled/selected/hover/pressed faces, mouse capture
  and release-to-activate behavior are implemented. Focus loss cancels capture.
* Training selection and Launch Mission are separate actions. Double-clicking
  a lesson launches it. The original next-lesson progression, narrated missions,
  introduction slides and debrief flow are retained.
* The games list, team roster/lobby and keyboard mapping popup use original
  artwork and control positions. Lobby chat channels, editing, sending and
  retained lobby display work. The full roster scrolls with the original
  fourteen-pixel rows, columns, clipped hit regions and scrollbar. Left-click
  selects/deselects a pilot for private chat; right-click opens pilot actions.
  Unchanged remote lobby state no longer rebuilds its controls every frame.
  Network lobby rows retain the actual pilot IDs and host/commander/self/mute
  roles; duplicate names and reordered reports cannot redirect private chat or
  actions. Departures dismiss a stale popup and clear its private recipient.
  Original above-pointer player popups now expose Make Leader [L], Boot [R],
  applicant Accept [A]/Reject [R] and Mute [M]/UnMute [U], preserving
  roster/chat state on dismissal. Popup text capture
  prevents accelerators from leaking into the chat edit. Make Leader follows
  FedSrv's command-wing/role change without the separate handover action's
  transfer of money or automatic donations. The owning side's leader retains
  mission host controls. Boot moves a member to No Team and bans their return
  to that faction for this round. The physical Accept/Reject/Make Leader controls
  use the selected stable pilot identity and follow the original role visibility.
  Changing factions now updates membership in place instead of recreating the
  match: pilot IDs, other participants, world objects and round time survive.
  Departing leaders elect their old side's successor and hand over host controls.
  The authority/replica foundation for No Team uses IGC's existing lobby side
  (`SIDE_TEAMLOBBY`, -2) and partial Reset, including crew promotion, donation
  redirection and account refunds. Unassigned pilots receive public faction
  data in one shared arena and their own identity in a separate arena; they
  retain no former faction's scene, scanner map, equipment or command camera.
  Departure/re-entry, repeated departure, invalid IDs, continuing simulation,
  crew preservation, first-report research restoration and shared/private
  No Team arenas have synthetic regressions. Connected admission now supports
  pending requests, commander Accept/Reject, Auto Accept, JOIN TEAM cancellation,
  Lock Lobby/Lock Sides and running-match joining/defection policy. Applicants
  appear in the requested team's roster without gaining that team's visibility;
  their status flashes by updating resident text colors. The authored prompt
  reports the player's team or pending request. WinTrek's removed waiting modal
  is not restored. Personal Away and commander force-ready remain separate;
  force-ready never rewrites another pilot's Away state. Authenticated reconnects
  retain membership even when fresh late joining is disabled.
  Public pilot names/membership/readiness use a separate compact record from
  faction-private account/wing data. Running-match recipients share one cached
  public roster arena and encoding; unchanged rosters keep their revision.
  Paused lobby/full-bootstrap reports retain the existing reliable full-report
  path. The encrypted admission check covers Boot bans, locks, fresh hull-less
  applicants, cancellation, acceptance, force-ready, secure resume and original
  count-imbalance boundaries. A synthetic 1024-recipient check verifies one
  public-roster encoding. Native pilots currently have no authenticated Club
  ranks, so Auto balance treats them as unranked; ranked matchmaking remains
  separate work.
* Input mapping uses the original grouped action order, names and axis labels,
  shared with WinTrek through `ClientCore/CommandDescriptions.h`. Selection,
  Enter/double-click editing, multiple keyboard mappings, conflict confirmation,
  all-device deletion, isolated Restore/Cancel/Accept and the original plain-key
  versus chord modifier rule are implemented. Mouse buttons/wheel and joystick
  buttons/axes can be captured in the same popup. Axis travel uses the original
  0.9 threshold; cancelled edits preserve routes and device identities.
* Input and roster lists use original scrollbar arrow/thumb images, tiled thumb
  rendering, dragging, page clicks, half-second initial arrow delay and original
  repeat intervals. Wheel navigation moves selection and reveals it, as ListPane
  did. Glyphs, images and hit regions clip to the authored viewport.
* Original launch animation XOR/RLE frames decode before rendering. Scoped MDL
  imports preserve independent panel width/color/position definitions. Composite
  images, translated text, hull-stat gauges and crop coordinates now compile.
  `ImportImageLR` preserves the original top-anchored part-stat label backgrounds;
  image and text quads clip to authored animated-pane rectangles.
  Font and image resources share the renderer's resident bindless texture cache.
* Loadout uses the original faction background, equipment-slot definitions and
  bitmaps, rotating 3D ship, description, hull gauges and selected-part statistics.
  Hull/part browsing operates on a detached IGC preview, using original purchase
  policy. Defaults, buy/confirm/launch, empty slots, cargo for other ships and
  Ctrl+digit save / digit restore work. Preview ships do not enter live rendering,
  visibility, sound, network identity or account state, or advance simulation RNG.
  Hull/part menus use original prices, continuous fourteen-pixel rows and
  pointer-relative placement. Cargo's adjacent "Parts For Other Ships" submenu
  retains core item order and opens on hover or Right. Hover statistics read
  shared ClientCore metadata without constructing another preview ship.
  The original menu has no overflow scrolling; the native version adds the
  shared bitmap scrollbar only when a list cannot fit the authored canvas.
  Escape/Left, wheel, page keys and thumb dragging keep every choice reachable.
  Cancelling a menu preserves the staged loadout.
* Options and the in-game Escape menu now use the original small bitmap font,
  fourteen-pixel menu rows, colors, raised borders, shortcut columns and cascade
  arrows. Hover opens an adjacent submenu; keyboard navigation, accelerators,
  one-level Escape/Left and outside-click dismissal work. Supported preferences
  apply and persist immediately, without an Apply/Cancel page. Visual changes
  do not reopen input devices or rebuild key maps. Input mapping closes the menu
  first and returns to the parent screen or flight view when dismissed.
* Original popups overlay the existing flight scene and HUD in one ordered batch.
  The flight renderer is shared with ordinary rendering; menus do not duplicate
  simulation or scene generation, clear the world, or show the former native
  pause/help sheet. The default chase preference persists and applies to flights.
  Unsupported legacy/backend rows explicitly show their current native status;
  these are remaining work, rather than functioning D3D/DirectSound toggles.
* Credits uses the authored popup and complete original staff text, fonts and
  colors, with the original ten-pixel-per-second scrolling and clipped viewport.
  The compiled text stays resident; scrolling does not reparse or copy it per frame.
* Conquest setup uses `newgamescreen` at its original 800×600 size, including
  twenty-one authored choice menus, checkbox faces and hover explanations. Game
  presets and their recognition policy are shared with WinTrek through ClientCore.
  Lives retain the original death-count convention; resource/treasure choices
  update all of their coupled fields and preserve the original count truncation.
  Accept reads the displayed choices and validates before entering the team lobby
  or sending a host configuration; Cancel restores the staged session. Nonhosts
  and running matches are read-only, and losing host authority closes stale menus.
  Generated map previews reuse WinTrek's topology maker with the original axis
  conversion, owners, aspect/clipping limits, drag bounds and hover counts. Line
  runs and icons are cached between changes and use the existing ordered UI batch.
  The native preview adapter releases membership and destination reference cycles;
  WinTrek retains its existing lifetime path. Checks cover all 140 valid generated
  map/team/size combinations, teardown and original settings-screen pixels.
  Club-only controls retain their original disabled state. Allow Joiners,
  Allow Defections and Max Imbalance now drive authority policy. Alliances
  retain the original forced-defections/unbounded-imbalance policy. The per-team
  player-limit field shows the native server's uncapped policy.
* The teleport console uses `teleportpane` over the hangar, with the original
  left-screen anchor and half-second slide. Docked turret ships precede own
  restart bases; current destinations, kept/refitted hull colors and occupied
  turret counts follow the original. Mouse-down selects, double-click travels,
  arrows/page keys and the scrollbar reach the whole list. Selection uses a
  typed ship/base identity and survives live reordering; removed destinations
  clear it. Station transfer, boarding failures and their sounds retain original
  behavior in offline and network action paths. F8 and the hangar button toggle
  the console independently of the other banishable panes.
* F5 investments use the original `conpurch`, six tab strips, seventy-two-pixel
  purchase cards, part images, highlight/brackets, green/yellow/red progress
  shading, credit box, information panel and bitmap fonts. The pane retains its
  right-screen anchor and half-second entrance/exit at integer scales. Filters,
  prerequisites, stockpile/drone/base counts and constructor status read actual
  own-side IGC buckets. Full investment and right-click's ten percent payment
  follow the original policy; partial payment cannot finish a bucket. Selection
  persists through live money/progress updates, and wheel/scrollbar paging moves
  whole cards without changing it. The flight pane overlays the existing scene;
  keyboard/joystick flight controls remain active while its pointer is available.
  The original Toggle Mouse action releases the fresh investment pointer without
  hiding the console, allowing mouse flight with the cards still visible.
  Command-view selection/orders remain available on the uncovered scene, with
  console hits and mouse capture taking priority. Closing the pane resumes input
  immediately while its inactive exit animation runs.
  Hovering a base card or its Invest button highlights sectors containing known
  matching asteroids with the original `sectorripcordbmp`. Subtype precedence
  follows `SectorMapPane`; this visual hint does not change construction legality.
  Discovery, loss of knowledge and destruction invalidate a cached sector list.
  Unchanged frames do not re-extract bucket metadata, rebuild that list or rebuild
  the layout.
* The sector map uses original expanded/collapsed panel art, sector outlines,
  ownership/invader colors, seven-pilot population dots, warning flashes,
  accepted/queued orders, ripcord and construction/teleport markers. Original
  aspect limits, discovered-sector geometry, bounded drag and half-second slide
  replace the simplified native map. Hover information uses the original bitmap
  font and station/ship/resource/probe icons, observed resources and known objects;
  unchanged information reuses its cache, and projectile/missile changes do not
  invalidate it. F5 and F8 force map visibility
  independently of F7. Left-click while docked selects a command-view sector;
  while flying it accepts an original sector-buoy goto order, or retargets an
  active manual ripcord. F8 map clicks choose own restart stations.
  Right-click opens friendly-sector chat for that sector. Ctrl-right-click
  toggles commander highlights for the faction; other pilots dismiss them only
  locally. Highlight records live in the existing shared faction arena and use
  the existing lossless delta codec, rather than separate per-pilot encoding.
  Geometry/tie/warning checks, discovery/observed-resource and buoy-lifetime
  checks, native map pixels/input, and encrypted GNS highlight sharing,
  dismissal, faction isolation and record insertion/removal/reordering checks
  cover these paths. The same policy, encrypted sector-map, pixel and actual
  F5/F8 map-click checks pass on Linux Wayland, Windows under Wine and macOS
  (including Metal shader validation). Original inline command composition and
  recipient controls are now implemented as described below. Map cursor artwork
  and remaining pane-specific presentation are still pending.
* In-game chat uses the original `dialog::ComposingPane` artwork above the
  existing flight, hangar, loadout or investment pane. The five original compose
  states, bottom anchor, half-second slide, side-colored `smallFont` text and
  single-line/right-clipped entry fields replace the added modal chat adapter.
  Chat and command recipients remain separate; opening main quick chat uses the
  current composition's recipient, or the saved chat recipient when idle.
  Enter, Escape's command-to-chat transition, Backspace, the `!` shell, Ctrl plus
  verb initials, Tab's context-sensitive recipient cycle, recipient prefix
  matching and uppercase typed-target completion follow `ConsoleDataImpl`.
  Target candidates use original IGC command legality and visibility, with stable
  IDs instead of retained object pointers. Departed recipients are removed before
  sending; public pilot identities remain usable without receiving their ships.
  The original command-bar Me/Sector/Team/All buttons, wing combo, disabled verb
  faces and hover descriptions work. The wing combo selects a recipient rather
  than changing the pilot's wing. Typed commands retain left-click target picking;
  ordinary selection and right-drag height placement remain available. Empty
  selections cancel composition. `ViewCommand` and `ConModeCommand` now toggle
  the same command view, rather than opening added order menus.
  Quick chat uses its authored keys, item order, original lower-left popup and
  cascading child menus. Composition is compiled on change, and joins the existing
  bindless HUD batch without a separate render pass or per-frame MDL evaluation.
  Explicit wing routes no longer fall back to the sender's wing. Group orders
  deliver individually to each human and produce one author echo, matching
  FedSrv's group-channel conversion. Waypoint/cluster commands transmit intent;
  the authority creates and manages their buoys. `allegiance-console-check`, the
  encrypted `allegiance-network-check --case console`, and actual SDL input in
  `--console-smoke-test` cover these paths. Console policy, encrypted routes,
  bitmap pixels and the 700-frame SDL interaction check pass on Linux Wayland,
  Windows under Wine and macOS, including Metal shader validation. This work
  leaves WinTrek's hot paths and all platform frame-pacing policies unchanged.
  Lobby quick chat and original cursor artwork remain on the parity
  queue.
* The separate F6 player console uses the original `teampane` compact and
  expanded artwork, bitmap fonts, authored buttons, team tabs/insignia, list
  widths, column positions, donation/order/ship icons and half-second slide.
  F6 expands the compact pane and then hides the expanded pane; the mini-team
  shortcut selects the compact pane. It overlays hangar or flight without
  replacing either, and preserves selection across size changes and reopening.
  Original human grouping and stable member ordering, header sorts, team selection including
  Not On A Team, clipped scrolling/dragging/repeat, own-wing selection and
  100/500-credit donations are connected to the game. Flight context menus
  select private-chat recipients or commander drone Dock/Launch actions;
  lobby-only actions are not substituted for those menus. Public human identity
  remains selectable without revealing an unseen enemy's ship, location or
  account. Stable IDs protect duplicate names, departed selections and popups.
  Commander Stop Donation toggles the selected account's donation route off or
  back to the commander, matching FedSrv, through the authoritative network
  action. Take Me To respects sector/visibility restrictions and picks a console
  recipient without replacing the weapon target or enabling camera following.
  Command transitions retain the shared original camera's interpolation,
  including the two-second Take Me To transition. F6 respects the launch/death
  camera override. Layout is retained on unchanged frames, refreshed at most
  10Hz while open and appended to the existing HUD batch.
  Both sizes retain `dialog.mdl`'s shared SideRight origin at `(-750,-150)`;
  the compact pane's inset position is present in the original authored layout.
  Sighting and replica-retirement notifications retain last observed ship/sector
  information while the pane is closed. Public departures and changed identities
  clear obsolete history without requiring F6; an authorized ship-death effect
  removes a retired drone. Unknown opponents still reveal no live hull or sector.
  The commander's compact pane shows the original `Docked for...` text for other
  docked humans. Its timestamp comes from the actual station transition, survives
  hull reloads and uses an existing unused ship-record field without enlarging
  network records. Canonical player identities also survive crew/hull reloads.
  Server-side faction history now bootstraps last-known hull, sector, dock and
  crew status independently of physical replicas. Unseen drone retirement and
  human death/restart ordering are restored; their evidence is recorded below.
  More allied/observer/ghost lifecycle comparisons remain on the queue.
  `allegiance-player-pane-check`, original console/session/keymap/script checks,
  Vulkan pixel checks and `allegiance-network-check --case console` pass on
  Linux, Windows under Wine and macOS, including Metal shader validation. The
  actual SDL player-pane check covers F6, authored collapse/expand buttons,
  roster/context picking, private chat, hangar/flight overlay, launch override
  and continuous steering on all three platforms (1,325 frames on Linux/Wine,
  1,010 on macOS). Remaining player-pane work is listed explicitly below.
* The `/` Game State console now uses the original header/footer, victory-condition
  title images, bitmap font, colors, grid widths and left-screen slide placement.
  It shows the original leading-team/own-team and leading-human/self pairs,
  including kill/death tie rules, countdown/elapsed clock and mission goal text.
  F5, F6, F8, `/`, Escape and the authored close button retain banishable-pane
  behavior; steering stays active while the console is open. Its compiled quads
  join the existing HUD batch and refresh only when displayed data changes.
  Human kills, deaths and ejections now accompany the shared public roster, so
  Game State and F6 scores update for unseen pilots without transmitting their
  physical ships. The original short counters occupy eight additional explicit,
  initialized bytes per membership record. The authority still captures the
  roster once per publication cycle and shares one encoded arena across all
  recipients; unchanged scores retain the roster revision. Native builds reject
  older incompatible clients through their existing build identity check.
  `allegiance-game-state-check`, encrypted `--case scores` and actual SDL
  `--game-state-smoke-test` cover this restoration. Policy, keyboard, original
  script/session/pane, encrypted score/crew/console/sector/health/throttle and
  rendering checks pass on Linux, Windows under Wine and macOS. Actual SDL
  Game State interaction passed in 1,424 Linux frames, 1,425 Wine frames and
  1,110 macOS frames, including Metal validation. Original training mission 5
  and the existing player-policy checks also pass on Linux. WinTrek and platform
  frame-pacing paths are unchanged. Postgame statistics are a separate authored panel; the live Game State
  console does not replace them.
* Original crew displays distinguish observers from turret gunners and name
  the parent pilot rather than their hull. Flight turret captions use the original
  top-right anchors, bitmap fonts, colors and additive blending. Loadout uses the
  original unkeyed `hangarinturretbmp` and side-colored Observer/Turret label at
  their authored center offsets. The compiled display retains its controls on
  unchanged frames and joins the existing ordered HUD batch.
  Docked passengers can open loadout, stage hull/equipment changes and buy their
  own ship, as WinTrek's `DisembarkAndBuy` allowed. Previewing and cancelling
  preserve the crew relationship. Invalid purchases leave it intact; a validated
  purchase disembarks and equips the child without changing the parent, its value
  or either pilot's identity. Affordability uses the passenger's money and own
  trade-in value, never the parent's hull. The existing authoritative Loadout
  action handles the network transition; this adds no wire records.
  View Base now imports and dispatches the original logical command, toggles
  back to cockpit and retains WinTrek's actual external-view fallback for that
  camera mode. It does not invent a different station camera. Crew pane policy,
  source naming, blend/anchor/cache/scaling, purchase validation, encrypted crew
  transitions and actual original-menu input have dedicated checks. These checks,
  original session/keyboard/script/pane regressions and the crew pixel
  probes pass on Linux Wayland, Windows under Wine and macOS, including Metal
  shader validation. The mouse/key interaction check completed in 1,114 Linux
  frames, 1,116 Wine frames and 800 macOS frames. Linux Vulkan validation is
  clean; the crew bitmap matches its authored pixels and additive red pilot
  labels preserve the scene's green/blue channels. The three current client and
  server builds have the same source/build identity. Encrypted health and throttle
  checks also pass on all three platforms; the broader loadout policy check passes
  on Linux and Windows under Wine. WinTrek and platform pacing
  paths are unchanged by this restoration.
* Postgame uses the authored `gameoverscreen` background, header strips,
  chat artwork, original 14-pixel player/team tables, fonts, side colors and
  hover descriptions. Header clicks retain the original sort defaults and
  directions, including position-header reversal and ejections/deaths choice.
  Both checked header directions retain their hover/pressed faces. Tables sort
  only when data/order changes and paint clipped visible rows; the 1,024-player
  check renders twelve rows. Chat wraps with resident bitmap-font advances,
  continuation indentation, scrolling and ten-second autoscroll restoration;
  Everyone, pilot-name, Admin and shell input retain their postgame routing.
  Save Stats writes the original XML schema with escaped text and the actual
  core filename, reporting success/failure in the local chat area.
  The authority accumulates original IGC `PlayerScoreObject` scores for humans:
  damage-share kills/assists, base kills/captures, discoveries and high-value
  sightings, repair, rescue, recovered tech, objective deliveries and timers.
  Final reports include departed pilots, original eligible win/loss handling
  and commander credit. Native persistent ranks have no authenticated source
  and remain blank; defection score-history attribution needs a further audit.
  Results freeze once and use one immutable public report arena, encoded once
  across recipients. The manifest limit includes this arena alongside faction,
  public roster, viewed/physical sectors and the eight projectile partitions.
  Restartable native Conquest matches send final recipient messages on the
  reliable control lane, create the next readiness lobby, retain previous scores
  and preserve each client's received chat. Back returns to Team Lobby; starting
  the countdown returns on-team pilots from results to the lobby as WinTrek did.
  Subsequent round reports replace the previous frozen data. Leaving a local
  classic session returns to the original main menu; authored lessons return to
  training. Tests cover actual fractional damage credit, departed players,
  command/win/loss timers, raw result records, relocation deltas, XML escaping,
  1,024-recipient shared encoding, two encrypted rounds and maximum manifest
  subscriptions and distinct revisions when a round reset is delayed.
  Endgame/session checks, two encrypted rounds and original compositing probes
  pass on Linux, Windows under Wine and macOS. Actual SDL postgame interaction
  completes in 250 frames on each platform, including header clicks, scrollbar,
  Save Stats and Back. Linux Vulkan and macOS Metal/shader validation are clean.
  Current clients and servers on all three platforms share one build identity.
  WinTrek rendering and platform pacing are unchanged.
* All four original turret-swap and turret-promotion commands now import from
  keyboard/mouse/controller maps and dispatch through the existing authoritative
  swap/promotion actions. Indices start at the first non-fixed hardpoint. The
  original dock/passenger restrictions, actual occupied mount and ownership
  transfer are checked against WinTrek/FedSrv. Offline and encrypted network
  checks cover boarding, stable identity after hull reload, firing, promotion,
  crew death/pods and hangar recovery. These changes add no work to WinTrek's
  input or rendering paths.
* Original message boxes replace added bottom-of-menu error labels. They use
  the keyed dialog background, large bitmap font/shadow, original text width
  and OK position, and capture input above animated console panes. Connection
  progress uses the original buttonless `Connecting...` modal and clears on
  admission; failures remain visible for acknowledgment. Shared-report regression
  checks also exposed implicit snapshot padding: existing snapshot, admission,
  configuration and mission-policy alignment bytes now have explicit initialized
  fields, retaining their record sizes/offsets. A poisoned-memory default-
  construction check covers every raw wire record; direct/deferred captures are
  byte-identical again. Successful classic
  loadout operations do not add success popups.
* Original LAN Join/Abort, quit confirmation and Help artwork now replace their
  experimental counterparts. Automatic discovery and the original selectable
  server list are completed in the LAN restoration batch below. Help closes with F1
  or Escape. The command view uses the original bitmap command bar and verb
  hit regions. Added flight help/session and inventory action footers, the modern command button
  panel, slideshow footer and modal help sheets require `--modern-ui`.
* `allegiance-ui-script-check` checks installed scripts, original button layouts,
  launch frames and imported loadout composition. `allegiance-session-check`
  exercises mouse cancellation, menu changes during capture, lesson progression,
  double-click launch, lobby chat recipients, large rosters, mapping conflicts,
  modifier semantics, original teleport/investment/modal interactions and virtual joystick remapping. Linux, Windows and macOS builds
  run the same checks. Vulkan pixel checks cover bitmap text with and without
  clipping; screenshots are reviewed separately from the policy checks.
  A 700-frame client check passes on Linux Wayland, macOS and Windows under
  Wine: F5/F8 toggles, station transfer, launch beneath investments, keyboard
  steering with the fresh pane open, Toggle Mouse release and immediate return
  while the exit animation runs. Construction-site checks cover unknown rocks,
  newly discovered sites, destruction, hover clearing and the original marker.
  Native-size/whole-scale layout checks cover odd dimensions and pointer
  transforms, settings persistence and every frontend screen route. Vulkan
  probes check exact pixel replication at 1×, 2×, 3× and 8× and verify that
  sampling variants do not duplicate GPU image allocations.

* Original `cursor.mdl` now supplies all fifteen keyed cursor images and their
  authored hotspots. Native menus, movable map previews, sector-map nodes and
  dragging, friendly ship selection and legal order targets use those images.
  IGC supplies default orders and command legality, including the miner's Mine
  cursor on helium rocks. Projected object radii and nearest-center selection
  replace fixed command-view click boxes; left selection and right orders retain
  their original eligibility rules. Cursor queries consume resident contacts and
  cached subject IDs, with no fleet-report construction per pointer sample.
  Cocoa/Wayland logical cursor sizes account for device density; compatible
  integer densities use native cursor planes with exact nearest-replicated image
  representations. Fractional/incompatible densities use one ordered HUD quad,
  keeping the requested whole-device-pixel scale. UI panels and clips block
  underlying world-hover names/cursors. Cursor resources remain cached.
* Training restores IndicatorImage's twenty-three red callouts, two-segment
  lines, original highlight-dot image, bitmap font, captions and secondary text.
  `ClientCore/TrainingPresentation.h` contains the original geometry/text rules;
  WinTrek uses the same rules and retains its rendering backend. Its common
  no-callout path returns immediately. Native callouts respect original camera
  modes and view-edge/center anchors rather than the added yellow boxes. The
  original flight lesson's actual Roll highlight is checked through its authored
  action, TrainingHost and GameState. The training diagnostic now uses a valid
  MSVC stdio buffer size rather than failing before startup on Windows.
  The complete macOS sequence also exposed a real training teardown lifetime
  hazard: libc++ clears a unique_ptr before calling its owned destructor, while
  authored action destructors still call TrainingHost through Game. Game now
  retires training in its destructor body while its core remains reachable,
  preserving the existing action cleanup and subsequent core destruction order.
  This adds no simulation or WinTrek work.
  The Mac's 44-day uptime exposed a separate original zero-timer sentinel bug:
  arena waves stopped after two kills because the 32-bit signed clock comparison
  never initialized their respawn deadline. Offline games now use the same
  match-relative epoch as network games. The unchanged original lesson logic
  completes all 49 kills across eleven enemy hulls and its victory narration.
* Graphics options restore the independent Off/Low/Medium/High debris cycle and
  Off/Performance/Low/Medium/High particle cycle with original labels and
  immediate application. Smoke Off stops thrust/damage/teleport emission while
  retaining explosions, trails, glows and independently configured debris.
  Performance uses the original rocket-style fire/smoke emission and shorter
  default ship smoke profile. Missile-specific smoke parameters stay intact.
  Debris uses its original spacing values and effective inverse-speed equation;
  disabling it retires existing debris immediately. Original `Debris` and
  `SmokeEffects` imports and native persistence retain their separate meanings.
  Popup cycle, independence, saved preferences and particle lifetime checks
  cover these behaviors; no original WinTrek effect path was changed.
* Team/lobby and postgame screens restore the original quick-chat popup at the
  upper-left corner, authored captions/accelerators (including digits), child
  cascades, mouse choices and one-level Escape behavior. Closing console
  animations now affect only their owned artwork and do not move later popups
  or modal acknowledgments; lobby/postgame transitions retire those animations.
  These screens send the
  logical sonic voice to their selected chat channel/person without flight
  command targeting, as WinTrek does when its flight console is absent. Sending
  or canceling retains the screen, recipient and unsent text; accelerator text
  events cannot leak into the draft. The existing reliable QuickChat action
  carries the voice-only intent in an unused argument; no wire structure grows.
  The authority also forces voice-only delivery before/after gameplay. UI tests
  cover all four team/postgame routes; an encrypted two-client check verifies
  individual voice delivery without replacing the recipient's queued order.

* Chat options restore the original three lobby filter modes, one-to-ten line
  limits, Small/Large/Huge font cycle, captions and accelerators, with immediate
  application and independent persistence/import. The classic HUD uses the six
  original bitmap fonts (including bold commander messages), the original
  355-pixel wrapping width inside its 370-pixel black pane, original line height
  and top anchor. Loadout limits the requested lines to six without changing the
  saved preference. All font images remain resident; line-count changes do not
  rewrap history, while font changes rebuild it once. Configured paging retains
  the original ten-second autoscroll timeout and one-row overlap.
* Network chat notices retain sender identity, delivery/filter facts, commander
  font metadata and compact side color. The recording accompanies the notice,
  so filtered text cannot leave its separately dispatched voice playing.
  A notice grows by eight bytes while each recorded chat removes a separate
  full sound-event record. Recipient filter/mute changes use existing reliable
  action records, on connection or user changes rather than on simulation ticks.
  The authority respects those preferences before queuing orders and keeps them
  separate from its currently activated pilot. Lobby filters preserve the
  original distinction between public lobby messages and private messages;
  lobby recipients do not apply the in-game lobby/public restrictions. Privileged
  moderator PM exemptions remain contingent on a native authenticated moderator
  facility rather than trusting pilot names. New-round client initialization now
  reapplies persisted presentation/input/chat preferences to the replacement core.

The October 6 parity validation passed on Linux/Wayland, Windows under Wine,
and macOS/MoltenVK: original menu/session checks, actual IGC chat-filter and
postgame font checks, encrypted chat delivery/order exclusion, and live SDL
chat/quick-chat keyboard tests. Live checks exercise all three bitmap chat sizes,
one-to-ten line limits, six-line loadout adjustment and reuse of resident history
layout. macOS live runs enabled Metal API and shader validation. Linux also
passed the encrypted normal-match, console routing and lobby/new-round checks.
That checkpoint used client/server source identity
`7b5ec5990e9abbb1a2531bb6aca15108ec358ea226ebebbf707fbb740fdb4d93`.
Further macOS pacing work is deferred at the user's request.

The following team/lobby batch restores the authored `TeamSettingsDialog`
artwork, original large bitmap font, centered controls, name limit, faction
choices by actual core ID, staged accept/cancel, and name-field focus after the
faction menu closes. Each team leader can edit their own team independently of
game ownership; the original team-settings lock is enforced by both the UI and
server. Opening settings as a ready non-owner commander withdraws personal and
team readiness as WinTrek does. Loss of leadership, team departure, locking or
countdown closes the live editor. Squadron membership and the original disabled
random-faction choice are not replaced with fabricated options.

Faction changes call original IGC civilization/base/technology code. The native
pregame hulls and initial miners are refitted, and faction attributes, buckets
and starting money are refreshed without replacing the world, pilot identities,
leadership, match, connections or chat recipient policies. A ready opposing
commander's consent is withdrawn. Names and concrete faction selections persist
in session records, and existing replicas now apply changes to side names and
civilizations. Existing side records carry these changes; steady-state report
sizes are unchanged. Team names add data only to reliable session setup records.
Civilization artwork metadata is kept in a separate catalog rather than added
to every ship/part choice.

The native MDL compiler now supports the original `BlackPane`/`PaneImage` and
premultiplied `BlendColorImage` composition used by the lobby watermark. Occupied
teams show the non-keyed original faction image with the authored tint; empty
teams and No Team show the original default watermark. Team tabs use actual IGC
side colors. This remains flat native UI composition in the bindless renderer.
Further macOS pacing work remains deferred.

This follow-up passed original session/UI checks and encrypted team-settings
checks on Linux, Wine and macOS. Linux also passed the full lobby/new-round,
normal network match, original player-policy and all-faction economy checks.
The rendered 260-frame team-dialog diagnostic passed on all three platforms,
including actual SDL mouse/text input, faction popup placement and cancel/focus
behavior; Linux used Vulkan validation and macOS used Metal API/shader validation.
Current client/server package source identity is
`6e62ec0e27fca263cfa687a9f0c4b5a09b03016fcaebf64cad5592e4cfd7e971`.

The next lobby batch restores `FM_C_RANDOMIZE_TEAMS` semantics: normally Flush
moves non-commanders to No Team; Enforce Balance instead redistributes ready
pilots. Commanders, pilot identities, world, side locks and personal AFK state
are preserved. Auto Accept is disabled, Boot bans are cleared, and balance mode
clears pending requests. AFK pilots remain in No Team. Native pilots have no
authenticated Club ranks, so the original unranked rank-one weighting and stable
first-side tie order are used; existing No Team arrivals precede newly flushed
players, and roster counts are cached during balancing.
Original Flush/Balance reason messages use the authored message box, are not
reopened by repeated reports, and remain acknowledged across retained next-round
history. Team changes follow the accepted roster and reset the channel to All,
as `TeamScreen::OnAddPlayer` does.

Shared `ClientCore/LobbyReady.h` policy now drives native pre-start count/balance
and readiness gates, with the original status lines and timer formatting.
Start cannot restart an existing countdown. The authority publishes the original
`STAGE_STARTING` state and restores `STAGE_NOTSTARTED` when a native countdown is
canceled. The native empty-team/single-player policy still uses minimum zero and
the original signed-short identity capacity, without a separate human pilot cap.
Existing WinTrek/FedSrv callers retain the unchanged `SideReady` implementation;
no WinTrek render or per-frame path changes are included in this batch.

Assigned pilots now see the original `missionbrief::MissionBriefDialog` artwork,
faction-specific title/objectives, original fonts, tip, team/civilization icon,
countdown and original generated map preview. This uses the existing native flat
UI renderer and shared preview composition, not legacy Pane/Image rendering.
Briefing text is formatted once per opening; topology and original assets are
cached. No Team sees the authored timer fields instead. Briefing input blocks
the underlying lobby. The local pre-world countdown uses the selected map as
its location until a world exists; network briefings use the actual starting
sector. Custom story text and custom static-map briefing data remain queued.
Further macOS pacing work remains deferred.

This batch passed session/UI policy checks and encrypted Randomize authority
checks on Linux, Wine and macOS. Linux additionally passed existing lobby,
admission and faction-settings checks. The 400-frame rendered team/briefing/
No Team diagnostic passed on all three platforms; Linux enabled Vulkan
validation and macOS enabled Metal API/shader validation. The render review
also corrected briefing topology being composed into the underlying lobby's
preview and a stale starting-sector label. Current client/server source identity
is `281785a02a948abafefdd5162a834c4a618581ff3cf46b667793fbb97ce268b4`.

## Games entry checkpoint (2026-10-06)

The original main menu's Internet action now opens a real games list. Offline
scenario fixtures no longer appear as public server rows. The authored artwork,
bitmap fonts, status icons, population/No Team columns, duration, mission-style
icons, header sorts, scrolling, selection, Enter and double-click joining are
restored. Refreshes retain selection by endpoint rather than by row number.
The original Filter dialog implements all eleven Yes/No/DontCare fields and
case-insensitive name filtering with staged acceptance/cancellation.

The native browser currently queries **known endpoints**. It includes loopback
`127.0.0.1:27500`, accepts repeated `--server IP:PORT` arguments, and reads one
numeric endpoint per line from `servers.txt` beside the platform's default
`native.conf` (blank lines and lines starting with `#` are ignored). Explicit
connections are also remembered for the current run. Example:

```sh
build/linux-release/src/native/allegiance-native --server 192.168.1.10:27500
```

Queries use GNS and the existing exact-build/data compatibility gate, without
claiming a pilot, assigning game ownership or transmitting a world baseline.
One poll group handles at most four concurrent probes and sixty-four endpoints;
refreshes are five seconds apart and stop outside the games screen. Closing a
probe does not invalidate the gameplay roster. Listings contain current real
occupancy, admission locks and rules, rather than client-side scenario guesses.

New Game opens the original creation dialog with an explicitly named **Local
game / This computer** target. It creates a named Conquest game through the
original settings and team screens. The name is retained in session records and
presented by the settings, team lobby and server browser. Native servers accept
`--name` and `--server-name` for their listing. Leaving a remote game returns to
its entry screen and clears connection authority/countdown/admission state, so
subsequent local creation uses a local settings transaction. Bad explicit
addresses produce a message box instead of terminating the client.

This is not an implementation of the legacy Club directory. Automatic LAN
discovery is completed in the LAN restoration batch below. Find Player across
known/discovered authorities is completed in the Find Player audit below.
Public directory registration and remote multi-game creation remain queued.
A native authority still hosts one match; the local creation target is not
advertised as a discovered server.

Linux, Wine and macOS session/browser checks and the rendered SDL entry diagnostic
passed. The latter visits the games list and Filter dialog, joins a real
authority, uses the original team lobby and leave confirmation, and creates and
cancels a local game after leaving. Linux used Vulkan validation; Wine joined the
matching Linux authority; macOS used Metal API/shader validation. No WinTrek/FedSrv runtime or frame
pacing changes were made in this batch. Source identity is
`e46a2ec7da537a87f76ab1c364aa0f1c244947f54179ac2bdd128249743a146a`.

## Purchase/refit checkpoint (2026-10-06)

The classic loadout panel now uses IGC's original `PurchaseShipLoadout` rather
than treating a staged purchase as cached-preset restoration. Actual purchases
retain requested part quantities; the original ten preset banks still store
part IDs and mounts and request full quantities when restored. The original
Buy/Confirm/Launch enable rules include owned ships and teammates who can cover
a money-request shortfall. Launch no longer depends on local hull availability
when the ship is already owned.

Network purchase and optional launch are one reliable, idempotent action. The
original "Loading out ship...." / "Preparing ship for launch...." input lockdown
ends after both the receipt and the corresponding canonical outfit/wallet report
arrive. Existing report header acknowledgement space provides that barrier;
there is no extra steady-state record or world bootstrap for a purchase.
Partial purchases keep the ship docked, copy the actual outfit back into the
editor and show the original unavailable-parts message. Rejected or unaffordable
requests preserve equipment/crew and cannot accidentally launch. A resumed
connection recovers a lost purchase receipt from the authority's existing cache.
Post-purchase cargo quantities are no longer overwritten by an extra refuel
call, and launch from the loadout panel correctly returns to the flight view.

`allegiance-network-check --case purchase` covers requested quantities, causal
completion, wallet accounting, malformed/unaffordable requests, unavailable
equipment, reconnect and atomic launch. `--refit-smoke-test --connect IP:PORT`
drives the actual SDL classic controls through staging, purchase, launch,
autopilot docking, default refit and relaunch. This batch changes no WinTrek or
FedSrv runtime code and makes no frame-pacing changes. Source identity is
`0dd58207a40243d3a5f32e9421a569602d72fa890615c437314badb2d6a57197`.

Loadout, session and purchase checks passed on Linux, Windows under Wine and
macOS. The rendered SDL refit/dock/relaunch cycle passed on all three; Wine used
the matching Linux authority. Linux used Vulkan validation and macOS used Metal
API/shader validation. Existing Linux network economy/flight/docking and crew
boarding/promotion/recovery checks also passed. Authored 800×600 loadout, cargo
menu and purchase-lockdown captures were reviewed at native UI pixels.

## Remembered loadout and recovery checkpoint (2026-10-06)

WinTrek's matching-hull `BaseClient::RestoreLoadout` policy now lives in
`ClientCore/Loadout.h`. WinTrek adapts its existing cache and linked-list order
directly, without an additional allocation or pass. The native client uses
part/hull IDs and contiguous cache entries. A frozen copy of the original branch
provides 288 comparisons of surviving equipment, lost mounted parts/cargo,
launcher quantities, duplicate matching and successor hulls. Those include 138
retained unavailable mounted parts and 102 lost-slot recoveries.

Editor choices are separate from the live ship and from carrier rearm state.
Closing the editor remembers its staged fitting without buying it; changing
hulls remembers the old fitting and initializes the selected hull from custom
bank zero, as `WinTrek/load.cpp` does. Opening Options and returning preserves
staged edits. Initial restoration retains owned mounted equipment that a station
no longer sells, replaces/refills missing equipment on the original budget,
and uses the original first affordable fallback when a cached hull is unavailable.
Browsing remains read-only for equipment, credits, RNG and mission membership.

Normal docking now restores missing equipment automatically. Offline purchases
run after IGC finishes its docking/death callbacks. Network clients request the
same purchase after reconciling the docking report and use the existing receipt
and canonical-report barrier. Steady docked reports do not repeat the purchase,
and no docking event or steady wire field was added. A recovered lifepod remains
a lifepod until the client restores its remembered hull/equipment; the authority
no longer overwrites those choices with the starting ship. Regression checks
cover actual IGC docking and fatal recovery, launcher depletion/refill, different
remembered hulls, duplicate prevention and credit conservation.

This batch factors an existing WinTrek UI policy; it changes no WinTrek frame,
renderer or simulation loop and makes no frame-pacing changes. Loadout, session
and network purchase/dock/recovery checks passed on Linux, Windows under Wine
and macOS. The rendered original hangar/staging/purchase/launch/autopilot-dock/
refit/relaunch cycle passed on all three. Linux used Vulkan validation; macOS
used Metal API/shader validation. Existing Linux authority health/ejection/pod
recovery and crew boarding/promotion/recovery checks also passed.

Current client/server source identity is
`8768bea24cb6d16607e0ecb083a29049c5a6ccd74b23519d724064d9581d6192`.

## Mission-entry and transfer checkpoint (2026-10-06)

Initial mission entry now performs the original `ReplaceLoadout` purchase using
the most recent custom-bank-zero fitting, with the original affordable default
fallback. This is separate from docking's `RestoreLoadout`, which preserves
surviving mounted parts and trades in the current ship. The native client no
longer starts with an unrelated authority-fitted loadout and then proposes a
refit as soon as the player opens the editor.

Explicit disembark, passenger teleport and teleport to a base that cannot retain
the hull also perform the custom-zero replacement. The authority refunds and
changes station/crew membership; each affected client purchases its fitting
after reconciling that state. Atomic passenger purchase continues to use the
requested fitting directly and does not trigger a second replacement. Offline
station changes queue restoration in the original `ChangeStation` callback and
perform it after IGC callbacks finish, covering direct rescue/evacuation routes.
All these paths reuse the existing purchase intent, receipt and canonical
completion barrier; there are no additional wire records or steady-state fields.

Regression checks cover custom-zero precedence over the session cache, different
hulls, original default fallback, read-only previews, no unsolicited initial
editor changes, lost equipment, direct station membership changes, passenger
disembark, unavailable-hull teleport, parent/crew identity and credit conservation.
Loadout, session, teleport, network purchase and crew checks passed on Linux,
Windows under Wine and macOS. Linux economy/flight/docking, health and
lobby/start/reconnect/next-round checks also passed. The rendered remote refit
cycle passed on all three platforms, with Vulkan validation on Linux and Metal
API/shader validation on macOS. Original-size 800×600 entry/refit captures retain
the same bitmap artwork, font sizes, control placement and equipment quantities.
This adds no WinTrek runtime changes beyond the preceding shared restoration
factoring and changes no pacing code.

Current Linux, Windows and macOS client/server source identity is
`184d124a11e90419df2192b92e59102f41df2a525c3ab1b3b338d5a20f0d3ef4`.
The Windows ZIP and deployed macOS bundle include the matching client, server
and this audit; the macOS development bundle has a verified ad hoc signature.

## Lobby history and action checkpoint (2026-10-06)

The lobby's former nine unwrapped messages are replaced by the original wrapped
chat-list behavior at its authored 407×123 viewport, with bitmap small/bold
fonts, two-space continuation indentation, scrollbar arrows/thumb, wheel input
and mapped Chat Page Up/Down commands. Lobby and postgame share a native cached
history control. Paging targets the visible pane even when a flight core exists;
custom keyboard chords and other mapped input routes use the same dispatch.
Tab preserves the selected original lobby channel rather than cycling into
flight-only channels.

All Players, Team Only and Leaders Only filter by the original ChatTarget.
Private and administrator messages remain visible across those tabs. Human
messages on other channels flash their resident header controls without
rebuilding the layout. Case-insensitive `Pilot:message` and `admin:message`
addressing works. A manually read line and the unsent draft survive new messages
and expiry of older history. The original ten-second timeout returns to the
latest messages. Chat wrapping is cached until history, channel or censor policy
changes. The original target uses unused bits in the existing notice flags;
this batch adds no wire fields or record bytes.

Shared ClientCore policy restores applicant Accept/Reject and the teammate
Boot [R] shortcut. Locked team settings disable teammate Boot in the lobby and
F6 button and reject it at the authority, while outstanding requests can still
be rejected. Make Leader and voluntary departure retain their original gates.
The F6 Accept button follows the original human-count/imbalance availability
policy; the server independently revalidates admission. This adds no WinTrek
runtime or platform-pacing changes. Flight-console Boot confirmation and its
no-defections mission-kick path are implemented in the following checkpoint.

Session/player-pane checks, encrypted admission/chat checks and the rendered
`--lobby-chat-smoke-test` pass on Linux/Wayland, Windows under Wine and
macOS/MoltenVK. Linux additionally passes lobby/new-round and purchase checks.
The rendered check sends actual SDL paging, a remapped chord, text and channel
clicks, and verifies the ten-second return to the latest message without losing
the draft. Linux enables Vulkan validation; macOS enables Metal API and shader
validation. Original-size bitmap artwork, fonts and wrapped history captures
were reviewed. No pacing changes are included.

Current client/server source identity on all three platforms is
`a811b96d16342c23a79112beaf70394acf242c694c29b9da5b0ef538d9cc52ae`.
The Windows ZIP and deployed macOS development bundle include the matching
client/server and this audit; the latter has a verified ad hoc signature.

## Flight Boot and modal checkpoint (2026-10-06)

The authored F6 Boot button now opens the original `Boot <name>?` message box,
using the keyed dialog background, large bitmap font, shadow and OK/Cancel
buttons at `MessageBoxImpl`'s original offsets. Enter confirms; Escape and
Cancel abandon the action. The stored pilot ID and originating side are separate
from the pane's later selection. Membership, outstanding requests, leadership
and the team-settings lock are checked again when OK is accepted. Cancelling or
confirming preserves the underlying F6 pane and returns to the preceding hangar
or pilot view.

`ClientCore/LobbyActions.h` shares the original `BootSink` decision: an
outstanding request is rejected; otherwise enabled defections move the pilot
to No Team, while disabled defections remove them from the mission. Lobby
context Boot retains its separate original No Team behavior. The authority also
removes a pilot once every faction has banned them. These use the existing
reliable action and notices; no wire structures grow. Side removal supplies the
original personal message box and gameplay teammate chat/quit sound. Mission
removal retains the original commander-removal sentence; the unsupported ASGS
alternate-callsign login instruction is omitted.

Expelled peers are marked before the rest of the received batch is processed,
then disconnected and retired after action references have been released.
Their session cannot enter the dormant reconnect pool. The original remaining
gunner promotion preserves the crew, hull and human identity before retirement.
Local matches use the same confirmation/defection policy, rather than silently
dropping a flight-console action.

Message-box composition now has cached backdrop/dialog layers. Independent
crew, player and chat overlays draw below the dialog, and modal input cannot
activate or scroll them. The dialog is drawn once, last in the same ordered HUD
batch, with no additional render pass or per-frame layout copy. WinTrek and
platform-pacing code are unchanged.

The player-pane check covers original geometry, OK/Cancel/Escape, parent capture,
duplicate names, later selection, stale membership, leadership and lock changes
and both local defection policies. `allegiance-network-check --case boot` covers
the distinct F6/lobby authority paths, sender/side/self gates, faction bans,
all-faction expulsion, original notices, nonresumable retirement and real crew
preservation. Admission checks also cover confirmed request rejection under a
team-settings lock. The extended `--player-pane-smoke-test` drives actual SDL
clicks and keys in hangar and flight, verifies modal capture, and cancels and
accepts Boot while retaining the F6 overlay.

Session/player-pane checks, encrypted Boot/admission checks and the extended
rendered F6 interaction check pass on Linux/Wayland, Windows under Wine and
macOS/MoltenVK. Linux additionally passes the encrypted crew and lobby/new-round
regressions. The held-click check verifies the cached dialog's original pressed
button index and release-outside cancellation. Linux uses Vulkan validation;
macOS uses Metal API and shader validation. Original-size hangar/flight dialog
captures were reviewed, including overlap with the Mac's independent player
pane. All owned diagnostic processes have exited.

That Boot restoration checkpoint used client/server source identity
`f329b87175d50e073f6e8261dbbac48785794be0f0247894dc82e9e22e685f42`.
The refreshed Windows ZIP and deployed macOS development bundle contain matching
client/server builds and this audit. macOS retains a verified ad hoc signature.
Further macOS pacing work remains deferred.

## Faction ship-status restoration

FedSrv's per-side `ShipStatus` and its mission-entry bootstrap are the reference
for this pass. The authority retains initialized value records independently of
physical-sector reports. A joining pilot receives their faction's last-known
hull, sector, dock timestamp and parent/observer/turret state, including contacts
that left sight before they connected. Losing sight freezes the observed values
and marks them unknown; hidden hull changes and restarts do not update opponents.
No hidden ship pose, equipment or account accompanies these records, and stale
contacts do not create physical IGC replicas. Canonical identities invalidate
history when an ID changes incarnation or a pilot leaves/changes faction.

The records live in the existing cached faction arena. They are captured alongside
the existing ship traversal, sorted by identity, and encoded/delta'd once for all
recipients. Static status does not carry a moving position or continually changing
timestamp. The explicit 60-byte record includes a 16-byte status and pilot identity;
it avoids IGC's compiler-dependent bitfields, padding and uninitialized parent ID.
The snapshot envelope grows by eight explicit initialized bytes. Existing strict
build identity admission rejects incompatible clients; this adds no wire-version
negotiation or per-client status encoding.

Scanner edges retain brief encounters even with F6 closed. Sector departure
preserves the old observation before scanner eyes reset. Sighting, docking,
launch, hull/crew changes and known-aleph traversal update the authorized status.
Complete faction reports remove retired drones even when their death occurs in
an unseen sector and no explosion is sent to that viewer. Human recovery now
selects its restart before leaving flight, removes the dying pilot from the old
sector, then loads the pod and docks, as FedSrv does. This avoids the prior
transient pod sighting in the old sector. A human with no restart retains their
pilot identity with dead status and no known hull or sector. The original pilot
stealth indicator now uses the authority's detected flag rather than attempting
to infer opposing scanner sight from the replica's limited world.

`allegiance-player-pane-check` covers fresh-join history, scanner loss, absence
of hidden physical replicas, raw-record roundtrip, unseen drone retirement,
observed/unobserved human recovery and no-restart death. The encrypted
`allegiance-network-check --case intel` exercises real GNS late joining,
stealth detection/loss, shared faction capture, one encoding for 1,024 recipients,
status insertion/removal/reordering through delta relocation, and invalid-state
rejection. Existing raw-record checks verify deterministic bytes, including
nested status defaults. WinTrek and all platform pacing paths are unchanged.

The new status/late-join regression, player-pane and session checks, encrypted
health/throttle/crew regressions, and actual original F6 mouse/key interaction
pass on Linux Wayland, Windows under Wine and macOS MoltenVK. Linux additionally
passes encrypted sector, purchase, Boot, console and lobby/new-round checks.
The rendered interaction run completed in 1,333 Linux frames and 1,335 Wine
frames and 1,020 macOS frames; macOS used Metal API and shader validation. Linux Vulkan validation is
clean. Original-size captures were reviewed. These are functional validations,
not isolated performance benchmarks. At that checkpoint, all six client/server binaries used
source identity `d16eb397d09d0905da0030bb92c390441a07be3931a4bf5b666ea0bec80642ae`.
The Windows ZIP and deployed, ad hoc signed macOS bundle are refreshed together.
Further macOS pacing work remains deferred.

## Original orders and console tips

`console.cpp::ConsoleData`, `dialog.mdl`, WinTrek's queued-command/money
handlers and `teampane.cpp` are the references for this pass. The default
classic console uses the authored **Current orders** display, `comqueuedbmp`
and `comacceptbmp`, the original lowercase command verbs, colors, bitmap fonts
and top-left anchors. Target name changes update the retained command label.
The added centered request/order banner and centered autopilot-order label are
confined to the optional experimental interface.

Critical tips have four independently expiring slots (7.5 seconds normally,
30 seconds in training); ordinary tips retain three lines until replacement.
The original critical-tip wrapping reservation and training replacement rule
are preserved. Tips use the display clock, so pausing simulation does not keep
warnings visible. Chat and tips have separate display histories. Acceptance
posts **Command accepted.** and clears the newest critical slot, as WinTrek
does. The authored overlays are compiled once, cached across unchanged frames
and appended to the existing ordered HUD batch; fonts/images remain resident.
Integer UI scaling preserves their authored anchors and nearest sampling.
The chat image now uses its original lower-left anchor: its bottom is 126
pixels below the top of the screen. Its authored 355-by-125 `CuttingRect`
clips the grown chat list and excludes the hidden scrollbar. Mixed chat/tip
and large-font/history render checks cover this placement and glyph clipping.
The fixed authored offsets can still crowd a full chat panel and several
critical tips, as the cross-platform captures show; those offsets are retained.

Money requests keep the requested hull and requesting pilot's public identity,
and the exact original **Press the [Insert] key to approve it** prompt. The
server addresses this critical tip to its recipient instead of requiring that
recipient to be the current authority actor. A spare notice flag distinguishes
tips without enlarging the wire record. Request/order replacement, partial or
zero-balance approval, fifteen-second reply lifetime, late order acceptance
and affirmative/negative acknowledgements retain the existing shared policy.
Allied donations regain the original ordinary-tip confirmation. Default order
and training instructions preserve the original Accept wording and text;
remapping Accept changes the key label without substituting Execute/autopilot.
The canonical Insert label remains when the shipped keymap still accepts
Insert, even if its primary Accept entry is the bracket key.
Docked investment/teleport chat now routes text and cancellation to the same
inline composer as flight and hangar chat; the live test checks that Escape
cancels the draft while retaining the investment pane.

`allegiance-console-check` covers authored images/geometry/blending, four/three
slots, independent expiry, paused display time, wrapping, training replacement,
cached layout, preference changes and integer scaling. The encrypted
`allegiance-network-check --case requests` covers recipient-only prompts,
request/hull identity, duplicate reports, acceptance/rejection/execute,
replacement, conservation and expiry. `allegiance-render-check` checks actual
Vulkan order/tip pixels, additive composition, expiry, cache stability and
absence of a second banner or chat copy. The rendered
`--console-smoke-test` also exercises the mapped Insert/Delete actions and
captures money, queued and accepted-order states. WinTrek and platform pacing
paths remain unchanged.

Console, session, player, encrypted queued-request and full render checks pass
on Linux/Wayland, Windows under Wine and macOS/MoltenVK. The live console check
passes 950 frames on each, including Research typing/Escape, mapped approval
and rejection, and authored queued/accepted-order captures. Linux uses Vulkan
validation and macOS uses Metal API and shader validation. These are functional
diagnostics, not presentation-latency or performance measurements. At that
checkpoint, Linux, Windows and macOS client/server source identity was
`419abdb16c72a82c50109d732f712b0c6531b9d2e72f85043f2614da5ff89dba`.
The Windows ZIP and deployed, ad hoc signed macOS bundle include matching
client/server builds and this audit. Further macOS pacing work remains deferred.

## Original Game Details popup

`WinTrek.cpp::OnGameState`, the read-only `NewGameScreen` constructor,
`mmlpopup.cpp`, `pagepane.cpp` and `mmlpane.mdl` are the references. Escape-menu
**Game Details** now opens the original centered, unkeyed dialog artwork with
its authored close button and scrollbar. It no longer opens the native mission
settings adapter. The yellow bitmap-font heading, white report, field order,
blank paragraphs, lowercase **yes/no**, raw allowed-death count and nearest
authored map/connectivity/resource/money/imbalance labels follow the original.

The report snapshots the active IGC mission parameters on opening. Word flow
is prepared once; scrolling translates retained text controls inside the
450-by-220 page. Arrow clicks and repeats advance eight pixels, paging advances
one page, and the thumb reaches the last fields. Other keyboard input is
captured; Enter, Escape and the authored close button dismiss the popup. The
original outside-click dismissal is retained. The parent's actions remain
blocked while it is open; top-popup input cannot edit or send a suspended chat
draft, or select an underlying wing popup. Hangar state and staged
fitting are retained, and flight or a docked command view resumes on dismissal.
Integer UI scale changes retain the report without re-reading mission state.
A cached top layer keeps the popup above independent console panes without
duplicating the report or its parent in the HUD batch.

The native server endpoint (or **Standalone**) and active core filename are
shown. Legacy authenticated directory server names, friendly core names and
green official-server/core markings still require their directory/configuration
service equivalents; this popup does not claim official status.

Session checks cover the IGC snapshot, exact paragraphs, original controls,
read-only capture, scrolling/repeat, retained report, scaling and restoration.
The render check covers actual Vulkan background/font pixels, clipping,
scrolling and the preserved world scene. The live console smoke check uses
actual SDL Escape/I input, text/key capture above a suspended draft, authored
scrollbar clicks above a suspended wing popup, and Enter to return to command
view without changing its accepted order or sending its draft.

Session, console, encrypted queued-request and full render checks pass on
Linux/Wayland, Windows under Wine and macOS/MoltenVK. The live console check
passes 950 frames on each platform, including the suspended draft/wing-popup
cases. Original-size popup captures were reviewed on all three. Linux Vulkan
validation and macOS Metal API/shader validation are clean. These are functional
diagnostics, not performance or presentation-latency measurements. All six
client/server binaries use source identity
`cc36e2a0ffb574f571d46c2650ae8a85b1f50b6ad2dc708256b15636f1c6f21a`.
The Windows ZIP and deployed, ad hoc signed macOS bundle include matching
builds and this audit. WinTrek and platform pacing paths are unchanged in this
pass; further macOS pacing remains deferred.

## Original ballots and voting

`FedSrv/fsmission.cpp::Ballot`, its resign/draw/mutiny subclasses,
`FedSrv.CPP` chat-command dispatch, `clintlib/appmsg.cpp`,
`BaseClient::GetCurrentBallot/Vote`, `WinTrek.cpp` vote keys and
`dialog.mdl::VoteTextImage` are the references. The native authority now
handles the original case-insensitive `#resign`, `#draw` and `#mutiny` chat
prefixes. Eligible human pilots are snapshotted at proposal time; drones and
later arrivals do not vote. The proposer automatically votes yes. There is
one outstanding ballot of each proposal type across the match, with the
original private duplicate feedback.

Ballots last thirty seconds and settle earlier when all votes arrive. Each
participating side needs strictly more yes than no; abstentions are displayed
but do not count against a proposal, and ties fail. The original prompts,
result strings, side-name/count order and FIFO queue are retained. The
commander silently votes no on a mutiny. A successful mutiny transfers command,
the owning side's game-owner controls, the old commander's funds and incoming
auto-donations. A successful resignation eliminates that faction. A passed
team draw offer goes to humans of the other active factions and requires each
participating faction's approval. An accepted draw freezes the original
no-winner result and returns network players to the next-round team lobby.

The HUD uses the authored centered, white, 620-pixel `medBoldVerdana` vote
image and its original chat-relative top anchor, including the loadout's
chat-line limit and integer UI scaling. The MDL template is compiled once;
retained controls are rebuilt only when displayed text or layout changes.
Original Y/N commands import from the shipped maps, can be remapped, and
work in flight, hangar and the docked investment/teleport consoles. Chat
composition and top modal popups retain keyboard ownership. The original
hardcoded Y/N instruction text remains even with remapped keys.

Network ballots use the existing private acknowledged-notice stream and
reliable input-action lane. They add no periodic status fields or new packet
stream. Stable IDs and sorted voter snapshots avoid retaining IGC pointers or
walking the mission on each vote. Malformed, duplicate, foreign, expired and
late-arrival votes are rejected. A voter who changes factions cannot apply a
vote to the new faction's tally; the old vote remains an abstention. This
closes the original cross-side tally hazard while keeping normal vote rules.

Console checks include an independent exhaustive 729-combination, three-side
majority oracle and real IGC tests for overlapping/FIFO proposals, global
uniqueness, drones, later arrivals, strict expiry, mutiny funds/donation
cascades, resignation and three-team draw completion. Encrypted tests cover
eligibility/privacy, muted proposers, automatic commander opposition,
leadership/game-owner and balance replication, invalid/duplicate/late votes,
notice acknowledgment, timed-out ties, draw acceptance and new-round reset.
Wire parsing tests cover wide stable ballot IDs and malformed notices.

Console, keyboard, session, player, endgame, encrypted ballot/queued-request
and full render checks pass on Linux/Wayland, Windows under Wine and
macOS/MoltenVK. The live console check passes 1,030 frames on each platform,
including actual mapped Y/N, FIFO advancement, chat text capture, Game Details
capture and voting above the investment console. Vote captures were reviewed
on all three. Linux Vulkan validation and macOS Metal API/shader validation
are clean. These are functional diagnostics, not performance or latency
measurements. All six client/server binaries use source identity
`38d360bb4cc23b40de4cecf7b37f34280bc804a3064fb020c6f547be6f317f07`. The Windows ZIP and deployed, ad hoc signed macOS
bundle include matching builds and this audit. WinTrek and platform pacing
paths are unchanged in this pass; further macOS pacing remains deferred.

## Original mission chat and global main-menu parity

Audited `FedSrv.CPP`'s `CS_CHATMESSAGE` handling, `CFSMission::SetAutoAccept`
and `SetMaxTeamImbalance`, `BaseClient::SendChat`, and WinTrek's global
`OnTrekKeyFilter(TK_MainMenu)`. The native adapters now retain these behaviors:

- `#autoaccept` toggles only the issuing commander's faction in a started
  match, preserves its selected chat audience, and clears/revalidates existing
  applicants when enabled. Invalid applicants stay in No Team without a stale
  pending request. The underlying admission action is also available during a
  match; the original lobby button retains its original disabled state.
- `#autobalance [1|2|a|n]` is restricted to the mission owner in a started
  match. It changes the original IGC imbalance setting without resetting the
  match, ships, funds or stations. Its forwarded chat goes to Everyone while
  the author's local echo retains the originally selected audience. Recognized
  unknown parameters still broadcast without changing policy.
- `#list` consumes the command and privately returns the original HQ help
  text to a real-team pilot during a match. Before start, or from No Team, it
  remains ordinary chat. Case-insensitive prefix matching, minimum lengths,
  spacing and suffix behavior follow FedSrv.
- `#ban <name>` is a separate mission-owner action, available before or
  during the match. It only removes a human from No Team, uses the exact
  case-insensitive full name without trimming, and consumes an authorized
  command even when no target matches. Retirement uses the existing deferred
  connection/model cleanup. The original owner-boot reason is displayed;
  unsupported ASGS account/relogin instructions are omitted. This is not an
  authenticated administrator ban or a persistent account ban ledger.
- Typed outgoing messages appear locally immediately and once, including
  server-consumed commands. Forwarded reports exclude their author. Ordinary
  individual messages addressed to self, including the original default `NA`
  recipient, remain local and do not invoke server commands or send an action.
  Unassigned replicas reject chat before bootstrap.
- The mapped main-menu key follows WinTrek's global filter priority above
  the original hangar and investment panel. Chat composition, original popup
  dialogs, text editing and key-mapping capture retain their input. Closing
  the original menu and Game Details restores the underlying panel.
- Ordinary chat is now drawn over the original docked hangar, refit,
  investment and teleport backgrounds. These paths reuse the flight chat's
  resident glyphs, wrapping/history cache, anchors, clipping, paging and the
  original six-line loadout cap. Original options/main-menu popups have cached
  separate background and top layers, so chat and console panes appear below
  the menu without drawing its artwork twice or rebuilding text each frame.

Shared live policy changes use one prepared immutable control record for
all connected recipients, retaining the current native control lane and strict
build/ABI identity. Ordered updates survive late welcome and next-round
bootstrap; older physical snapshots do not contain or overwrite this policy.
The frontend reads the live IGC rule for its original Game Details report.

Validation passes on Linux, Windows/Wine and macOS/MoltenVK: console/session,
the installed keyboard maps, players, endgame, player pane, Game State, crew,
the original training sequence, encrypted mission commands/chat/ballots/
admission/requests, and the full rendering diagnostics. The live console
check runs 1,220 frames on each platform, using actual mapped chat/main-menu
keys and Enter, private help, live policy changes, the authored Game Details
scrollbar and commander admission toggles. Docked chat, original clipping,
the six-line refit cap, unchanged wrapping-cache reuse and separate resident
main-menu layers have regression coverage. UI captures were reviewed on all
three platforms. Linux Vulkan validation and macOS Metal API/shader validation
are clean. These diagnostics do not measure production performance or latency.

All six client/server binaries use source identity
`5a37d2c6de05513e7b0f8b3caafea012af1ede179ad7e62225d4bcda8a959cac`.
The refreshed Windows ZIP and deployed, ad hoc signed macOS bundle contain
matching builds and this audit. WinTrek/FedSrv, renderer and platform pacing
sources are unchanged in this pass; further macOS pacing remains deferred.

## Communication pane and popup/input equivalence batch

Audited the complete path through `chatpane.cpp`, `ListPaneOld` in
`trekctrls.cpp`, `DisplayImage`/`ConsoleImpl`, the original popup keyboard
container, and the installed `dialog.mdl`. This batch groups the following
observations and fixes rather than treating each as a separate build cycle:

| Surface | Original behavior and result |
|---|---|
| Mapped chat paging | PageUp/PageDown advance a full page of visible slots. Removed the native one-row overlap. |
| Empty and short histories | PageUp disables autoscroll for ten seconds even when there is no scroll offset. PageDown with an empty list also keeps it disabled; with the newest row visible it restores following immediately. New messages retain the historical page's first row, including after bounded-history eviction. |
| Font and line-count changes | The original rebuild restores autoscroll. Native font/line changes and the loadout row change now do the same while retaining the cached wrapping when its inputs have not changed. |
| Padding and visible rows | The list is `configuredLines * boldFontHeight + 8` pixels high; visible slots are integer division by the bold font height. Preserve the additional slot possible with a small font and the original six-line loadout setting. |
| Artwork selection and clipping | Installed `ShowScrollbarOnCockpitChat = 1` selects the full CommPane. It is not always cropped to 355x125. Restore the authored background dimensions, growing list surface, three-pixel text inset, bottom anchor at y=126, and the alternative fixed crop policy. |
| Background transparency | `ConsoleImpl` supplies a black color-key hint to the composed PaneImage. The RGB black background must be transparent. Removed the opaque native rectangle; the original resident image is keyed once and entirely transparent artwork emits no draw. |
| Chat persistence | Critical console tips expire independently; messages retained in the classic chat list do not fade merely because they carry a warning flag. Preserve the independent notification/history surfaces. |
| Text colors | Original chat colors are authored display RGB, like other classic interface text. Convert them into the renderer's linear color representation when caching new/wrapped messages rather than brightening them or converting each frame. |
| Popup pointer capture | Original popups capture clicks and wheel events ahead of the underlying scene and panes. Prevent options/forms from selecting an F6 player, changing background chat, manipulating the map, or zooming/ordering the command scene. |
| Popup keyboard/text focus | The original popup container consumes keyboard messages. Route F6, paging, Escape and other keys to the foreground popup; route text to its own team-name/filter/address editor while preserving background chat drafts. Message boxes and Game Details retain their own capture rules. |
| Command-view wheel accumulation | Continuous fire buttons are disabled in command view, but wheel input remains available. Native button sampling was clearing fractional detents every frame. Separate wheel availability from held-button availability, retaining focus-loss reset and suppression of held fire. |

Two easily misread original details are deliberately retained. The image named
“with scrollbar” does not enable the internal scrollbar: `ChatListPane`'s
authored final argument is false. The flight CommPane's display tuple also marks
it **undetectable** to mouse hit testing. Its wheel and clicks therefore pass to
the flight/command view; history uses the mapped paging commands. No new visible
scrollbar or mouse interaction is added. Critical notifications already use
their authored 620-pixel wrap width, and ordinary tips/orders retain their own
widths; these were checked and left unchanged.

The geometry rules are factored into `ClientCore/CommPanePresentation.h` without
changing WinTrek. Artwork scalar selection is compiled once at HUD creation.
Resident bitmap fonts, image descriptors, append-only wrapping caches and the
ordered native HUD batch remain in use. This pass changes no renderer,
WinTrek/FedSrv runtime, transport or platform-pacing code.

Validation passes on Linux, Windows/Wine and macOS/MoltenVK: console/session,
installed keyboard maps and mouse/controller routes, original UI scripts,
controls, players, F6 pane, Game State, crew, endgame, loadouts, teleport, the
original training sequence, encrypted mission chat/chat/ballots/admission/
requests, and full rendering diagnostics. Synthetic coverage includes 48
font-height/line-count/loadout/crop combinations, plus 24 HUD font/list
combinations, empty/short-history holds, eviction and independent expiry.

Each platform completes the 660-frame chat/CommPane flow, the 1,220-frame
console flow and the 400-frame original team-settings/briefing flow. These use
actual SDL wheel/key/text/click events to check pilot/command wheel pass-through,
mapped flight/docked paging, popup wheel/F6/paging capture, background F6 click
suppression, text editing/cancellation, and hidden-pane release. The command
test advances the simulation in bounded steps to settle the original two-second
camera entrance before checking zoom; its production behavior is preserved.
Captures were reviewed on all three platforms. Linux Vulkan validation and
macOS Metal API/shader validation are clean. These checks make no production
performance or presentation-latency claim.

All six client/server binaries use source identity
`5c84c160ea7dc20a77c1a33b79e5e8f711d4ba2d0ae308a68c4ea72f23de3fce`.
The Windows package/ZIP and deployed ad hoc signed macOS bundle are refreshed
together with this audit. The remaining queue below still applies; this batch
does not claim full original UI equivalence.

## Authored HUD equivalence batch

Compared eighteen authored surfaces together against installed `dialog.mdl`,
`cortui_huds.mdl`, inventory MDLs, `console.cpp`/`consoledata.cpp`,
`inventoryimage.cpp`, and WinTrek's HUD options/color code. The native client
now compiles their image descriptions at load time into indexed arithmetic and
ordered native draw records. It retains resident images/fonts and reusable
frame storage; it does not instantiate the original Image/Pane renderer.

| Observation | Result |
|---|---|
| HUD style selection | Restore the current WinTrek six-slot cycle, original menu labels and `H` accelerator, immediate application, persistence and `StyleHUD` configuration import. Default remains Normal. Compile Classic, Raveen, lessHUD, Glass, Parci and authored Jello together. |
| Center instruments | Use authored grouping, layering, anchors, gauge images/rectangles and individual style layouts instead of a manually approximated Normal HUD. |
| Fonts and text | Restore original bitmap font symbols, exact label widths, justification and wrapping. The non-default inline Verdana fonts use resident FreeType coverage at their authored cell heights. Preserve original `NumberString` conversion `(int)(value + .5f)`. |
| Gauge clipping | Use original integer crop lengths, top/bottom/left/right directions and mirrored source rectangles. Crop the artwork rather than stretching the completed gauge. |
| HUD colors | Restore faction tint, brightness and separate player/target opacity; preserve original shadow bytes produced by the historical unmasked D3DRGBA packing. |
| Damage and detection | Restore the authored flash/detection branches, including Glass's state-dependent source images and the undetected-eye indicators. |
| Carried objectives | Render original flag/artifact images at authored positions instead of the added classic text banner. |
| Target display | Restore style-specific target name/type widths, range/speed/armor/shield text, range image and ship-only experience display. Remove added classic reload/lock text. |
| Multiple target models | Preserve both authored target views in Glass and Jello. A bounded ordered preview chain renders each model at its own rectangle and insertion point, retaining the native instance arenas and draw batches. |
| Command instruments | Restore original icons and ship status/target group at the left-side command anchor, plus Glass detection text. |
| Station instruments | Restore ship type/kill bonus, station name/sector and observed hull/shields on hangar/loadout/station command overlays. These are client presentation fields, not additional network reports. |
| Pod and ripcord | Replace native classic lifepod/ripcord banners with authored oxygen/endurance and countdown displays; read the existing IGC endurance value. |
| Death/lives/kills | Restore original death artwork and conditional lives/kills images. Retain the authored countdown toggle's actual WinTrek zero state rather than inventing a duplicate timer. |
| Sector labels | Restore Current location / Currently viewing text, original fonts/colors and top-right anchors. |
| Panel skins | Switch inventory and minimap artwork with the HUD style. Preserve Glass's different inventory cap width/height. |
| Inventory layout | Restore fixed bottom title/tab anchors, eighteen-pixel background rows, original text columns, equipment icons and slot labels. Draw the ship title after the background. Remove added classic cargo heading/row selection/hover decoration. |
| Mounting/arming | Use original resident 64x14 background/gauge artwork, integer fill and authored status-column offset, replacing rectangles with substituted text. |
| Mouse transparency and crew | Preserve the original undetectable inventory display's mouse transparency. Restore Glass's shifted, faction-colored turret header/pilot-name variant while retaining default and loadout crew behavior. |

The HUD binder exposes existing local/replicated client observations only.
Signature display now includes the original faction signature divisor. No
WinTrek/FedSrv source or frame-pacing policy changes are part of this batch.

Twenty-two affected targets build on Linux, Windows (Clang/MSVC ABI) and
macOS arm64. Thirteen checks pass on each platform: UI-script, session, crew,
console, keyboard, controls, players, player-pane, game-state, endgame,
loadouts, teleport and the full original-training check. Five encrypted
loopback cases also pass on each: mission chat, chat, ballots, admission and
requests. These exercise the presentation changes with the existing gameplay
and network authority policies.

Rendered checks cover all six styles, the two independent Glass/Jello target
previews, command/station/pod/ripcord/death states, original gauge crops,
inventory artwork and mouse transparency. Each style runs 512 steady-state
HUD builds without creating GPU textures. CPU build p99 in this synthetic
fixture is:

| Style | Linux | Wine | macOS M1 |
|---|---:|---:|---:|
| Normal | 3.72 µs | 5.80 µs | 5.62 µs |
| Software | 2.96 µs | 4.60 µs | 4.25 µs |
| Custom Hud1 | 3.06 µs | 4.50 µs | 4.46 µs |
| Glass | 6.93 µs | 8.90 µs | 8.96 µs |
| Parci | 2.85 µs | 4.70 µs | 4.17 µs |
| Jello | 4.51 µs | 6.40 µs | 5.96 µs |

These are HUD construction timings, not whole-frame or presentation-latency
measurements. All three platforms complete the 660-frame chat, 1,220-frame
console and 400-frame team-settings/briefing live SDL input flows. Captures
were reviewed across the three platforms. Normal, command, station,
pod/ripcord and death fixture captures are byte-identical between Linux and
Wine; macOS preserves the same layout/artwork with minor rasterization and
clear-color rounding differences. Linux Vulkan validation and macOS Metal
API/shader validation report no errors. Wine uses its native Vulkan bridge
without the incompatible Vulkan validation layer.

All six client/server binaries use source identity
`6cfae5397ecc996f039e1404262050ecaedf92494520b84178324744f5f3e046`.
The Windows package/ZIP and deployed ad hoc signed macOS bundle are refreshed
with these binaries and this audit.

This is a broader completion checkpoint, not a claim that all inventory behavior
or UI parity is finished. Remaining inventory work includes authored per-part
statistics, cyclic selected-cargo ordering/quantity coalescing, empty compatible
mounts and observer/turret rows. Complete those with original MDL/IGC policies
and dedicated mixed-loadout/crew comparisons, not prototype labels.

## Asset-driven inventory and HUD overlays

The classic inventory now compiles each part type's `GetInventoryLineMDLName()`
namespace instead of formatting substitute labels and drawing a parallel C++
layout. The MDL compiler supports the installed inventory's ImagePane,
StringPane, GaugePane, switched panes/strings, original numeric formatters,
image dimensions and grouped right alignment. Native C++ supplies original
IGC observations and equipment/crew row membership.

| Observation | Result |
|---|---|
| Core-selected descriptions | Preload every distinct inventory namespace named by the active core, plus original empty/observer/turret descriptions. Different authored labels, widths, colors and positions need no C++ variant. |
| Row order and keys | Share the original fixed-gun, observer, turret, equipment and cyclic selected-cargo order; preserve pilot-only weapon/turret keys and the original cargo `S.` key. |
| Turrets and passengers | Use the source ship's inventory, preserve occupied-gunner rows and observer counts, and retain the actual local gunner selection. |
| Empty mounts | Show supported empty mounts only when an available part can mount there, following the original policy. |
| Cargo quantities | Preserve physical representative selection and original duplicate coalescing; normalize ammunition/fuel packs by hull capacity rather than printing substituted raw counts. |
| Gun and missile statistics | Share original range/damage/rate calculations, projectile blast contribution and viewer-side global attributes. |
| Boost and cloak | Preserve original maximum speed, viewer fuel/energy endurance and authored infinity captions. |
| Shields | Supply original strength and time-to-full statistics; MDL chooses their labels and formatting. |
| Readiness | MDL selects mounting/arming/status branches and draws the original gauge artwork with integer crops. |
| Selection and activity | Preserve authored gun colors, selection/activity indicators and original out-of-ammunition policy. |
| Text | Use original smallFont, declared StringPane dimensions, complete-character clipping and two-significant-digit RealNumberString formatting. |
| Inventory shell | Compile `MergedInventoryPaneImage`, including ship title, tab, background lines, translations and grouping. Retain the original authored seventeen-background-line limit. |
| Column dimensions | Use actual resident row/cap sizes; preserve the original 220-pixel column versus 214-pixel Classic skin and the resulting right-alignment shift. |
| Color key | Preserve ConsoleImpl's global color-key hint, including imported files whose local color-key flag is false. |
| Lifetime and storage | Copy IGC observations into owned reusable rows. Compiled programs discard namespace/AST references and retain resident fonts/images and draw/string storage. |
| WinTrek cost | Ten PartWrapper getter bodies move unchanged into inline ClientCore helpers; there are no added WinTrek passes, allocations or dispatch layers. |

`dialog.mdl::dialogs` also compiles into an indexed native overlay table. All
39 installed entries retain their authored screen anchors, off/on points,
visibility masks, transition durations and mouse-transparency flags. The
classic HUD's eighteen instrument/status roots, inventory, minimap and command
bar consume these records and paint in the original reverse table order.
Changing or removing their table entries changes placement or visibility
without modifying C++.

The table supports authored arithmetic, bit masks and dynamic point expressions
such as the software HUD's chat-line offset. Native coordinates preserve the
original lower-left image origin and all nine screen anchors. Mode/toggle
changes use WinTrek's linear endpoint-to-endpoint slide, including its restart
from the endpoint when direction reverses. A newly attached native view starts
settled; initial attachment therefore omits the original entrance animation.
Closing map/command panels stop receiving input immediately. MDL VisibleImage
options hide their contents immediately, independently of the overlay slide.

This inventory checkpoint left tips, chat composition, team, research,
teleport and Game State with separately authored placement/slide constants.
Those hosts are migrated in the subsequent panel-adapter batch below. Dynamic list item painters, popup/widget composition,
additional host size bindings and non-stock stateful colored gauge flashing
remain further asset-interpreter work. Selection, purchase, commands, network
validation and other gameplay rules remain existing client/server policies.

Validation for this portable batch is Linux-only, following the requested
validation policy. The earlier Windows/macOS release packages remain at the
previous published checkpoint; they are intentionally not refreshed here.

UI-script, inventory, session, crew, console and loadout checks pass. Frozen
original comparisons cover 292 hulls, 7,325 row/order/empty/stack/crew cases and
4,745 part-statistic comparisons. MDL checks cover 456 style/row/readiness
combinations, all six inventory shells, twenty independently specified overlay
records, alternate asset descriptions and programs surviving the destruction
of their source namespace. Encrypted crew and authoritative purchase/recovery
loopback cases pass. No network record changes are introduced by this batch.

Linux rendered checks pass with Vulkan validation enabled: all six styles,
ordered target previews, command/station/pod/ripcord/death, map entrance/exit
and input, retained popup hits, inventory transparency and actual alpha
cutouts. Captures were reviewed. The two-row, 512-frame-per-style synthetic
HUD build p99 is 6.25 / 4.65 / 6.43 / 8.20 / 6.34 / 6.32 microseconds for styles
0 through 5, with no GPU texture growth. These measure HUD construction only.
A stale diagnostic assumption that panels immediately occupy their final
position was corrected to wait for the restored entrance animations.
The 1,220-frame live console flow also passes with actual SDL input: chat,
command recipients/verbs, typed orders, quick chat, research, queued/accepted
orders, mission details/scrolling and vote prompts. Linux Vulkan validation
reports no errors. Only diagnostic schedules were changed to wait for the
restored slide; production frame pacing is unchanged.

The Linux client/server source identity at this inventory checkpoint was
`73bca633e3a965548270df0a219a20aafa047d48d7f49510a8fd3fe05b27f2a8`.
All-platform packages will require rebuilding together at the next release
checkpoint, because the existing same-source network compatibility policy
rejects the previous packaged clients connecting to this newer server.

## Authored panel adapters and console compositions

The remaining native teleport, research, F6 compact/expanded roster, Game
State, inline composition and command/tip/vote hosts now bind their original
`dialog::dialogs` entries. Placement, screen anchors, durations, masks and
mouse-transparency are compiled once. Changes in an authored overlay entry
therefore do not require a new positioning variant in native C++.

| Observation | Result |
|---|---|
| Teleport | Destination-list and scrollbar rectangles and painter column boundaries come from `teleportpane.mdl`. Preserve the original fourteen-pixel row painter and its width-growth rule. |
| Research | Compile `MergedInvestmentsPaneImage` around host-bound purchase/info dimensions. MDL supplies the three component offsets and the complete credits image/labels/fonts/clipping. Preserve the existing investment actions and original C++ card painter. |
| Credits alignment | `StringPane`'s sixth `true` argument overrides its center justification with original left alignment/clip-right behavior. The previously centered native credit amount now follows that policy. |
| Compact/expanded F6 | Read list, scrollbar, team/combo/icon/selected-team rectangles and column arrays from `teampane.mdl`. Preserve original row heights, width growth, fixed expanded-name clipping and compact unrestricted-name policy. Retain the selected layout during its exit animation. |
| Game State | Bind the dynamically sized original mission/standings host into `gameStateImage`. Interpret nested right/top/left/vertical-center justification and the authored close-button composition. The installed sixteen-pixel button sits four pixels below the header top; the previous native adapter placed it twenty pixels down. Honor this PaneImage's color-key flag and align its whole group to pixels even when the host height is odd. |
| Chat composition | The overlay entry supplies the composition location and entrance/exit duration. The command wing popup follows its authored combo rectangle and command-panel anchor. |
| Orders/tips/votes | Compile complete LastOrder, translated critical/vote and three ordinary-tip graphs into native instructions. Their internal translations, bitmap fonts, colors, blend modes and reverse table paint order now come from MDL. The critical wrapping threshold uses its authored width and font. |
| Console color key | Carry WinTrek ConsoleImpl's global color-key hint into the F6, teleport and chat host layouts and research surfaces. The F6 bitmap's black area above its roster now reveals the scene. Main-menu backgrounds retain their own authored import policy. |
| Input | Hit regions use the same animated screen-anchor transform as drawing, including odd window dimensions. Mouse transparency applies to the console layer while separately opened popup items remain interactive. |
| Mode changes | A shared native mode calculation supplies hangar, pilot, command, loadout and cinematic flags. Pane visibility/layout invalidates when that context changes. |
| Runtime storage | Keep fonts/images and compiled arithmetic resident. Retained console controls change on notices, layout, modes, expiration or display preferences; unchanged frames do not traverse MDL or rebuild their control vectors. |
| WinTrek | This batch changes only native adapters and native MDL interpretation. It adds no WinTrek work. |

The native MDL layout compiler also interprets integer lists,
StringColorComboPane slots, dynamic host sizes, general image-justification
flags and PaneImage color-key hint scoping. An independent two-pixel PNG/MDL
fixture checks opaque black, unchanged nonblack pixels, transparent BlackPane
fills and restoration of the hint between images. Host slots are deliberately native draw/layout data, not legacy
Image/Pane instances. Gameplay selection, command, investment and ranking
policies remain separate from authored appearance.

Linux UI-script, console, F6 player, Game State and teleport checks pass.
The new independent layout checks cover four original column arrays, combo/list
slots, research host composition and credit clipping, three dynamic Game State
sizes, all nine screen anchors, 1x/2x and odd window sizes, authored entrance/exit
durations, zero-duration changes and mouse transparency. Existing notification,
roster, authority, scrolling, keyboard, crew and teleport behavior checks pass.
Linux Vulkan checks pass with validation enabled. The new Game State probes
compare every close-button pixel against its keyed bitmap and underlying
header; F6 probes cover both opaque artwork and its keyed upper border.
Research/teleport placement, slides and hits, original tip/order/vote pixels,
all six HUD styles, crew, map, setup and postgame checks also pass. Reviewed
captures include Game State, F6, research, inline composition and ballot/tip
text. No Vulkan validation errors or per-frame texture growth are reported.
The 512-frame-per-style HUD construction p50 is 5.07 / 4.38 / 4.51 / 7.03 /
4.41 / 5.40 microseconds, and p99 is 8.29 / 6.08 / 4.68 / 8.44 / 5.02 / 6.25.
These describe the diagnostic HUD build, not complete game frame times.

The final 1,220-frame live console flow passes with actual SDL input: chat,
recipient/verb clicks, typed commands, quick chat, research parent retention,
critical money/order notices, mapped acceptance/rejection, details/scrolling,
ballots and mission commands. Production frame pacing is unchanged.
The final logs are `/tmp/allegiance-mdl-panels-render-console-key.log` and
`/tmp/allegiance-mdl-panels-live-console-key.log`; live captures are in
`/tmp/allegiance-mdl-panels-live/`.

The Linux client/server source identity at that checkpoint was
`9e63a83cad507b23e4ec2e5d0f1584b9a8afc8b75e31b53ea2ce97eaef106be5`.

This is a portable batch: Windows/macOS rebuilds are deferred until platform
code changes or a release checkpoint. Native source identity still changes
under the existing exact-source compatibility rule; release packages must be
rebuilt together when refreshed.

At that checkpoint, reusable stateful widgets, inline/system-font bindings outside the HUD,
non-stock flashing gauges, more dynamic list/card painters, general composed
surface color-key semantics and initial console attachment animation remained
interpreter work. The HUD/tip layers still attach settled as recorded above;
subsequent panel openings/closings use their authored transitions. There is
no claim of complete MDL or original UI equivalence.

## Model-card artwork, Games List and shared authored fonts

This Linux-only batch addresses related asset/compiler/painter differences
without reinstating the original Image/Pane renderer.

| Observation | Correction |
|---|---|
| Research cards | The existence check passed a namespace name to the filename-only `Assets::find`. Consequently every real model card took the blank fallback. Resolve namespace/file presence consistently with `Assets::image`; use the original `i<model>bmp` card when present and preserve the missing-picture fallback. |
| Loadout images | The same filename check discarded the authored slot prefix. Let the asset adapter resolve that prefix before its existing `l<model>bmp` fallback; verify an independent alternate prefix as well as equipped stock slots. |
| Games List geometry | Read list/bar rectangles and eight column boundaries from MDL. Preserve the original row painter's minimum-width growth, twenty-eight-pixel rows, integer-centered state icons, actual column clips, slot/count offsets, unnamed-game label and independent development/lives icons. |
| Raw painter clipping | Original DrawString painters clip glyph pixels at a surface boundary. They do not drop a whole last glyph just because its advance exceeds the column. Keep that policy separate from StringPane's character clipping. |
| Games List clock | Factor the existing countdown, negative-clock-skew and long-duration formatting into an inline ClientCore helper used by both clients. WinTrek retains the same branches and labels, without new per-frame state or allocation. |
| Hover columns | Compile each HoverPaneColumns boundary and ID into a non-drawing hover region. Header and list columns use the same authored help strings. Hovering a visible disabled button can still enter its original wrapper; hidden child controls and modal-covered regions remain inactive. Button-bar regions follow their individual authored buttons. |
| Hover composition | Find the SwitchString table through its actual HoverSite binding, including namespace qualification, instead of requiring a global variable called hoverString. Arbitrarily named StringImage/StringPane help displays retain their authored font, clip and slide transform. Separate sites do not share their help text. |
| System fonts | Interpret the installed Win32Font Verdana declarations in menus and composed images as well as HUD programs. The asset layer owns one lazy system-face pair and resident font cache; layout and drawing use identical cell-height/weight metrics and glyph atlases. Move the existing platform face lookup unchanged into that service. |
| Large glyphs/default fonts | Grow system-font atlas cells when needed instead of truncating advances at thirty-two pixels. Preserve StringPane's omitted-font default and measure its omitted-size form with actual font metrics. |

Linux builds, the complete UI-script and session checks, and the two-core
artwork audit pass. The audit now loads **246 distinct original research-card
images**; 423 references across those cores have no corresponding model card
and retain their original fallback. All 383 models and 431 image references
in the extended audit decode without failures. These are asset-reference checks,
not a claim that every card/gameplay state has received visual comparison.

New independent checks cover case-insensitive file/namespace presence,
alternate loadout prefixes, eight header/list help regions, independently named
hover sites and displays, stock system-font descriptors, shared menu/HUD font
identity, exact layout metrics, cache reuse and a forty-eight-pixel glyph.
Original Games List clock edge cases and equipped loadout images pass alongside
real IGC investment, purchase, lobby, input and session behavior checks.
Older slide assertions now check authored screen anchors and whole-pixel
HiDPI placement, including odd window dimensions.

The Vulkan render check passes with validation enabled. It compares opaque
interior pixels of an actual research model card with its source bitmap,
checks partial-glyph UV clipping, independently routed/clipped hover displays,
visible-disabled/hidden-target behavior, modal blocking and sixty-four repeated system-font menu builds without texture
growth. Existing six-style HUD, cursor, training, panel, map, Game State/F6,
notification, details, crew, setup and postgame checks pass. HUD construction
p50/p99 values in microseconds were 5.20/6.04, 4.51/5.21, 4.68/4.81,
7.16/9.05, 4.54/5.14 and 5.51/8.22 for styles 0–5; these measure CPU HUD
construction, not total frame time.

The live 1,220-frame console flow also passes with actual SDL input and Vulkan
validation. Review of the live research capture confirms the original miner,
tower, mine-layer and station pictures now appear. Logs are
`/tmp/allegiance-assets-ui-script.log`, `/tmp/allegiance-assets-ui-session.log`,
`/tmp/allegiance-assets-ui-audit.log`, `/tmp/allegiance-assets-ui-render.log`
and `/tmp/allegiance-assets-ui-live.log`; captures are under
`/tmp/allegiance-assets-ui-live/`.

The Linux client/server source identity at that checkpoint was
`387342d88bbe5f65ed08537d121d4443235b2d280bcacef1a6b3fdb4f98642f8`.
Windows/macOS packages were deliberately not rebuilt for this portable batch;
refresh all release packages together under the exact-source connection rule.
Frame pacing and gameplay/network behavior are unchanged.

Stateful reusable widgets, additional dynamic painters, non-stock flashing
gauges, exact composed-surface color-key behavior, initial console attachment
animation and further text wrapping comparisons remain interpreter work.
The system-font service supports the Verdana declarations used by the installed
assets; other families, nonzero stretch and negative character-height variants
remain unsupported. Complete MDL/original UI equivalence is still an active goal.

## Original text policies and authored combo faces

This Linux-only batch compares several shared text and widget behaviors with
`engine/image.cpp`, `engine/controls.cpp`, `effect/combopane.cpp`,
`engine/menu.cpp`, `WinTrek/trekmdl.cpp` and `WinTrek/teampane.cpp`.

| Observation | Correction |
|---|---|
| StringImage wrapping | Use the original measured, byte-based word/newline break policy for compiled image height, menu drawing and composed HUD text. Preserve separator consumption, leading-space behavior, the historical hard-token break and absence of an extra row after a trailing newline. Empty images have zero height and retain authored width. |
| StringPane clipping | Keep a single byte stream. Left/center panes retain a fitting prefix; right and LeftClipRight retain a fitting suffix. LeftClipRight places the suffix at the left edge. Zero-width clipped panes emit no positive-width glyphs. Preserve the sixth constructor argument in compiled HUD panes too. |
| Shared original policy | Factor StringImage's existing break algorithm into an allocation-free ClientCore helper also called by WinTrek. Its font fitting callback and separator policy are unchanged; the replacement avoids the old saved-string copy. A frozen extraction of the original implementation remains the independent comparison reference. |
| Fonts and edits | Retain authored height, weight and color on EditPane and combo descriptors. Dynamic captions use those descriptors rather than selecting a hardcoded font. Console text adapters retain image/pane policy and system-font metrics. |
| Combo layout | Retain face size separately from overall/minimum control size, as the original left-justified ComboPane does. The compact F6 team selector paints/clips to 109 by 12 pixels and accepts input over 125 by 12. Setup, team settings and Games List dialog captions use their authored face rectangles. |
| F6 colors | Paint StringColorComboPane's original faction-colored face; preserve the original seven-color palette in an inline helper used by WinTrek too. The No Team face/menu label has no pilot count. Team popup rows retain their faction colors and brighter selected background. |
| Popup typography | Use the team combo's smallFont and the wing combo's largeFont, including font-derived row spacing. Setup/civilization popup labels and accelerators retain the source combo's font metrics. Flight context menus use the original purple/red row colors, font-based width, label/key offsets and raised border. |
| F6 interaction | The host adapter used the default selection delta for the team/wing opening controls. That selected team zero/wing zero rather than opening a menu. Give opening controls explicit opening actions and open combos on left mouse-down, matching ComboPaneImpl. Preserve Escape dismissal and the expanded layout's authored wing control. |

The Linux build, UI-script, session, console and flight player-pane checks pass.
The text comparison runs **5,460 frozen-source cases** across three resident
fonts and widths from -1 through 180, including whitespace, newlines and hard
tokens. Independent checks cover prefix/suffix fitting, exact-fit boundaries,
zero-width panes, measured word-wrap height, separately authored combo
face/control sizes, edit fonts/colors and system-font cache identity.
Flight input checks cover opening on mouse-down, release retention, Escape,
the compact team font and expanded wing font/row height alongside existing
donation, Boot, visibility/history, scrolling and crew behavior.

The full Vulkan render check passes with validation enabled. It verifies
suffix glyph identity/position, zero-width clipping, all four measured wrapped
rows and actual compact-team face pixels, alongside existing six-style HUD,
artwork, hover, panel, cursor, training, setup and postgame checks. HUD CPU
construction p50/p99 values in microseconds were 5.32/6.12, 4.56/5.07,
4.71/5.39, 7.18/7.92, 4.57/4.69 and 5.61/6.37 for styles 0–5. These are
diagnostic construction times, not total frame times or a controlled comparison.
The implementation retains resident font atlases and one ordered HUD batch;
it adds no per-frame line-string allocation or texture creation.

The final 1,220-frame live console flow passes with real SDL input and Vulkan
validation, including chat, orders, research, details/scrolling, ballots and
mission commands. Reviewed captures show the compact colored team face and
native-sized Game details text. Logs are
`/tmp/allegiance-text-widgets-{script,session,console,player,render,live}.log`;
live captures are under `/tmp/allegiance-text-widgets-live/`.

The Linux client/server source identity at that checkpoint was
`fa20463568b0acccaee36e62afffab2de15f1f85467e0ee61c45f61a1a4b5c44`.
Windows/macOS packages were deliberately not rebuilt for this portable batch.
Refresh release packages together under the existing exact-source connection
rule. Frame pacing and gameplay/network simulation are unchanged.

This restores text policy and widget metadata, not a complete stateful widget
interpreter. Games List filter dropdowns still have a separate host painter and
need original popup/input behavior; general dynamic image-size reflow and
legacy inline color-code text need further comparison. Non-stock fonts/stretch,
composed-surface color-key semantics, flashing gauges and initial console
attachment animation remain on the execution queue.

## Shared authored combos and popup interaction

This Linux-only batch compares the original `effect/combopane.cpp`,
`engine/menu.cpp`, `engine/popup.cpp` and Games List filter host with the native
adapters. It restores related popup behaviors together rather than adding
another screen-specific dropdown painter.

| Observation | Correction |
|---|---|
| Filter menu presentation | Replace the separate black, fixed-nineteen-pixel dropdown with the shared original popup painter. Keep the original Yes/No/DontCare order and IDs, purple/red palette, raised border, label/key offsets, font-derived row height and point-sized upward anchor. Use the authored combo's font descriptor. |
| Popup keyboard priority | Route keys to the top menu before the Games List filter dialog. Up/Down wrap among the three choices; Enter selects the menu choice; Left/Escape dismiss only the menu. Dialog acceptance/cancellation remains separate. |
| Edit focus and capture | The filter dialog keeps its original name-edit focus underneath the menu. The top popup consumes text and blocks the dialog/list while open; closing or choosing restores editing without accepting the draft. An outside click dismisses the menu and is consumed. The filter dialog's original non-close-all policy keeps it open. |
| Shared combo state | Setup, faction selection and filters use one native authored-combo path with immutable host-provided choices. The source descriptor supplies the font and the shared popup supplies layout/input. Retain per-control menu highlight independently of the displayed committed selection, matching ComboPane/IMenu. Scope remembered state by namespace/control, reset on asset reattachment, and rebuild faction state when its contents are replaced. Remove separate setup-row and filter-dropdown state. |
| Initial highlight | Original IMenu selects the first added row. ComboPane::SetSelection updates the face and selected ID, not the menu highlight. Preserve first-row initialization and subsequent independent menu-highlight retention rather than initializing every opening from the committed choice. |
| Drag/release | Original MenuItemPane dispatches on left release. Frontend and F6 menus now accept release over a row after dragging from a combo face or another row, without requiring a matching press on the destination row. A stationary opening release over the blocked parent remains inert. F6 right release does not execute a left-button command. |
| Disabled menu rows | Original MenuItemPane receives hover even when its command is disabled. Native popup hit testing permits that highlight while activation remains gated. Disabled ordinary buttons and blocked underlying surfaces remain excluded. |

The Linux build, UI-script, session and player-pane checks pass. New session
cases exercise all eleven filter controls: integer-scale hits, authored font
spacing and anchor, choice order, first/independent retained highlights,
keyboard wrapping, top-menu capture, text suppression, focus restoration,
drag release, outside dismissal and cancellation of staged changes. Existing
setup authority/preset/cancellation, faction/name transactions, options,
loadout and other session behavior checks pass. Flight checks additionally
cover wing-menu drag release and rejection of right-button release.

The full Vulkan check passes with validation enabled. Independent pixel/layout
checks verify filter-row order, menu width/height, point anchor, authored font
and label offsets, selected/unselected palette, raised white edge and keyboard
highlight. Sixty-four repeated builds retain the same GPU texture count.
Existing six-style HUD, artwork, hover, console, setup, panel and postgame
checks also pass. Popup state changes rebuild the existing native controls;
unchanged frames continue to use cached descriptors, resident font atlases
and the ordered HUD batch.

The live Games List diagnostic now checks actual SDL popup keys and
drag/release, not just opening/cancelling the filter dialog. It passes through
live endpoint query/selection, dropdown input, staged cancellation, encrypted
join, original remote lobby, leave, named local creation and cancellation back
to the Games List. The final flow completed in 538 presented frames. Reviewed
captures show the native-sized original filter dialog/dropdown and faction
lobby artwork. Logs are `/tmp/allegiance-combo-{script,session,player,render,live,server}.log`;
captures are under `/tmp/allegiance-combo-live/`. The temporary test server was
shut down after validation.

The Linux client/server source identity at that combo checkpoint was
`c5e0c06f754065c2ed0a466f2da449d1d6289212d575adfa70d717e014bbc446`.
Windows/macOS packages were deliberately not rebuilt for this portable batch.
Frame pacing, transport and gameplay/network simulation are unchanged.

This establishes a shared native combo adapter, not a full legacy widget
engine. At that checkpoint, original EditPane input/rendering, further overflowing
and cascading menu cases, additional dynamic painters, color-code text, composed
surface color keys and initial console attachment remain audit work. F6's
repeated-open highlight retention still needs comparison with its original
pane re-creation/mission lifecycle.

## Original authored edit fields and focus

This portable batch audits `engine/controls.cpp::EditPaneImpl`,
`zlib/window.cpp::SetFocus`, the Games List filter/create dialogs, team-name
popup, lobby chat and postgame selection handlers together. Original EditPane
has append/backspace editing and an end cursor; it has no movable caret,
selection or Delete operation. Preserve that interaction instead of adding a
new editing model to the original interface.

| Observation | Correction |
|---|---|
| Cursor drawing | Replace the appended underscore with the original filled rectangle. Reserve the resident font's width of `o`; draw from text-width + 1 to text-width + cursor-width, and from pane-top + 1 to pane-bottom - 2. Empty fields retain their cursor. |
| Long focused text | Retain the suffix fitting the field minus its cursor width. Unfocused and readonly fields retain the fitting prefix. Use the original byte/glyph fitting rules and exact-fit boundary. |
| Text placement | Center text vertically using the original pane-height/font-height rule. Use the authored EditPane's font, color, bounds and clipping, including lobby/postgame chat; remove postgame's hardcoded field geometry/font. |
| Blink | Evaluate the original one-second period with its half-second visible phase. Ordinary text input restarts the phase before max-length/filter validation; Backspace and focus changes do not. Keep phase state per namespace/control and clear it on asset reattachment. |
| Modal focus | The shared edit adapter blurs previously painted edits when a modal supplies a focused field. Team settings show one name cursor; covered lobby chat returns to prefix clipping and has no cursor. Closing restores the retained chat draft/focus. Faction menus retain the name field's focus underneath while consuming their own input. |
| Postgame selection | Original player/team selection does not remove chat focus. Preserve it for row/header selection and player-list wheel navigation. Chat still accepts text afterward; End/Home do not navigate the player table while chat owns input. |
| Shared implementation | Filters, creation, readonly setup name/password, team names and lobby/postgame chat use one authored edit adapter and the existing ordered Vulkan HUD batch. Clipping returns a string view without allocation; blinking needs no control reconstruction or font/texture upload. The painter also honors password masking. |

The Linux UI-script check passes 17,472 comparisons with a frozen original
`EditPaneImpl::UpdateStrings`, covering three actual resident fonts, empty and
long text, widths -1 through 180, focus, readonly and password combinations.
Explicit blink checks cover the half-second boundaries and negative elapsed
time. Existing 5,460 frozen StringImage comparisons remain passing.

The session suite passes with new authored-field/focus assertions, raw text
without cursor characters, append/backspace behavior, inert Left/Delete,
unchanged blink phase on Backspace, readonly setup fields, faction-popup name
focus and single-cursor modal restoration. The postgame check passes chat
focus/typing after selection, actual scrollbar-thumb navigation, sorting and
all existing scoring/report/export/next-round checks. It now verifies bounded
presentation directly: both 1,024 and 2,048 pilots produce twelve visible
player rows, 226 authored controls and 43 hit regions.

The full Vulkan check passes with validation enabled. New checks verify
suffix/prefix/readonly/password glyphs, vertical centering, exact cursor bounds
and white GPU pixels, empty-field drawing and blink-off. Sixty-four retained
builds add no textures. Existing HUD, artwork, popup, setup, console and
postgame pixel comparisons also pass.

Actual SDL diagnostics pass through long filter-name editing and original
popup interaction, live encrypted join/leave, creation and cancellation (632
frames); team-name/faction/cancel/focus and briefing/countdown (400 frames);
and victory, row/header selection with continued chat typing, scrollbar,
Save Stats and Back (250 frames). The final team-dialog capture confirms the
covered chat cursor is absent. Reviewed captures retain the native-sized
original artwork and fields. Logs are
`/tmp/allegiance-edit-{script,session,endgame,render,games,team,results,server}.log`;
captures are under `/tmp/allegiance-edit-live/`. The owned temporary server
was shut down after its join/leave validation.

The Linux client/server source identity at the EditPane checkpoint was
`259d5661e04b7d6112e2ec94e863e9538d09b7a060b504525b3d29f3eb583865`.
Linux checks are sufficient for this self-contained portable change;
Windows/macOS packages were not rebuilt. No WinTrek source, frame pacing,
transport or gameplay/network simulation changed in this batch.

This restores shared EditPane presentation and focus, not a complete legacy
widget interpreter. SDL text-event batching at max length, native UTF-8 input
and filtering still need comparison with legacy per-character/ANSI handling.
Further dynamic painters, overflowing/cascading popup cases, inline color-code
text, composed-surface color keys and console attachment remain in the queue.

## Original inline colors, batched input and creation validation

The native text adapters now interpret the original font color controls:
`0x81`, a separator and eight hexadecimal `rrggbbaa` bytes select a color;
`0x82` and a separator restore the supplied color. Compiled HUD text, original
menu fields, bitmap labels and flight chat share the allocation-free parser.
Invalid/truncated controls consume only available bytes and retain the current
color. Encoded RGB follows the existing authored sRGB-to-linear conversion;
alpha remains independent. Controls emit no glyphs or displayed advance, and
each separately drawn row starts with its supplied color.

Original `GetMaxTextLength` deliberately fits raw bytes, including control
bytes. StringPane clipping, StringImage breaks, EditPane suffix fitting and
console overflow retain that policy. Displayed extent ignores color controls.
This follows the intended original font behavior consistently: the legacy
`GetTextExtent` overloads compare signed `char` with integer markers, unlike
its `BYTE` drawing loop, so their recognition can depend on compiler character
signedness. The native extent tests are explicit golden checks, not a claim
of frozen equivalence with that compiler-dependent behavior.

Original gameplay producers are restored as well. Enemy ripcord-probe sightings
color the faction/model name, station-destruction messages color both the
killer and station owner/name, and non-recovery commander orders use the
original orange tip. They reuse the existing `ConvertColorToString`, preserve
the original reset separators, and retain delivery, visibility and sound gates.
Capture notifications and further less common producers remain to be audited.

SDL text events are processed as individual characters. Limited edits accept a
fitting prefix instead of rejecting a whole event; numeric fields discard
non-digits individually. A frozen extraction of `EditPaneImpl::OnChar` passes
9,900 ASCII cases across capacities, batch sizes, normal/numeric and readonly
states. Team names retain the original 24-byte limit. Native UTF-8 capacity and
Backspace checks ensure valid multibyte characters are not split; printable
filtering and general Unicode rendering remain native behavior requiring
further comparison with original ANSI input and byte-indexed fonts.

Create Game and Games filtering retain their original unbounded editable
fields. Create validates on acceptance, using the original messages
`Error : Game name is empty` and `Error : Game name is too long`.
The original `c_cbGameName` is 65 including the terminator: a 64-byte name is
valid. Dismissing an error retains the draft and restores field focus. Session
and browser records now preserve all 64 bytes. Session consumes one previously
reserved byte and remains 452 bytes; Configuration remains 472. ServerInfo is
604 bytes, four more than before, with explicit initialized tail padding.
This affects browser queries, not periodic simulation reports. Existing strict
source/ABI admission gates remain unchanged.

Linux checks pass for UI scripts, sessions, console/chat, player panes,
callbacks, postgame and encrypted browser/commander-order delivery. Tests also
assert actual probe, station-destruction and received commander color strings.
The callback run exposed an uninitialized `Vector` velocity in its synthetic
chaff fixture; initializing it to zero restores the real IGC 2.75-radian/second
sprite-rotation check without changing gameplay.

The full Vulkan validation passes, including actual red/green glyph pixels,
reset/alignment, encoded sRGB/alpha, invalid/truncated controls, masked edits,
cursor color, compiled HUD text and 64 repeated builds without added textures.
Six authored HUD styles retain resident textures, with measured CPU build
p99 between 5.4 and 8.8 microseconds in this diagnostic run. These are isolated
UI-build measurements, not whole-frame latency claims.

Actual SDL Games validation passes in 849 frames: live encrypted query and
join/leave, original filter popup interaction, empty/overlong creation errors,
restored focus, a complete 64-byte name and local setup cancellation. The
1,220-frame console diagnostic passes hangar chat, command recipient/verb
artwork, completion, quick chat, command acceptance, Game Details, ballots and
mission commands. Creation error/setup captures were reviewed at their native
800x600 canvas; diagnostic captures are under `/tmp/allegiance-inline-live/`.
Logs are `/tmp/allegiance-inline-*.log`. The owned server on port 28759 was
shut down after validation.

The coherent Linux source identity at the inline-color checkpoint was
`fc0c26be1f4d49988f52cefa09a5d5223888c5a9060ccce9f9794b2457e35d7e`.
Portable changes use Linux validation; Windows/macOS builds are reserved for
platform work or release checkpoints. No WinTrek implementation, frame pacing,
transport or simulation policy changed in this batch.

The following clipping/chat and dynamic-host batches address the wrapping and
parent-clipping gaps observed at this checkpoint. A complete legacy widget
interpreter and full UI equivalence remain unfinished.

## Inherited MDL clipping and original chat breaks

Native pane compilation now retains each fixed parent's actual extent and
intersects its clip with descendants. ListPane minimum sizes and transparent
hover wrappers permit the original painter to widen a runtime list. ImagePane and ScreenPane use the imported image
size; BlackPane inherits its child's size; PaneImage uses the root pane size,
rather than the largest flattened child. This follows original
`ImagePane::UpdateLayout`, `Pane::PaintAll` and `PaneImage` surface creation.
Children extending beyond a background no longer enlarge or escape that host.
The oversized Create Game edit keeps its authored 450-pixel width and original
suffix policy, while drawing and pointer hits respect the parent boundary.

A shared OriginalControl translation moves both bounds and clips. Games,
filters, credits, mappings, LAN, teleport, briefing, team settings, investments
and player panes use it. Widget substitution intersects its own rectangle with
the inherited clip instead of replacing that clip. Authored hit adapters use
the intersection of control bounds and clip. Layout/painter coordinates remain
unchanged, so clipped controls do not reflow or stretch.

Composed-image bounds also respect clipping. JustifyImage, BlendColorImage and
composed/animated image hosts use the visible union, preventing clipped-away
children from shifting an image's anchor or tint rectangle. The existing
AnimatedImagePaneRect clip remains authoritative; its original child text
geometry is retained instead of rewriting dimensions to approximate clipping.
These operations run during layout compilation/rebuild, with no legacy surfaces,
additional renderer passes or per-frame textures.

Classic flight, lobby and postgame chat now use ChatListItem's original wrapping
policy. It fits raw bytes, preserves spaces at the word break, prepends two
spaces to continuation rows, and retains the historical blank-row/three-byte
skip for extremely narrow lines. A shared allocation-free ClientCore helper
represents the virtual prefixed string with spans. It avoids repeated copies of
the remaining message and preserves the native cached/incremental histories.
The optional modern UI retains its existing Unicode-aware wrapper. Native
character input/font decoding still require further ANSI/Unicode comparison.
WinTrek's implementation and performance are unchanged.

A frozen copy of the original ChatListItem constructor passes 22,807 comparisons
across four actual resident fonts, widths 1 through 450, empty text, spaces,
long tokens, embedded newline bytes and inline colors. Another 593 cases hit
the original short-string assertion because its forced three-byte consumption
exceeds the remaining string. Those undefined cases are excluded from exact
comparison; independent negative/tiny-width checks prove bounded native
progress. Existing frozen StringImage, EditPane clipping and input checks pass.

Independent MDL fixtures verify nested partial/empty clips, root pane/image
extents, BlackPane, translated widget substitution, and clipped JustifyImage,
BlendColorImage and composed/animated host bounds. The session check verifies
that clipped-away edit pixels cannot accept pointer input. Caption diagnostics
now distinguish a clipped hit rectangle from its unchanged authored draw bounds.

Linux UI-script, session, console, player-pane and postgame checks pass. The
full Vulkan suite passes with validation, including a translated authored
Create Game host, visible field glyphs and black pixels beyond the parent edge.
The capture `/tmp/allegiance-native-parent-clipping.png` was reviewed at the
native 800x600 canvas. Sixty-four repeated menu builds add no textures; six HUD
styles retain their textures, with isolated CPU-build p99 of 4.9–8.6 microseconds
in this run. Other original panel, font, setup, popup and postgame GPU checks
remain passing. Logs are `/tmp/allegiance-clips-*.log`.

## Batched dynamic-host asset audit

This pass surveys the active native UI hosts together, distinguishing values
imported from MDL from constants in the original WinTrek/effect painters.
It does not change WinTrek, simulation, frame pacing or transport policy.

| Host | Asset-driven values now consumed |
| --- | --- |
| Lobby/postgame chat | ChatListPane content width, height, font index and internal-scrollbar flag; inherited clip and actual hover extent; cache invalidation for font/width changes. Scrollbar width comes from the original arrow artwork. |
| Lobby pilot roster | Authored list position/minimum size, scrollbar reference/geometry and six player columns. Host/leader marks, name, status and wing follow the columns. |
| Lobby faction seats | Authored team-list position, height and columns. TeamPainter's six-faction-plus-No-Team division sets row height. Original per-side tab artwork, small-font labels, faction line, available/destroyed state and alliance edges replace the approximate tinted tabs. The arrow marks a requested team. |
| Postgame tables | Both authored list positions/sizes and column arrays, linked player scrollbar, clipped selection hits and cell widths; only visible rows are built. |
| Input mappings | Authored list/bar positions and extents, mappingOffset and text/selection/no-mapping colors. The runtime instruction is bound before StringPane measurement rather than replacing its geometry afterward. |
| Create Game | TextButtonBarPane is compiled with authored labels/columns and original small/hover-bold faces. Server/core rows and scrollbars use their authored hosts and column arrays, including alternate scrollbar names. Local creation remains the actual supported backend. |
| LAN | The authored server-list host and scrollbar retain original painter row/padding/font values. The LAN restoration batch below replaces the temporary endpoint editor with discovered, selectable servers. |
| Credits | Authored message-pane extent/position and numeric animation expression. The existing HUD arithmetic compiler now supports retained scalar programs and static MDLFileImage dimension queries. The scroll path evaluates indexed arithmetic without AST traversal/allocation. |
| Briefing emblem | Original host image dimensions are supplied to the MDL wrapper. Position, rectangle and inherited clip come from the compiled teamImage slot instead of copied coordinates. |
| Crew captions | TurretOfImage and BoardTextImage use resident HudPrograms; their labels, fonts, colors, blend modes, Glass branches and positions come from MDL and the overlay table. InTurretPaneImage preserves its authored black-keyed PaneImage. |
| Game State | Both GameStateContainer font arguments are retained, including system-font height/weight. Title/grid measurements use the title face, clock uses its separate face and footer clipping. |
| Cockpit chat | Authored CommPane composition, ClipImage, list width, background and overlay anchor replace copied crop/placement math. Layout rebuilds only when line/font settings change; cached controls and resident textures are reused. |
| Existing authored hosts | Teleport, F6 roster and Game Details now follow each list/page's scrollbar reference too. Their geometry/columns were already imported. Research cards, loadout items, map preview and popup borders remain original C++ painter policy, rather than invented MDL parameters. |

Chat's original font-size fallback (small for unknown indices) is shared in
ClientCore. ChatListPane's physical extent includes its visible scrollbar,
while its content viewport remains the declared width. Hidden scrollbars do not
create pointer regions. ListPane sizes are minima; typed list descriptors do not
prematurely clip a painter to the declared minimum width. Fixed containing
images still clip their children.

The input-mapping MDL declares a font but WinTrek does not import it: its actual
call site passes TrekResources::SmallFont. Native now follows that call site;
the formerly hardcoded medVerdana was incorrect. Original 14-pixel lobby/result
rows, 28/15-pixel flight roster rows, painter padding, artwork-name policies and
ListPaneOld's eight-pixel footer are retained where the original C++ defines
them. Current native faction rows show their actual pilot count; the original
maximum/rank annotations require authoritative fields that are not exposed by
the current lobby report. Persistent Club ranks remain unavailable.

Original ImagePane::UpdateLayout fixes the containing image's size even when
ChatListPane::UpdateSize creates a taller child. Native cockpit chat now respects
that parent. The old independent max(backgroundHeight,listHeight) approximation
and its copied 355/125/126 geometry were removed from ClientCore. ClientCore
retains only the original row-count/font-height/padding calculation. MDL handles
composition, clipping and anchoring. Diagnostic expectations were corrected
against these original methods, rather than preserving the old approximation.

Overlay adapters compile only their requested roots. They do not evaluate the
whole dialog table merely to position one panel. Crew HUD programs, scalar
animations and chat layouts remain resident. The renderer uses its existing
ordered bindless UI batch; there is no legacy surface tree or per-frame texture
creation.

`test_reference/authored_hosts.hpp` builds a temporary alternate asset directory
using file symlinks, unlinks each override before writing, and leaves installed
Steam data untouched. It changes mapping colors/split/geometry, renamed Create
scrollbars and header columns, lobby/result extents/columns/fonts/scrollbar
visibility, briefing clips, crew anchors, credits size/speed and Game State font
arguments. These checks exercise actual native frontends, not just parsing the
changed declarations. Existing frozen text policies and headless lifecycle/UI
checks are retained. The Vulkan suite checks actual pixels, clipping, blend
modes and resident texture counts.

Validation on Linux:

- Release client/server/session builds passed with matching source identity
  `760055acc2bcaa496e7c955e767371fe974302e721db5023930382e7e22a1b61`.
- Session, UI-script, crew, Game State, player-pane, endgame and console checks
  passed, including the alternate-MDL fixture. Postgame table work remains
  bounded to twelve visible rows at both 1,024 and 2,048 pilots.
- The full Vulkan pixel suite passed. Six authored HUD styles measured CPU build
  p99 of 5.33–9.28 microseconds; resident texture counts remained unchanged.
  The suite also verifies the original black-keyed turret loadout pane and fixed
  containing-image clips for cockpit chat.
- Wayland SDL/Vulkan validation runs passed: live Games queries/encrypted join,
  filters, remote lobby/leave and named local creation (848 frames); team dialog,
  faction selection and mission briefing/countdown (400); actual victory,
  postgame sorts/scrollbar/XML export/Back (250); lobby chat paging, mapped chords,
  channel clicks, draft retention and ten-second autoscroll (1,931).
- Captures of the Games/Create dialog, remote lobby, briefing and postgame table
  were inspected at the original 800×600 canvas. No Vulkan validation errors
  appeared. The owned test server was closed after validation.

Logs are `/tmp/allegiance-hosts-*.log`; live logs and captures are in
`/tmp/allegiance-hosts-live/`. This portable batch was validated on Linux only,
without repeating Windows/macOS builds. Remaining functional/session-service
gaps are listed below; the asset-host batch does not supply persistent ranks,
authenticated lobby capacity/rank annotations or legacy Club services.

## Lobby roster ordering and state batch

Comparing the active native team screen with `TeamScreen::PlayerPainter`, its
header callbacks and the original `SortedList` exposed several related gaps.
The six authored player headers now dispatch their original sorting behavior:
name, rank, status and wing select a mode; the host/leader columns retain the
current mode and reverse it. Repeated clicks reverse direction. The checked and
checked2 artwork follows the clicked header. Default entry restores the original
leader ordering without an initially checked header.

`ClientCore/LobbyRosterPresentation.h` preserves the actual comparisons, rather
than their occasionally misleading comments. Name/rank ties are stable in both
directions; reverse status/wing sorts reverse equal entries too. The leader
comparison is not a strict weak order. An indexed linear insertion routine
preserves its results without passing it to `std::sort` or retaining the
original quadratic linked-list insertion. Scratch vectors retain capacity.
Other modes use strict stable key sorts. Members precede applicants in the
source sequence, independently of the interleaving of raw membership reports.
Selection, popup actions and private chat recipients continue to use pilot IDs.

The painter now displays the original numeric zero rank for unranked native
pilots; non-Club `LookupRankName` returns an empty name when no rank table exists.
This does not invent a persistent rank service. Applicants no longer display
host/leader checkmarks. Negative wing IDs and No Team membership leave the wing
cell blank. Applicant and unread-chat flashing now follows the original
quarter-second timer rather than the former 0.75-second approximation.

First entry selects Team chat for a faction member and All chat for No Team.
The remembered tab survives postgame/other-screen navigation; a remembered
Leaders tab that is unavailable falls back to the original membership default.
Leaving a match clears that memory without the closing team screen saving it
again. Original membership changes still reset to All. Lobby quick chat follows
the selected/default channel. Inactive faction seats are blank before play;
selecting an empty destroyed faction during play falls back to No Team.

Validation:

- 40,960 independent frozen-comparison cases cover all modes/directions,
  duplicate/case-varied names, rank/wing ties, membership/readiness combinations,
  arbitrary host/leader flags and empty lists. Real frontend checks cover all six
  headers, direction faces, selected private recipients/drafts/focus, applicant
  role suppression, rank text, wing cells, chat entry/return and inactive seats.
- Session, UI-script, player-pane and endgame checks passed. The full Vulkan
  pixel suite passed; HUD build p99 was 5.11–9.48 microseconds with unchanged
  resident texture counts. The 2,048-row leader-ordering microbenchmark averaged
  5.98 microseconds; this measures ordering, not a whole frontend rebuild.
- Wayland SDL checks passed all six sort-header clicks, descending name order,
  zero-rank display and retained private selection/draft, along with chat
  paging/chords/filters and ten-second autoscroll (1,675 frames). A real encrypted
  Games query/join/lobby/leave/local-creation run passed (848 frames). Native-scale
  captures were inspected. No Vulkan validation errors appeared; the owned test
  server was closed.
- Release client/server builds match source identity
  `83e69538de589c0a4b42dba3742a019d2e081e2c216c4ed89a1fb60e282d7408`.
  Logs are `/tmp/allegiance-roster-*.log` and captures/live logs are in
  `/tmp/allegiance-roster-live/`. Portable code was validated on Linux only.

## Wing assignment family (2026-10-06)

This batch covers the complete related path exposed by the lobby comparison:
configuration migration/persistence, initial faction entry, lobby and flight
selectors, pregame authorization, commander succession, faction transitions,
replica metadata and public/F6 presentation.

`ClientCore/SavedWing.h` preserves TeamScreen's absent-preference default of
Command (0), explicit NA fallback to Attack (1), and one-time preference
restoration on faction entry. Ordinary reports and other commanders' later
assignments do not replay the local preference. The native client saves own
selections from either interface; joining another faction, handing over
leadership and starting automatic donations restore the saved wing through the
same action queue. A new match resets entry tracking; authenticated resume of
an existing match retains authoritative state.

The actual MDL `wingComboPane` now supplies its original image, bounds, font
and ten original wing names. Original image faces retain their baked label;
text/color faces render the selected wing using the authored face size. It is
available only while viewing one's own faction; leaving the faction closes its open menu. Pointer press/release,
drag selection, Escape and keyboard input use the existing classic combo host.
Authored F6 selectors use the authoritative own selection and open with that
row selected, without adding a selector where the MDL omits it. Native local
setup retains its editable preference.

Authority now assigns Command on initial command or succession, accepts wing
changes in the pregame lobby, and preserves ordinary members' wing across team
changes. It rejects unauthorized, out-of-range and No Team assignments. This
matches FedSrv's current code, including its deliberately commented-out reset
of an outgoing leader's wing; explicit client restoration is separate.

No Team refresh explicitly clears the displayed wing and updates the
side identity, rather than retaining the previous faction in client state.
The No Team report carries canonical empty physical state: newly created pilots
can have an adapter Thing before their hull/pose is initialized, and a departed
pilot's previous motion/radius/damage/signature has no lobby meaning. This also
fixes non-finite-state failures during fresh pending-applicant bootstrap.

Existing faction `Pilot` records carry the wing independently of a physical
ship. A retained ID-indexed replica table fills once in the existing private
pilot pass, clears only previously touched slots, and feeds public-roster/F6
presentation. There is no per-member search across all pilots. Physical ships
also receive this metadata when present. Allied/private/public boundaries are
preserved: public `Member` records contain no wing, and allied pilot/entity
records mask the assignment. The faction report encoding and delta arenas are
unchanged; no additional wire fields or per-recipient encoding are introduced.
WinTrek/FedSrv source is unchanged by this batch.

Validation:

- Session checks cover original preference defaults/import precedence, NA and
  invalid values, save/reload, one-time entry and later commander assignments.
  Real frontend checks cover all ten wings, original bitmap-label semantics,
  an alternate text face/font/position, drag/release, Escape, No Team and
  opponent roster visibility. UI-script and F6 suites passed.
- Encrypted loopback wing, admission and lobby suites passed. Coverage includes
  ownership/range rejection, a missing physical teammate, private metadata
  retirement, allied/public masking, commander succession, team changes,
  authenticated resume and fresh pending-applicant acceptance. No Team reports
  are checked both without a hit test and with deliberately poisoned allocated
  motion, ensuring physical getters are unnecessary and state remains finite.
- The full Vulkan pixel suite passed. HUD build p99 ranged from 5.76 to 9.35
  microseconds across the six styles, with resident texture counts unchanged.
- Actual Wayland SDL game-browser/query/join/lobby/leave/local-creation testing
  passed 1,003 frames. Wing tests exercise press/drag/release, Escape, saved
  preference reload and encrypted pregame confirmation. Captured 800×600
  artwork shows the original popup/button and the confirmed `charlie` row.
- Client/server were built coherently on Linux; final source identity is
  `5de6b3180cda0d0e65e2c4f56d3b42de7b127e0d77bc8c8f790ab84b166d9a91`.
  These portable changes did not trigger Windows/macOS builds. Owned test
  windows and servers were closed.

The broader execution queue below remains open.

## Grouped asset-host and transient-screen completion (2026-10-06)

This batch collects the remaining identified copied MDL presentation values
and the related parent/capture/composition defects across the active original
frontend. It compares intro, training, Games and its dialogs, game/team setup,
hangar/loadout, investment, teleport, mappings, Help/Credits, lobby/results and
the common message box together. The earlier dynamic-host batch already imports
list geometry/columns/fonts, mapping styles, credits animation, briefing slots,
crew/Game State descriptions and cockpit-chat composition. Those paths remain
asset-driven; this batch closes the hover/preview and transient-parent cases
found in the remaining adapters.

| Family | Changes made together |
| --- | --- |
| Hover feedback | Compile all hover-driven SwitchString and SwitchPane branches into resident controls. Resolve the actual HoverSiteGetID or host-provided CurrentHover binding, including independent sites and arbitrary display names. Draw the authored caption, font, color, justification, geometry and inherited clip instead of substituting the action's C++ help label. |
| Intro/hangar hover events | Publish the original host's hover IDs/enumeration values. MDL owns the displayed branches. Disabled visible HoverPane wrappers still report hover; removed targets and modal-covered parents do not. |
| Loadout model preview | Read geoPosition and geoSize from loadoutpane. Derive viewport and aspect from that rectangle, retain the original C++ camera/rotation policy, and intersect the renderer's clear/scissor with inherited clipping without changing the projection. |
| Per-hull loadout art | Position optional loadout&lt;hull&gt;overlay images from the same authored preview origin and clip. Insert them over the ship and under the remaining controls, matching the original preview group. |
| Transient parents | Settings, Pause, Controls, Help, Credits and Game Details preserve the actual parent, roster identity/order, recipient, unfinished draft, focus and staged fitting. Returning restores retained state directly instead of reopening/resetting the screen. |
| Quit Mission | Use the common original message box above the actual lobby, docked screen or live flight scene. Close the global menu before opening Quit or Help. Preserve exact ordinary/training captions, OK/Cancel and Enter/Escape behavior. Application Quit retains WinTrek's immediate close behavior. |
| All common modal callers | Block parent pointer/hover, keys/chords, text, wheel and context input; retire old pointer capture; retain/restore parent focus across cancellation and pending completion. Unrelated screen transitions clear stale dialogs. Nested errors over separately composed menus/details keep each parent layer once, and flight-resume dialogs clear cached global-menu layers. |
| Active dialog hover | One blockInput operation marks the existing parent actions and hover sources before each popup adds its controls. Apply it to Games filters/creation, team settings, briefing, global/quick-chat/combo menus, mappings, Credits, Game Details and common messages. A blocked parent does not suppress the active child dialog's independent hover site. Nested messages block both layers until dismissal. |
| Message text | Match efpopup's LargeFont StringImage measurement, wrapping, shadow and containing-image clip instead of an invented fixed text height. Width, offsets and buttons are original C++ painter constants. |
| Related state gates | Team-dialog state cannot paint or capture above unrelated transient screens. Map-preview cursors are scoped to the actual original settings screen and suppressed by a modal. |

The audit distinguishes MDL values from original C++ behavior. Research cards,
equipment quantities/gunners, popup borders/rows, original list-painter padding,
message-box offsets and the model camera are not invented MDL parameters.
Their source policies are retained. This batch changes neither WinTrek/FedSrv
nor simulation, transport or frame-pacing policy.

Parsing and branch construction happen on load/rebuild. Per-frame hover only
selects retained controls; it does not parse expressions, recreate text assets
or upload textures. The existing ordered bindless HUD batch renders them.
The preview uses the existing model pass with a clipped clear/scissor.

`test_reference/classic_ui_batch.hpp` checks all six transient-parent paths,
nested composition, lobby/flight/training Quit, pointer release outside the
modal, input suppression, focus restoration, active Games dialog hover above a
blocked parent, nested hover capture, immediate application exit and
long message measurement. Its alternate-asset fixture renames both intro/hangar
feedback displays, changes the caption/font/color/size/justification, moves and
resizes the preview, and enables a per-hull overlay. Installed Steam files are
untouched. GPU tests use two independent compiled hover sites, a SwitchPane with
a different font/color, a separately blocked parent under an active dialog,
and a moved preview with both full and partial clipping.

Validation on Linux:

- Release client/server and relevant check binaries build coherently with source
  identity `339c058fe538fd112758be3618e6ae8444b9817a75312b0c2b1d4dacd57cb2fb`.
- Session, UI-script, player-pane, crew and Game State checks pass, including the
  grouped alternate-MDL and transient/modal cases. Encrypted loopback admission
  and lobby suites pass.
- The full Vulkan pixel suite passes, including actual authored hover colors,
  independent sites, blocked/disabled/hidden targets, active child dialog hover,
  preview ordering and parent clipping. Six HUD styles measure CPU build p99 of
  5.42–9.07 microseconds; resident texture counts remain unchanged.
- Actual Wayland SDL Games/query/filter/join testing passes 1,066 frames, including
  encrypted wing changes, modal Quit cancellation/reopening/acceptance, return to
  Games, creation errors with retained field focus, and named local creation.
  The pilot/command-view console run passes 1,220 frames: global-menu capture,
  chat/quick-chat, orders, Game Details scrolling/capture/return, ballots and
  mission policy. Captures show the original 800×600 canvas at 1x, including the
  Quit modal over the actual lobby and nested Create errors. Vulkan validation
  reports no errors. Owned test windows and the temporary server are closed.

Logs are `/tmp/allegiance-ui-batch-*.log`; live logs/captures are in
`/tmp/allegiance-ui-batch-live/`. Portable changes are validated on Linux without
repeating Windows/macOS builds. The remaining service/backend and visual/state
coverage work below is still open; this is not a claim of full UI equivalence.

## Grouped docked controls, crew launch and button-state audit (2026-10-06)

The follow-on comparison treats the four docked console hosts and their
training/crew transitions as complete workflows. The references are
`WinTrek/hangarscreen.cpp`, `WinTrek/load.cpp`, `WinTrek/trainingscreen.cpp`,
`WinTrek.cpp`'s console/camera handlers, `clintlib/appmsg.cpp`'s disembark
completion and `engine/button.cpp`'s original input/face policy.

| Family | Changes made together |
| --- | --- |
| Docked console bindings | Hangar, loadout, investment and teleport forward original inventory, command and camera bindings through one host policy. Dialogs, global menus, text entry and mapping capture retain priority. Ordinary flight/fire actions do not enter this docked route. |
| Inventory / combat camera | The mapped inventory command opens the docked loadout and closes it back to hangar. From docked command view it returns to the loadout; a mapped cockpit/camera command returns to hangar. Ghosts retain command view instead of opening equipment. Each route records the original logical key once, preserving authored training restrictions. |
| Development visibility | Read the mission's actual `bAllowDevelopments` into presentation state. Hide the hangar's original investment control and gate its mapped command by that flag. Live policy changes rebuild the artwork. Economy/scenario choice alone no longer substitutes for this mission policy. |
| Hangar Launch | Buy the owned fitting through the existing purchase/completion path before launch. A docked gunner or observer can launch their own ship from their own custom-zero fitting or station defaults, including available hull/part successors. Validate first; use the existing atomic purchase operation to disembark, fit and launch, retaining identity and leaving the parent ship untouched. A direct launch intent for a still-parented pilot is rejected rather than falsely acknowledged. |
| Owned presets | Loadout digit/Ctrl-digit shortcuts operate only on an owned ship, matching the original loadout key handler. Browsing a passenger's loadout does not save or apply the parent's fitting through these shortcuts. |
| Static buttons | Home, training, hangar and loadout no longer invent generic Tab/arrow navigation or Return/Space activation absent from original ButtonPane. Original mapped commands, popup/list keys, text entry, chat and mapping handlers remain separate. |
| Button artwork | Training Launch retains its original checked state after lesson selection. The renderer uses the original checked and second-checked fallbacks for complete and sparse bitmap strips, and omits a face when the original painter would omit it. This central correction applies across all original hosts. |

No original simulation/WinTrek/FedSrv code or wire record is changed by this
batch. Crew launch uses the existing authenticated PurchaseLoadout operation
and causal purchase receipt, with no extra protocol fields or disembark/refit
state machine. The docked routing check performs no allocation; button face
selection stays in the existing ordered HUD batch with resident assets.

The grouped session regression covers all four hosts, modal/global-menu capture,
mission policy changes, static-button keys, owned presets and training checked
state. Crew checks cover both observer and gunner purchase/launch, parent
preservation, station upgrades and redocking. The encrypted crew case also
checks custom-zero equipment, credit conservation, causal launch completion,
absence of a duplicate disembark replacement and subsequent automatic dock
restoration. Vulkan checks cover 80 full/sparse button states and actual
training Launch pixels before and after lesson selection.

Validation on Linux:

- Release client/server and check binaries build coherently with source identity
  `3bd99135dffa0f057d46340cee446eb76187bc37939410c776b92cf95c4111c0`.
- Session and observer/gunner crew checks pass. The full authored training suite
  passes all twelve mission runs and 62 introduction/debrief slides, including
  original input restrictions and pause/resume.
- Encrypted loopback purchase, crew and console suites pass. Crew coverage
  includes own custom-zero launch, parent/identity/credits preservation,
  redocking, firing/prediction, promotion, death/pods and recovery. Purchase
  coverage includes exact quantities, partial stock, reconnect, atomic launch,
  automatic dock restoration, recovery and teleport replacement.
- The full Vulkan pixel suite passes, including the 80 button-state cases and
  checked training Launch pixels. The six HUD styles measure CPU build p99 of
  5.12–10.53 microseconds, with resident texture counts unchanged.
- Actual Wayland SDL crew/docked control testing passes 1,113 frames: inventory
  open/close, docked command/cockpit transitions, mission development gating,
  staged passenger browsing/purchase, back/launch and remapped View Base.
  The remote refit run passes 3,312 frames: actual mouse staging, causal purchase
  completion, flight, natural autopilot docking, default refit and relaunch;
  no missing network baselines. Captures retain the original 800×600 artwork
  at 1x. Vulkan validation reports no errors. Owned clients and the temporary
  port 28765 authority are closed.

Logs are `/tmp/allegiance-docked-*.log`; live logs/captures are in
`/tmp/allegiance-docked-live/`. These portable changes were validated on Linux
without repeating Windows/macOS builds. The broader queue remains open.

## Functional widget input completion

This final grouped pass compares native input dispatch with
`engine/button.cpp`, `engine/frameimage.cpp`, `effect/listpane.cpp`,
`engine/menu.cpp`, and the original training, games and team handlers.

| Surface | Functional correction |
|---|---|
| Authored animated buttons | `FrameImageButtonPane` metadata now preserves its down-event action. Hangar Launch fires on press, takes no capture, and cannot launch again on release. This follows the MDL constructor across asset sets rather than a screen-specific exception. |
| Ordinary buttons and training | A double-click down emits the separate original event without capture or a second normal release action. Training's handler launches the already selected lesson on down; other buttons do not gain an invented double-click action. |
| Original lists | Mapping, games, roster, team, results, teleport and investment rows select on down without capture. Mapping edit, game join and team join execute their double-click handlers on down. A roster double-click does not repeat its single-click deselection handler. Existing admission/availability checks still apply. |
| Mouse capture | Both SDL frontend dispatch routes request capture from actual frontend state. A down action that leaves the hangar cannot retain capture merely because its release is routed to flight. Popup capture remains available for drag selection. |
| Inline chat popups | Wing and quick-chat menus execute the released row without requiring a matching press. Dragging from another row or the opening combo works. Clicking a menu border keeps it open; clicking outside dismisses it. |
| Ship visibility after crew changes | Boarding hides the passenger's empty model in IGC. Loading a real replacement hull now restores the native model's visibility flag, matching WinTrek's visibility refresh. Disembark/refit launches no longer omit the ship from the external camera, contacts or effects. Fog and cockpit/death-camera filtering remain separate. |

Regression coverage exercises animated launch timing, ordinary button
double-click suppression, training selection/launch timing, games-list joining,
mapping edits, roster toggle separation, team joining, popup drag/release and
border/outside dismissal. Observer and gunner launch checks verify both restored
visibility and actual scene submission, rather than only the camera's
`showPlayer` request. The command-wheel regression waits for the restored
original entrance blend before measuring a detent. The changes add no network
fields or per-frame work and do not change WinTrek or IGC source.

Linux release client/server, session, console and crew checks build coherently
with source identity
`145508d6e2517acb0cdc1228a9ef86fb5087ad91707812249d9a4f767bacb610`.
The session, console, crew and camera suites pass. Camera coverage includes
launch/death framing, command wheel zoom, boost/release, collision shake,
view transitions, head look and engine loops. The encrypted crew scenario passes
boarding, custom-zero disembark/purchase/launch, firing/prediction, promotion,
death/pods and recovery. Actual Wayland crew input passes 1,100 frames,
including mouse capture release, replacement-ship scene submission,
purchase/refit/relaunch and mapped camera transitions. The captured launch
frame was visually checked and shows the launching ship. The actual SDL
console run passes 1,220 frames including original chat/orders/quick-chat,
Game Details, ballots and mapped money/order prompts. Vulkan validation has
no errors in either run; the crew run records no presentation misses at 165 Hz.

Logs are `/tmp/allegiance-widget-*.log`. Portable changes were checked on Linux
without repeating Windows/macOS builds. Shared button/combo sound cues and
animated hover loop/finish details remain cosmetic comparison work; the
broader functional service/platform queue below remains open.

## Functional LAN entry restoration

The original `introscreen.cpp` ServerDialog and ServerItemPainter flow now
uses actual discovery instead of the temporary address editor. The main-menu
LAN action opens the original MDL dialog, shows `Searching...` while empty,
and displays real game names and occupancy in the authored list. Rows use the
original small font, fourteen-pixel height, red selection and single-line
clipping. Join remains disabled until an admissible server is selected.
Selection happens on mouse-down without capture; double-click joins on down
without a duplicate release action. Blank list space clears selection. Keyboard
and wheel navigation reveal offscreen rows through the authored scrollbar.
Stable endpoint selection survives refresh and sorting, and is cleared when
its server disappears. The public games screen retains its separate filters.

Discovery is an application-level IPv4 UDP endpoint probe on compiled port
27501, separate from the normal GNS game port (27500 by default). Its direct
56-byte record has no pointers or implicit padding and requires the exact build
digest. It contains a browser cookie, authority instance identity and game port;
names, occupancy, rules and admission are still obtained through the existing
encrypted GNS browse transaction. Browsing allocates no pilot or game object.
The probe does not change game records, GNS transport or WinTrek/FedSrv/IGC.

Active LAN browsing sends one discovery query per second to loopback and active
IPv4 interface broadcast addresses. Host-local multicast on 239.255.27.50 uses
the loopback interface and TTL zero so that multiple authorities on one machine
are found even on macOS, where loopback broadcast does not fan out. The multicast
query cannot leave the host. Interface replies are deduplicated by authority
instance. Socket work is nonblocking and limited to 32 received probes per
100 milliseconds; metadata uses at most four concurrent GNS queries. Discovery
sockets are released when leaving LAN entry, and stopped discovered authorities
expire after five seconds. Explicit `--server IP:PORT` and `--connect IP:PORT`
remain available when LAN broadcast is unavailable. No additional settings
surface or invented default UI is introduced.

Regression coverage includes two simultaneous shared-port authorities, actual
host-local fanout, stable instance IDs, duplicate reply suppression and responder
teardown. Frontend checks exercise empty/searching state, sorting, selection,
scrolling/paging, admission gates, double-click joining and selected-server
retirement. The encrypted browser check discovers an authority without a supplied
endpoint, validates its actual metadata without pilot/world allocation, and
removes it after shutdown. Actual SDL/Wayland input from the original main menu
discovers a temporary authority, selects its original row and double-clicks
through GNS into the original remote team lobby. Vulkan validation reports no
errors; the original one-times-scale capture is visually checked.

Linux, Windows and macOS release client/server builds pass with common source
identity `65a3bc602322592fbee78dc80b5f52426dd105cb34195ea02bbc6374c7f7e594`.
The Linux session and encrypted browse/expiry suites pass. The headless shared-port
socket test passes on Linux, in the Windows executable under Wine, and natively
on the test Mac. The macOS test initially exposed missing loopback broadcast fanout;
the host-local multicast correction is checked on all three platforms. These
checks validate discovery and UI behavior, not presentation latency. The new
macOS test was copied to `/tmp`; this batch does not deploy a new application bundle.

This completes LAN discovery and selection, not the legacy Club directory.
Public directory registration and remote multi-game creation remain in the
service queue. Find Player across known/discovered authorities is completed
in the later audit below. A native authority still hosts one match.
Validation logs are `/tmp/allegiance-lan-*.log`; all owned test clients and
authorities are closed after validation.

## Local game entry and team state completion

This grouped functional pass follows `newgamescreen.cpp`, `teamscreen.cpp`
and `hangarscreen.cpp` through creation, readiness, briefing, actual IGC
construction and leaving the match. It also exercises the corresponding
encrypted lobby actions.

| Surface | Functional correction |
|---|---|
| Game names | The original Create Game dialog accepts 64 characters. Actual mission validation now uses the same 65-byte storage capacity, so an accepted maximum-length name can start a game; 65 characters remain invalid. |
| Faction selection | Offline list indices are resolved to actual core civilization IDs. Defaults remain valid for small/noncontiguous civilization sets, and changing the human's side or team count preserves previously assigned factions. Team labels, watermarks, settings drafts, briefing and actual IGC sides agree. |
| Team state | Starting a new local game clears retired team names, activity, scores and private account rows, and selects its own team. Accepted team names appear in the original prompt. Editing a local team does not invent a new personal AFK transition. |
| Readiness | Original Join clears the joining pilot's AFK state. Team Ready clears the commander's personal AFK state as well as changing the force-ready bit. The native network UI submits the existing personal-ready action before the existing force-ready action; server policy and records are unchanged. The original in-progress AFK face is unchecked for an assigned pilot. |
| Cancellation and leaving | Briefing Cancel restores the team screen and chat focus from which Start was invoked, rather than returning to settings. Invalid mission settings display the actual validation reason. Team Back opens the original quit confirmation and retains its parent. Local conquest leave returns to Games, training leave to Training, and network leave to its entry screen. |
| Offline public roster | Both factions' human pilots appear on the original team screen. Public identity, role and score rows remain separate from allied account/location rows. The roster is refreshed from human accounts on economy changes; it does not scan AI ships every frame or expose opposing balances, equipment or locations. Authority/replica publication is unchanged. |

The original hangar Team button deliberately toggles the expanded flight roster
over the hangar; it does not navigate to the full pregame team screen. Actual SDL
coverage preserves this behavior and tests the hangar Exit confirmation separately.
The headless team-screen check independently tests Back/quit cancellation and
acceptance, including stale-session teardown.

`test_reference/local_game.hpp` checks the above state transitions and constructs
a real IGC mission with a maximum-length name, three public human seats and two
private allied account rows. It verifies the actual side civilizations against
the lobby assignments and renders the opposing public seat through the original
roster painter. Linux session and player suites pass. Encrypted lobby and team
suites pass, including leader permissions, configuration, readiness, countdown,
reconnect, victory and next-round behavior.

Actual Wayland/SDL local entry passes 930 frames through original Create Game,
settings, disabled Start, Team Ready, team-name editing, settings Cancel,
briefing Cancel/restart, actual docked game creation, hangar roster toggling and
quit Cancel/accept. A separate rendered games/lobby run verifies the AFK-to-ready
command sequence, encrypted personal/team state and original cleared AFK face,
alongside filter/edit/popup, persisted wing and quit handling. Captures use the
original artwork at 1x; Vulkan validation reports no errors.

The final Linux release client/server and check binaries have common source
identity `fcb9352441a55b0936aee5500c751ac23ecfc7ba305835ae9cb734811104a037`.
The last diagnostic-only adjustment delays dismissal until after roster/dialog
capture; it does not change runtime behavior or protocol.

Logs and captures are `/tmp/allegiance-local-flow-*`. These portable native
changes are checked on Linux without repeating Windows/macOS builds. No
WinTrek, FedSrv, IGC or wire record is changed by this pass. The remaining
service, platform and visual comparison work stays explicit below.

## Graphics-device and force-feedback actions

The original Graphics Device brightness actions and force-feedback Auto Center
action are now functional rather than disabled placeholders. The port imports
and saves `Gamma` and `FFAutoCenter` independently of cue gain/enabling and
controller mappings. Brightness uses `enginewindow.cpp`'s multiplication/division
by 1.01, `engine.cpp`'s range of 1–2 and WinTrek's startup default of 1.13.
The original U/D and C popup accelerators, immediate application and popup
parents are preserved. Current-device rows show the actual selected Vulkan GPU,
API version, framebuffer extent, borderless window/fullscreen state, pixel format
and gamma. They refresh after brightness application and drawable-size events.

The native display ramp operates on the completed image, after all world, UI,
alpha and additive composition. Applying gamma separately to each layer would
give different results. A compatible UNORM sampling view reads exact encoded
framebuffer bytes. A cached 256-entry table produces the original gamma response
and compensates the final sRGB attachment conversion. There are no per-pixel
power functions, filtered UI resampling or duplicate resident asset textures.
Each reusable frame owns its attachment, descriptor and small table; updates
touch only the frame whose fence has been waited. The table changes only when
gamma changes. Neutral gamma renders directly to the swapchain without allocating
or drawing the optional ramp attachments. Enabled gamma adds one fullscreen
triangle. Existing input/present scheduling is retained.

Auto Center sets SDL's supported global haptic auto-centering to 0 or 100 when
the controller is configured or hotplugged, independently of fire/impact/explosion
cue enabling. Unsupported devices retain ordinary input and rumble behavior.
Gain is sent only when the device advertises the corresponding capability.
No device query or update is added to idle input sampling. WinTrek's existing
Auto Center menu saves a preference but its DirectInput `SetRanges` implementation
always writes zero; the native menu applies the requested device behavior.

The grouped configuration regression covers action multipliers/clamps, popup
labels, scoped runtime effects, original preference import and independent
persistence. The Linux session suite passes. The virtual-controller suite passes
all existing focus/hotplug/throttle/button/hat/rumble checks, including 1,310,720
original response-curve comparisons. This is not a physical force-feedback
hardware validation.

The full Vulkan pixel suite passes. New coverage checks all 256 encoded values
at multiple brightness levels, original startup gamma, alpha/additive final
composition, exact texels, both frame slots, live changes and restoration of the
neutral direct path. The existing authored UI/crew/options/game-list/loadout,
planet, radar-key and star projection checks remain passing. Vulkan validation
reports no errors. Logs are `/tmp/allegiance-device-actions-*.log`.
These native changes do not modify WinTrek, IGC, server behavior or wire records.

Actual Wayland/SDL testing passes 300 frames through the original device and
force-feedback popups: selected device/extent, U/D runtime application and saved
gamma, refreshed device rows, Auto Center C/label/persistence and Escape back to
the original Home parent. The 1x captures are visually checked. The run records
zero presentation misses at 165 Hz and no Vulkan validation errors; this short
UI run is not a general GPU-performance benchmark. All owned checks/clients are
closed. Linux release client/server and checks build with common source identity
`15e1d08cb948b7ae14301bc611192edee1b7a6e04dc6f72d1e95eb7e6dcf94c3`.
Windows/macOS builds and physical haptic validation are not repeated in this
portable batch.

## Original graphics-quality actions

The original Advanced Options menu now applies antialiasing and maximum texture
size, following `WinTrek::ShowOptionsMenu`, `engine::SetAA`, the DX9 preferences
and `surface.cpp` eligibility rules. Both actions apply and save immediately
without reconfiguring the input, audio or gameplay systems.

| Original control | Native behavior |
|---|---|
| Antialiasing, A | Cycle No AA and the device-supported 2/4/8/16 sample counts. Vulkan has no 6-sample mode; unavailable counts are omitted and imported requests fall back to a supported lower count. No AA remains the default. |
| Max Texture Size, X | Cycle 256, 512, 1024 and 2048 pixels, with the original 256 startup default. Eligible mipmapped model images use resident views starting at the appropriate mip level. Irregular image dimensions and non-mipmapped assets remain exempt. |
| Original preferences | Import numeric/boolean `UseAntialiasing`, `Graphics.UseAntialiasing`, `MaxTextureSize` and `Graphics.MaxTextureSizeLevel`; namespaced settings override the legacy fallback. Save the resulting native quality preferences independently. |
| Classic artwork | Nearest UI descriptors retain full-resolution images independently of world sampling. Original bitmap texels, layout and automatic integer scaling retain their existing behavior. |

Optional MSAA uses transient multisample color/depth attachments, hardware
average resolve and compatible world/effect/HUD/preview pipelines. The completed
resolve feeds the existing final brightness pass. Changing sample count queues
one resource/pipeline rebuild at the next frame boundary, before pacing and
input sampling; changing only texture limits updates CPU descriptor selection
without GPU waits, uploads or resource recreation. The default AA-off path needs
no multisample color attachment or resolve.

All texture mip levels remain resident for immediate quality restoration. The
texture limit reduces the sampled mip footprint, **not allocated image memory**;
this deliberately avoids WinTrek's texture reupload/device-reset approach. Mip
views and bindless descriptors are constructed once with the asset, with no
additional per-fragment branching. This does not change WinTrek, IGC, server
behavior or wire records.

Linux release client/server, session, crew and render checks build coherently
with source identity
`93615513fc2597dfa6efc238b6e7628d893e6de12e29de476cda537bf230ed04`.
Session/crew regressions pass, including observer/gunner replacement visibility
and actual launch scene submission. Vulkan pixel checks pass hardware edge
coverage/resolve, brightness after resolve, alpha/additive depth ordering,
depth-cleared model previews, AA-off restoration, reduced/restored model mip
detail, full-resolution shared nearest UI and retained resident images. The
full existing compositing/original-UI/radar/planet/starfield suite also passes
with no validation errors.

Actual Wayland SDL menu testing passes the original device/brightness/feedback
controls, supported AA cycling, all four texture limits, refreshed labels,
saved preferences and Escape back to Home. The 1x capture is visually checked.
The 510-frame run records no misses at 165 Hz (a short UI check, not a general
GPU performance benchmark). Logs are `/tmp/allegiance-quality-actions-*.log`.
The actual crew/loadout/refit/launch SDL run also passes 1,112 frames, including
restored replacement-ship visibility, scene submission and original camera
transitions, with no validation errors or presentation misses at 165 Hz. Its
launch capture is visually checked and shows the ship.
Windows/macOS builds are not repeated for this portable batch; platform-specific
GPU sample-count and physical force-feedback checks remain for playtesting.

## Original connection reports and shared MML text

The main menu's **Player Pings (P)** now closes the menu and requests connection
information, matching `WinTrek::ShowPlayerPings`. Offline selection simply closes
the menu, as in the original. Network replies open the original `mmlpane` popup,
with its authored background, scrollbar, fonts, yellow heading, cyan self/name
labels, original green/yellow/red quality thresholds, padding and averages.
Escape/Enter and the authored close button restore the docked or flight parent.
Public pilot names are inserted literally and cannot become MML directives.

GNS supplies server-observed round-trip times and remote outgoing-delivery
quality. The original **Packet Loss** label is retained, but this value includes
unusual packet sequencing as well as dropped packets; it is not DirectPlay's
retry ratio. Unknown GNS quality displays zero loss rather than a false 100%.
Only current connected human pilots are reported, using the already-replicated
public roster for names. Statistics are never collected or transmitted each
simulation tick. One explicit request is 32 bytes; the response is a 32-byte
header, an 8-byte count and 8 bytes per pilot, before GNS framing/encryption.
The server caches the sampled and formatted rows for five seconds, sharing them
across requesters and invalidating them on connection membership changes.
Responses use the reliable bulk lane and match/query identities; the client
allows one outstanding request and consumes a response once.

Game Details and Player Pings now share a compiled MML report-text layout rather
than separate painters. This supports asset-resolved Font/Color/String tags,
literal dynamic values, whole-word wrapping, the original 16-pixel tabs,
default-font paragraph spacing and bottom-aligned mixed fonts. Adjacent spans
of identical style merge, and an ordered line index restricts composition to
the visible page. This is the report-text subset, **not** the complete legacy
Help hyperlink/image/topic system. Both MDL compilers also resolve the seven
named colors exported by the original effect namespace, preserving authored
overrides. WinTrek's rendering and frame loop are unchanged.

Linux release client/server and session/crew/network/render checks build with
source identity
`50d0bbc601d2aea285fd6d6c5ed9cceb336bcea1d5ebe1f6a6bcf0581769dc69`.
The session and crew suites pass. Report coverage includes exact PagePane
paragraph/tab/wrapping/font behavior, literal names, thresholds and averages,
original menu accelerators, docked/flight restoration and full-roster scrolling.
Opening 1,024/2,048 pilot reports takes 2.041/3.035 ms in this run and each
submits 90 visible text spans; these are short synthetic observations, not
general performance guarantees. Actual encrypted two-faction GNS checks pass
request-only delivery, duplicate-query suppression, cache reuse and departure
invalidation. The full Vulkan suite passes, including original report fonts,
quality colors, artwork and flight composition, with no validation errors.

Actual Wayland/SDL testing passes Escape/P from hangar and flight, encrypted
responses, read-only capture, original Launch and Escape/Enter restoration.
The 717-frame run records one presentation miss at 165 Hz and no missing
network baselines. Original 1x captures are visually checked. A separate
1,112-frame boarding/refit/launch/camera run passes with zero presentation
misses; its launch capture visibly includes the player's ship. All owned
clients/checks and the temporary server are closed. Logs and captures are
`/tmp/allegiance-connection-info-*`. Windows/macOS builds are not repeated for
this portable batch.

## Original custom-map setup and launch

The original map combo now reads `maps.txt` in FedSrv's catalog order, following
the authored generated-map choices. It retains friendly names and original IGC
basenames through setup drafts, accept/cancel, browser/session records, authority
configuration and replica bootstrap. Generated-map choices and game presets
clear the file override. Team-count errors use the original wording; remote
configuration uses the original non-dismissible **Changing mission parameters...**
message until acceptance or rejection.

The native branch of the original map preview maker reads the actual IGC
cluster, aleph and station records, preserving arbitrary sector IDs, single-sector
maps, no-aleph maps and station ownership. It checks record boundaries and
topology before creating a preview. Briefings use the catalog name and the
authored home-sector name. Resource lookup preserves Windows filename case
semantics on Linux/macOS, using a directory fallback only after an exact miss.
WinTrek's existing renderer and normal preview branch are unchanged.

Long point-anchored menus now fit on one side of the opening pointer, preventing
the opening release from selecting a row accidentally. Reopening a scrolled
combo reveals its current choice without forcing later scrollbar movement back
to the selected row. Original arrows/Enter remain the small-menu controls;
overflowing native lists additionally retain their existing paging controls.

The installed catalog has 39 entries, but **AW5Lead (`m2v2lead.igc`) is absent
from the Steam artwork**. Its selection reports no preview, and creation rejects
without silently substituting a generated map. The server checks availability
before replacing its authority, so an unavailable-map request preserves the
current match and connected pilots. The compact session/configuration record
adds a 16-byte basename field; this adds no per-tick reporting. Catalog metadata
also participates in the existing content-compatibility digest.

Linux release client/server and session, setup, crew, network and render targets
build with source identity
`5a9c6d4dd7ebc57a3375b0a17daee00a1ee9f150b074b2c34170822102c91ba8`.
Session and crew checks pass. All 70 supported generated map/team combinations
and all 38 available custom files initialize and launch; every custom preview
matches IGC sector IDs/names, aleph links and station ownership. Malformed and
unavailable files reject, and mixed-case filenames resolve. Encrypted GNS checks
pass multi-sector and single-sector configuration, authority/replica agreement,
unavailable/team-mismatch rejection without reset/disconnection, and restoration
to a four-faction generated mission.

The full Vulkan compositing/original-UI suite passes with no validation errors.
Actual Wayland/SDL testing passes catalog scrolling/End selection, authored
preview, wrong-team rejection/correction, lobby, briefing, hangar and visible
launch. Its final 663-frame run records two presentation misses at 165 Hz;
these short startup observations are not a steady-state pacing benchmark.
Original 1x captures are visually checked, including the ship during launch.
The launch regression also checks the persistent visibility flag and actual
scene submission after boarding/refit. Logs and captures are
`/tmp/allegiance-custom-maps-*`; all owned test processes are closed.
Windows/macOS builds are not repeated for this portable batch.

## Original mission validation and authoritative briefings

Mission setup now preserves the original no-winning-condition and admin-only
validation messages and precedence, and the guard against reducing the team
count when all remaining teams would be inactive. The client checks its staged
draft; the authority checks before replacing the mission. Rejection preserves
the match and cancellation snapshot. Shared `ClientCore/MissionConfiguration.h`
policies also serve WinTrek/IGC. Native pilot capacity remains uncapped, and
authority-authored no-goal missions remain supported.

Generated briefing text uses the actual home sector selected by FedSrv's first
restart-capable station policy, shared through `ClientCore/MissionBase.h`.
Offline Start prepares the original IGC mission once, freezes it during the
countdown, and reuses the same world at launch. Cancellation releases that
world and its briefing. The countdown begins after preparation finishes, so
loading time does not consume the visible countdown.

The authority accepts literal story text through `allegiance-server --story
FILE`. Its reliable briefing record is separate from browser/session and tick
reports. Generated sources are encoded once per faction; a shared story is
encoded once per match. Joining, changing team and reconnecting reuse those
bytes. The client preserves literal text, including percent signs and bracketed
text, without formatting, generated-objective additions or the generated
briefing's 2,048-byte formatting limit. Late authoritative delivery updates an
already-open briefing. No Team clears faction text. Client configuration cannot
replace the authority's story.

The setup audit also checks the original dropdown implementation:
`ComboPaneImpl::SetSelection` changes the face, while `IMenu` retains a separate
first/hovered highlight. Native string combos preserve that interaction,
including cancellation and reopening; the team-reduction test selects the
original first row rather than assuming the menu starts on its displayed value.
WinTrek's renderer and frame loop are unchanged.

Linux release client/server and session, setup, crew, network and render targets
build with source identity
`c47d59885398cd360a17a31baec9904c6ce15154faf5de5ef797fa973866963e`.
The full session suite passes, including all seven goal combinations,
original validation precedence, 1,024 local seats, inactive-team reduction and
cancellation, actual frozen home-sector preparation/release, long literal
stories and late delivery. Encrypted GNS checks pass authoritative home sectors,
shared encoding, No Team/team changes, reconnect, authority-owned story
persistence and rejected configuration without a reset. The custom-map and
briefing cases are registered in CTest and both pass. Setup checks cover all 70 supported
generated map/team combinations and all 38 installed custom maps; the crew
visibility regression passes.

The full Vulkan suite passes with no validation errors. Actual Wayland/SDL
custom-map testing verifies the briefing's actual home sector, reuse of the
prepared world, hangar and visible launch. Original 1x briefing and launch
captures are visually checked. The 802-frame run records four presentation
misses at 165 Hz; this short startup observation is not a pacing benchmark.
Logs and captures are `/tmp/allegiance-briefing-*`; all owned test processes are
closed. Windows/macOS builds are not
repeated for this portable batch.

## Allied station service and pending refit transitions

The original `FedSrvSiteBase::AlliedRearm` and carrier rearm share refill and
cargo rules, but only the carrier variant remounts depleted launcher equipment.
Native allied stations previously used the carrier variant. The common policy
now lives in `ClientCore/CarrierLoadout.h` with separate carrier/allied entry
points; the native client and original FedSrv use the same allied entry point.
Its constant template parameter preserves the existing carrier path without
adding a runtime branch or allocations. The original WinTrek client requests
rearm/launch after a crewed allied docking notification, so automatic relaunch
and crew preservation remain the intended visible behavior.

The loadout editor now tracks station identity alongside docking and crew
context. A context change refreshes or retires its preview immediately, including
the interval before the economy revision changes. Combined context/economy
changes compute a preview once, and steady synchronization creates no temporary
IGC ships. This is native presentation state, with no additional wire data.

Purchase lockdown checks cover both original messages, non-dismissible pending
input, docked view-command capture, retained fitting and safe post-launch
retirement/reopening. The network purchase check now makes a requested part
available at the client, queues purchase/launch, then withdraws authoritative
stock before execution. The result preserves the actual fitted ship, blocks
launch on partial fulfillment and remains subject to canonical-report ordering.
Reconnect, cached completion, docking, fatal recovery and teleport replacement
continue to pass. The broader lifecycle check's obsolete expectation that
docked disembark leaves a pod is corrected to the already-restored original
replacement transaction.

Linux release client/server and carrier, lifecycle, session, loadout, crew and
network targets build with source identity
`f9c810959d8bd0fa7eb6b7289e5ff54dd5a3b07126881bc5ed33c5aea4a46fd9`.
All checks pass. A frozen copy of the original allied equipment policy first
reproduces the mismatch and then passes with the correction; the carrier suite
contains 705 original server/offline/carrier/allied comparisons across missing,
paid, blocked, upgraded and partially depleted equipment. Encrypted solo and
crewed allied-base cases verify own-faction technology, original cargo placement,
repaired resources, relaunch, crew relationship/caption, pilot/hull identity,
wallet preservation and absence of duplicate dock-refit transactions. The allied
case is registered in CTest and passes.

Actual Wayland/SDL testing with Vulkan synchronization validation passes original
staging, purchase lockdown/completion, launch, autopilot docking, default refit
and relaunch. Original 1x loadout and pending-purchase captures are visually
checked. The 3,268-frame run records one presentation miss at 165 Hz, 221 network
snapshots and no missing baselines, with no validation errors; this short run is
not a general performance benchmark. Logs/captures are
`/tmp/allegiance-refit-parity-*`; the earlier regression logs are
`/tmp/allegiance-allied-rearm-*`. All owned test processes are closed.
Windows/macOS builds are not repeated for this portable batch.

## Full launch visibility follow-up

The reported missing ship does not reproduce in the current Linux build. The
previous remote refit diagnostic returned to base during the first launch camera
and ended almost immediately after the second launch. It now waits through both
complete external-camera sequences. Every rendered launch frame checks the
ship's persistent visibility flag, its inclusion in scene submission, the
camera's request to show it, and its projected position inside the view. The
encrypted crew regression also checks replacement-hull visibility and scene
submission throughout the camera after a boarded pilot purchases and launches
their own ship. The original cockpit policy continues to hide the player's hull
after the override camera finishes.

Linux client/server, crew, camera and network targets build with source identity
`62a8a49784329ed5721156d7ad08916e52e33063b4d888f8d0128d96da84d715`.
Crew, camera and encrypted crew tests pass. Actual Wayland/SDL initial launch,
autopilot docking, refit and relaunch pass; visibility/submission/framing checks
pass for all 244 initial-launch and 469 relaunch frames. Six actual Vulkan
captures, near the beginning, middle and end of each camera sequence, are
visually checked and show the Scout. This establishes the current reproduction
result, not that every possible launch disappearance has been ruled out.

The 4,610-frame run has no Vulkan validation errors or missing network baselines.
Logs and captures are `/tmp/allegiance-launch-visibility-*`; all owned test
processes are closed. This follow-up changes diagnostic coverage, not gameplay
or rendering policy. Windows/macOS builds are not repeated.

A repeat on the current source identity
`18fecbd217372fff94cc25dc2f2cdc6f742a85fd69c79842ebda8491f8860160`
also passes camera and encrypted crew regressions, plus a complete Wayland
purchase/launch/dock/refit/relaunch. Visibility, scene submission and framing
checks pass for all 396 initial-launch and 466 relaunch frames. All six Vulkan
captures show the Scout; the run reports no Vulkan validation errors or missing
network baselines. Evidence is `/tmp/allegiance-launch-followup-current-*`, with
the six captures together in `allegiance-launch-followup-current-contact.png`.
This still does not reproduce the reported disappearance; no additional
rendering or camera-policy change is made.

## Command-sector navigation and deferred focus

The original `WinTrek::PositionCommandView` position/distance branch now lives in
`ClientCore/CameraFraming.h`, used by both WinTrek and the native client. An
explicit focus selects the original 10 km command distance; a default focus
retains zoom and observes the station/ship minimum distance. Native sector
selection previously discarded zoom and centered at the origin. It now uses the
original default-position policy, including an off-center docked station.
Take Me To also retains command roll instead of resetting it.

Faction-wide ship reports intentionally omit precise remote positions. Take Me
To previously centered on that strategic placeholder and never corrected it
when the physical sector report arrived. Native now retains one pending focus
as ship identity and sector IDs, waits for the target's physical record (the
parent's record for crew), and applies the original framing after arrival.
Station targets use their known station position. Manual pan/roll/zoom,
perspective/tracking changes, sector changes, combat view, seat transitions and
target retirement supersede pending focus. No persistent model pointer, new
wire field, additional report or report encoding is introduced. The normal
WinTrek path gains no allocation or mission scan.

Native command/combat camera actions are submitted after local admission,
mode and launch-camera validation. Previously a locally rejected camera input
could still switch the authority's view. Sector IDs are bounded before conversion
to IGC's signed short ID, preventing wrapped aliases and unnecessary actions.
The authored docked/viewed-sector HUD bindings are checked and already use the
correct original station/view data; no location-label behavior is changed.

Linux client/server, camera, player-pane, session, crew, UI-script, network and
render-check targets build coherently with source identity
`18fecbd217372fff94cc25dc2f2cdc6f742a85fd69c79842ebda8491f8860160`.
Camera, player-pane, session, crew and UI-script checks pass. Camera checks
compare 18 framing cases with the frozen original branch and cover actual
replica admission/launch gates, valid actions, invalid modes and sector ID
bounds. All six authored HUD styles retain their docked, cockpit and viewed
sector text. The encrypted command-focus case first reproduces the missing
arrival focus and then passes remote pilot, crew and station focus, manual zoom,
superseding sector selection and peer departure/retirement. It is registered as
`native-network-command-focus` in CTest. Existing encrypted sector, purchase and
crew cases also pass, preserving pilot prediction, refit/recovery and launch
visibility.

Actual Wayland/SDL command-view testing passes original group selection and
waypoint controls with the authored 1x interface. Its capture is visually
checked. The 750-frame run has no Vulkan validation errors or presentation
misses at 165 Hz; this is a short functional check, not a general performance
benchmark. Logs/capture are `/tmp/allegiance-command-parity-*`; the initial
reproduction and focused iterations are `/tmp/allegiance-command-focus-*`.
All owned test processes are closed. Windows/macOS builds are not repeated for
this portable batch.

## Shared authored slideshow rendering and training transitions

Training introductions/debriefs previously had a separate MDL reader and image
painter. It dropped the authored StringImage font, color and justification,
used a different word-wrap/line-height policy, and treated ImportImage's color
key flag as additive blending. Slides now use `OriginalUi::imageList` and the
existing compiled MenuView image painter. The shared compiler retains linked
definitions, image composition, transforms, clipping, fonts, colors,
justification and separate alpha/additive flags. Image/value pairs and unresolved
cue names are validated at load time. Narration still uses the original sound
IDs and real completion followed by a one-second gap. Artwork and font textures
are preloaded; no MDL parsing, artwork copying or new texture allocation occurs
in a steady slide frame. The original 800 x 600 canvas and automatic integer
scale apply through the existing menu layout.

The shared menu painter also previously ignored an additive flag on image and
rectangle controls (text already honored it). Both now carry the authored mode
into the ordered HUD batch. Independent Vulkan probes confirm that adding red
preserves the green/blue background, alongside the existing complete menu/HUD
pixel suite.

WinTrek's Slideshow advances once for every keyboard-down or mouse-button-down,
including Escape and repeated keys. Native previously used Escape to skip the
whole sequence and suppressed repeats. The original input policy is restored.
On introduction completion, the last authored image remains behind the original
non-dismissible "Training simulation initiated..." message for its 0.1-second
delay before loading the mission. Introduction-only and post-training sequences
still return to the training menu. The test's double click now includes its
first select-on-release click and separate second launch-on-down event; it does
not change the original ButtonPane policy.

Linux native/client/server/network, training, session, UI-script and Vulkan
render-check targets build coherently with source identity
`97852bd77a205890d88086aab3edbd30ed5b6bafe2fbedfa73740ed629433928`.
UI-script, session, full training and actual Vulkan pixel checks pass. The slide
audit includes all 77 entries across the introduction and prior lesson/debrief
sequence cases, 16 authored text/frozen StringImage wrapping comparisons and
17,796 color-keyed circle texels. Independent alternate MDLs check imported
font/cue aliases, colors, justification, translation, clipping, layers and
malformed/missing values. Vulkan checks verify largeFont's first glyph position,
size and opaque coverage, circle alpha blending and stable resident textures.
The complete training simulation suite retains lesson startup/teardown,
command-view exercises, combat/repair outcomes, station service and arena waves.

Actual Wayland/SDL testing passes arena death/debrief, original lesson selection
and double-click launch, all five nanite introduction slides, Escape/repeat/
right/middle/Space advancement, presented startup lockdown, fresh lesson and
explicit exit. Its 864-frame run has no Vulkan validation errors; slide and
startup-message captures are visually checked. This is a functional workflow
check, not a pacing benchmark. Logs/captures are
`/tmp/allegiance-training-parity-*`. The initial sandboxed training run completed
its reported lesson checks but stalled in PulseAudio teardown after an EPERM
message; its owned process was terminated and the desktop-access rerun exits
successfully. All owned processes are closed. WinTrek and gameplay/network
policies are unchanged, and Windows/macOS builds are not repeated for this
portable batch.

## Launch-to-cockpit hull visibility

The full external launch captures still show the Scout, but there was a real
visibility mismatch when the override ended. Native called `resetCamera`,
discarding the last external pose and cutting into a cockpit that hides the
player's hull. WinTrek's `EndOverrideCamera` instead changes the previous mode
to external, then `SetViewMode`/`SetCameraMode` starts the normal one-second
cockpit transition (two seconds for preferred chase). WinTrek keeps the hull
visible while transitioning from an external camera into the cockpit.

Local simulation and network prediction now end a live launch override without
discarding its applied pose or presentation clock. The existing shared
`CameraMotion` performs the original transition, and native scene submission
continues to include the hull until the cockpit settles. Death, docked and
invalid-ship resets retain their existing behavior. WinTrek is unchanged; the
steady render path gains no scans or allocations.

The regression first fails on the previous implementation with "launch exit
cuts to the cockpit and prematurely hides the player's ship". It now checks
the frozen cosine position interpolation, one-second cockpit and two-second
chase durations, persistent visibility and scene submission, then verifies
that the settled cockpit hides the hull. The encrypted crew test checks the
same transition after boarding and purchasing a replacement hull. Camera,
crew, all original training missions and encrypted crew tests pass. The SDL
refit diagnostic waits for the complete rendered transition and checks each
rendered transition frame, without sampling the camera outside rendering.

Linux targets build coherently with source identity
`0553c6f8562363c8812126de6e99b12bff5691a077ffa0f2ac1b7050ae145d51`.
The final actual Wayland purchase/launch/dock/refit/relaunch checks visibility
and scene inclusion for 380/469 external-camera frames and 162/160 cockpit
transition frames. Both launch and transition captures are visually checked
and show the Scout. Its 5,440-frame run has no Vulkan validation errors or
missing network baselines. Logs and captures are
`/tmp/allegiance-launch-transition-*`, with the final comparison in
`allegiance-launch-transition-final-contact.png`. All owned test processes
are closed. This fixes the reproduced premature hiding at launch exit; it
does not establish a cause for invisibility throughout the external launch
sequence, which still has not reproduced. Windows/macOS builds are not
repeated for this portable change.

## Original lobby action side effects

The lobby audit compares the actual `TeamScreen` handlers, including their
effects on chat selection, personal AFK, team force-ready and pending requests.
Accept and Make Leader now clear the selected player/private chat recipient
while retaining the action's copied target and unsent draft. Reject retains
selection, as in WinTrek. Opening mission details withdraws a ready non-owner
commander's team force-ready before setting personal AFK, and cancels an
outstanding team request. Viewing previous scores only cancels the request;
opening Team Settings uses the same AFK rule without cancelling the request.
Explicit JOIN/CANCEL leaves personal AFK before cancelling, just as joining
does. Returning from details does not restore readiness.

One allocation-free native adapter submits these existing ordered control
actions. The encrypted regression uses the same adapter. There is no new
wire record, report encoding or simulation work. The seven team/No Team voice
buttons now resolve to WinTrek's corresponding invitation, using a shared
literal destination table; the general community button retains its general
invitation. Tests inspect the targets without opening external sites.

The session suite checks all 16 owner/commander/ready/request combinations,
button and context-menu Accept, Make Leader/Reject selection policy, unsent
drafts, record order, unnecessary-action suppression and all seven voice
destinations. The encrypted team case passes AFK/force-ready withdrawal,
pending JOIN, previous-score cancellation, explicit JOIN/CANCEL wake-up and
return to the original command seat without replacing the world, round or
pilot. Existing faction/base/hull/miner/money, recipient state and lock tests
also pass. Full Vulkan UI pixel checks pass.

Actual Wayland/SDL testing passes original Team Settings, dropdown/name
editing, cancellation, briefing/countdown/No Team timers, read-only mission
details, the global Escape main-menu popup, Back, retained AFK/chat draft and
Make Leader with cleared private selection. The initial test incorrectly
expected Escape to return directly from NewGameScreen; WinTrek's global
`TK_MainMenu` filter instead opens the main menu, and the authored Back button
returns to teams. The diagnostic now checks that original route. Captures of
the dialog, details/main-menu composition and returned lobby are visually
checked. The 560-frame run exits successfully with no Vulkan validation
errors. It records one presentation miss at 165 Hz and is a functional test,
not a pacing benchmark. Logs/captures are `/tmp/allegiance-lobby-actions-*`.
The final coherent Linux client/server/network/session/render-check build has
source identity
`3bee515ea062a560bc360f095fb94948e17755011ccb63f48c314b588780ec3e`.
WinTrek is unchanged and Windows/macOS builds are not repeated for this
portable batch.

The same audit identified the missing owner-only team right-click alliance
workflow. Its implementation and validation follow below.

## Original team alliance menu and live policy

Team right-click now selects the row, clears the private chat recipient and
opens the original menu above the pointer. It offers solo teams in team order,
then existing groups in group order with WinTrek's last-member representative,
or Remove Allies for an already allied side, followed by Cancel. The same
resident bitmap font, palette, raised border, keyboard navigation, menu release
policy and original colored team-edge images are used. Cancel, Escape and a
lost owner/stage/active-side gate close the menu without navigating away or
changing groups. The unsent chat draft survives. No Team has no actionable
alliance menu.

The menu is owner-only, before the start countdown, with at least three active
teams and no artifacts, kills, flags, prosperity or territory goal. The native
authority independently enforces these gates and valid, distinct active side
IDs. `ClientCore/LobbyAlliances.h` contains the original choice/gate/rule policy;
the existing shared `ClientCore/Alliances.h` still implements group merging,
removal and normalization. Local pregame setup and remote owner actions both
use it. The native action adapter sends one existing Lobby control action,
including an explicitly disabled allied-ripcord preference.

Authority changes update the existing IGC sides and mission parameters in
place. World, match, human ship objects, ownership, connections and recipient
state survive. Non-owner ready commanders lose personal ready and team
force-ready consent, and the countdown is cancelled. Existing alliances keep
defections/shared sight and disabled balance; a deliberate ripcord opt-out is
preserved. Removing the final group clears the allied rules and returns to
original automatic balance. The rule flags remain consistent with surviving
groups after a partial removal.

The existing rare reliable MissionPolicy record now includes six compact
alliance IDs and the three rule bits, expanding from 16 to 24 explicitly
initialized bytes. It is formatted once and sent on the ordered control lane
to all relevant peers. Snapshots gain no fields, and an unchanged repeated
alliance action produces no extra policy broadcast. The session/welcome and
next-round configuration retain the effective authority policy. Delayed side
records cannot overwrite the ordered alliance groups, including side creation
after the policy arrives. Initial invalid/all-sides groups are normalized
before constructing IGC parameters; reconfiguration/welcome uses the actual
authority session rather than echoing the submitted record.

The frontend observes changes to the authoritative rule tuple, updates team
edges and open details, and preserves that tuple in Back's snapshot. Unchanged
reports previously overwrote a locally staged balance choice every frame; they
now leave staged edits intact. This small value cache allocates nothing in the
steady path. WinTrek and its simulation/render work are unchanged.

Session checks pass solo/group/Remove/Cancel labels, ordering and targets,
pointer anchoring, actual menu keyboard/release policy, draft/recipient
retention, a single ordered intent, deliberate ripcord opt-out, ten
owner/stage/count/active/objective gates, No Team, stale-owner popup retirement,
live/open/staged settings and local formation/removal. The player suite passes
143,360 frozen original alliance graph/active-team/mutation comparisons, actual
allied roster/account/chat behavior, initial canonical authority/welcome/replica
policy and atomic rejection of invalid flags/groups. The encrypted alliance
case passes two groups, merges and partial/final removal, readiness withdrawal,
owner/stage/ID rejection, ripcord opt-out, an intentionally delayed world report,
late joining and stable world/round/ship identities. Existing encrypted team
settings, mission chat, custom maps and allied rearm also pass (five focused
CTest cases, 9.25 seconds total).

The full Vulkan pixel suite passes, including original alliance captions,
highlight/point anchor, team-edge artwork, composition and resident texture
reuse. Actual Wayland/SDL testing passes original team settings/briefing and
lobby side effects followed by team right-click, mouse formation, arrow/Enter
group joining and partial/final removal. Six alliance-stage captures are
visually checked in `/tmp/allegiance-alliance-parity-contact.png`. Its 900-frame
run exits successfully without Vulkan validation errors, at 165 Hz with two
presentation misses; this is a functional check, not a pacing benchmark.
Logs/captures are `/tmp/allegiance-alliance-parity-*`. All owned processes are
closed. Linux client/server/network/session/render/player targets build
coherently with source identity
`26fa0f66773743d23f0e0f5e7e780d6c8194c8233969fbce3ba7ffb2ef24bcc0`.
Windows/macOS builds are not repeated for this portable batch.

## Original crew feedback and station-transfer lockdowns

The hangar/crew pass compares `HangarScreen::OnBoardShip`, `BaseClient`'s
`LOADOUT_CHANGE`, `LEAVE_SHIP`, `PROMOTE` and `TELEPORT_ACK` handlers,
`TeleportPane::OnButtonTeleport`, and WinTrek's autopilot input handlers.

| Workflow | Restored behavior |
|---|---|
| Observer/turret boarding | The passenger receives the original non-critical "You have boarded …" console tip. A docked parent receives the original critical passenger-boarded tip and `boardSound`. These are separate recipients, including their tip sounds. |
| Explicit and implicit disembark | The original "You have disembarked." tip covers leaving, purchasing one's own hull, launching from a crew seat, changing parent ships, and station travel that cannot retain the crew/hull. Invalid purchases leave the crew relationship intact and emit no successful-disembark feedback. |
| Promotion | The promoted pilot gets disembark then critical promotion feedback; the former pilot gets boarding then critical demotion feedback, as in the original `PROMOTE` handler. Remaining crew keep their relationships without invented boarding alerts. Manual promotion and promotion on pilot departure use the same native transaction wrapper. |
| Boarding/teleport/disembark wait | The original "Boarding …'s ship....", "Disembarking from …'s ship...." and "Teleporting to …...." messages use the existing original bitmap-font/message-box artwork. Input remains blocked until the matching accepted action is represented in a reconciled authoritative report. Receipts alone do not unlock stale hangar state. |
| Disembark into refit | An automatic replacement purchase supersedes the transfer dialog. A delayed transfer completion cannot dismiss that purchase or a failure dialog. Passenger purchase/launch shows the original disembark wait while the native atomic transaction runs. |
| Ordinary flight actions | Autopilot engagement/manual cancellation retain the original spoken announcements, without substitute text. Target selection, launching, station transfer and direct fitting no longer add native explanatory messages absent from the corresponding original handlers. |

Crew presentation/feedback and transfer-message rules live in the existing
`ClientCore/CrewPresentation.h`. Native personal tip sounds now address the same
pilot as their text, rather than broadcasting the sound to every passenger of
that ship. Ordinary ship/equipment notification sound routing remains separate.
Network transfer completion reuses existing action receipts and report
acknowledgments; there are no new wire records or periodic report fields. An
unfinished transfer survives authenticated resume, and only its matching action
can release the original dialog. These changes do not alter WinTrek or introduce
per-frame crew scans or allocations.

Local crew, controls, teleport, camera, lifecycle, session and player-pane checks
pass. Encrypted crew checks cover exact text, critical flags, sound recipients,
absence of repeated notifications across steady reports, explicit/implicit
disembark, promotion, death/pods/recovery and canonical completion after resume.
They deliberately process successful boarding on the reliable lane while
withholding publication and confirm that completion stays unavailable. This
case is registered as `native-network-crew`. The purchase test also checks the
matching station-transfer completion after hull replacement. Existing encrypted
purchase, allied rearm, team/alliance and command-focus checks pass.

The full Wayland/Vulkan compositing suite passes with synchronization validation
and no Vulkan validation errors. All three new dialog captures are visually
checked at original 1x scale; probes verify original artwork, font/placement,
absence of an active OK button, and blocked pointer hits on background controls.
Blocking retains their normal rendered button faces, matching the existing
modal policy. Captures are `/tmp/allegiance-native-boarding.png`,
`allegiance-native-disembarking.png` and `allegiance-native-teleporting.png`.
Logs are `/tmp/allegiance-crew-transfer-*`. All owned test processes are closed.

Linux targets build coherently with source identity
`e3baa0cd3d313c0731006bc688a45c227c3d7a9bf286c8a7f4a8eff87ae5a185`.
Windows/macOS builds are not repeated for this portable batch. The larger
restoration remains incomplete; the execution queue below retains the remaining
scope.

## Original economy and preset feedback

The grouped `clintlib/appmsg.cpp`, `WinTrek/load.cpp`, `FedSrv/fsship.cpp` and
`fsmission.cpp` audit restores the original messages and sound cues for these
transactions:

- Owned loadout editor Ctrl+digit saves and plain-digit loads report the staged
  hull and bank, with the original click/mount and console-tip sounds. The
  Default Loadout button retains its automatic bank-zero persistence without
  announcing a keyboard preset save. Presets continue to stage equipment before
  an explicit purchase.
- Paydays report retained personal pay, donated pay or aggregated pay and
  donations, with the original payday sound. Integer sharing, remainders and
  account routing remain unchanged, including mining and departure distributions.
- Explicit donations and approved money requests report the recipient's credited
  amount and resulting wallet. Ordinary same-team senders do not receive the
  recipient's tip or sound; the original allied donation sender tip remains.
- Income donation changes notify the donor, new investor and previous investor,
  with the first-donor investor cue. Reciprocal reversal, commander toggles,
  handover, mutiny and departures retain the original IGC donation graph behavior.
  Unchanged routing does not repeat feedback.
- Cash collection reports retained/donated rewards and plays the original pickup
  cue. Completed research uses the original critical research-completion tip and
  the development's authored completion sound.
- Changed team leaders receive the original personal leader tip and commander
  sound; teammates receive the original named leader tip.

These presentation policies live in shared ClientCore helpers with native
adapters. WinTrek is unchanged. Private text and sounds use existing addressed
notice/event records; no wire fields or periodic records are added. Payday
credits are aggregated once per faction in linear work. Research and leader
notices address pilots directly instead of switching the server's active actor
for every recipient. Newly used sounds are warmed with the core's resident sound
catalog.

Linux client/server and relevant checks build coherently with source identity
`2eca51abb74e45fe77b218ead927945c342bd558e775fd269f2fc574fab6f312`.
Six local economy/player/loadout/session/control/crew checks pass. Encrypted
normal, command-console, queued-request and ballot/mutiny regressions pass,
including private donation sounds, retained/donated paydays, original donor and
investor tips, and no repetition in unchanged reports. The actual SDL classic
editor passes Ctrl+9/9/Ctrl+0/0, file persistence, automatic defaults, unchanged
owned fitting and Back. Its original-scale capture is visually checked and the
500-frame run has no Vulkan validation errors or missed presents. Logs and
captures are `/tmp/allegiance-economy-parity-*` and
`/tmp/allegiance-economy-presets-ui*`. Windows/macOS builds are not repeated for
this portable batch. The remaining queue below remains open.

## Purchase completion and research filter parity (2026-10-06)

The grouped purchase/funding audit confirms that the original Buy/Confirm/Launch
availability rules, staged-hull money requests, full investment and right-click
ten-percent investment are already implemented. Two remaining functional
differences are corrected:

- `PurchasesPane::OnShowAll` and `OnHideCompleted` rebuild the current tab, reset
  scrolling and select its first card. If a disabled technology path loses its
  Show All access, they return to the Purchases tab. The native filter handlers
  now follow that policy instead of retaining the previous selection/scroll.
- Purchase receipts retain their reliable action identity through the native
  UI adapter. Explicit purchases, passenger purchases and automatic docking
  refits release only their own wait dialog. An older completion cannot replace
  a newer launch/refit wait, a failure dialog or a later screen. Purchase and
  boarding/travel rejection diagnostics retain the same identity and are handled
  before completions, preserving the matching failure dialog.

The action numbers already exist in the reliable receipt stream; this adds no
wire fields, report traffic or server simulation work. WinTrek is unchanged.
Local session, controls and loadout checks pass, as do encrypted purchase, crew
and queued-money-request checks. Regressions cover original filter selection and
path fallback, superseding waits, passenger completion, retired screens, rejected
purchase identity, replay identity and automatic fatal-recovery refit identity.
The Linux client/server/checks build coherently with source identity
`3bd9a3981f64706e4cb7bdc8687aa19197c5c0d8d04d266d9c64142ac7dc5234`.
Logs are `/tmp/allegiance-purchase-workflows-{build,local,network}.log`.
No renderer investigation or platform cross-build is repeated for this batch.
The remaining queue below is still open.

## Mission lifecycle and stage controls (2026-10-07)

The grouped FedSrv automatic-start/end-round audit and WinTrek team/results-screen
comparison restores the authority-owned restart policies and original transitions:

- All-ready automatic starts honor active factions' minimum human populations.
  Automatic restarts preserve the first-start and between-round intervals, warn
  at thirty seconds, force faction readiness at the fifteen-second briefing
  threshold without changing individual AFK state, and cancel at the deadline
  if a required faction still lacks pilots. Parked lobbies do not advance combat.
- Results remain visible during the restart delay. On-team pilots return to the
  team/briefing screen when the briefing begins; No Team retains the score-screen
  countdown. Timers use the authority's already rounded seconds, and Back leaves
  an ended/canceled mission. Previous scores survive next-round preparation.
- Server pause follows the original service-draining behavior: existing combat
  continues, new admissions receive the original shutdown diagnostic, and ended
  games cannot restart while paused. Disabling restart similarly preserves ended
  results. Unpausing makes an allowed finished mission restartable again.
- Game settings remain available to the owner during the automatic restart
  delay, become read-only at briefing/termination, and use actual mission stage
  rather than the presence of a countdown. The original team-settings button has
  its lock-only gate; an open draft survives the delay and is replaced when the
  briefing begins. The authority still refuses changes outside NOTSTARTED.

Shared ClientCore lifecycle policy uses existing lobby flag storage; no wire
fields or report payloads are added. Policies are supplied by authoritative IGC
mission parameters and the server pause API, without adding client settings or
operator CLI controls. The native solo/AI default continues to allow empty teams;
strict original minimum-player behavior applies when that authority policy is
disabled. WinTrek/FedSrv are unchanged by this batch.

Linux client/server/checks build coherently with source identity
`bf116fe26e137575494b4408cca94228dcf3e1454092af37652bcc1c15362d79`.
Local session/endgame/control checks pass, including 400 original deadline and
readiness combinations. Encrypted lifecycle, alliance, browser and mission
briefing checks pass. The browser check is run in isolation because its existing
LAN-discovery fixture stops at the first discovered server, which may belong to
another concurrent fixture. Actual SDL classic dialog, faction dropdown, briefing,
No Team timer, Details, focus/draft retention and alliance workflows pass in a
900-frame Wayland/Vulkan run with no validation errors. Original-scale team
settings and briefing captures are visually checked. Logs/captures are
`/tmp/allegiance-lifecycle-parity-*` and `/tmp/allegiance-lifecycle-ui-final-*`.
Platform cross-builds are not repeated for this portable batch. The remaining
queue below is still open.

## Training and LAN entry dialogs (2026-10-07)

The grouped original `introscreen.cpp`, `trainingscreen.cpp` and `logon.cpp`
comparison restores entry steps that were skipped by the native frontend:

- Default first-run entry shows the original training suggestion, its authored
  checkbox and OK/Cancel choices. The preference persists only on dismissal;
  starting a lesson clears it. Original `HasTrained` and the explicit training
  suggestion preference import correctly. Both the suggestion's OK and the
  main-menu Training button open the original welcome before the lesson list.
- LAN selection closes discovery and opens the original `logon.mdl` callsign
  popup over the main menu. No connection begins until Logon. The selected
  endpoint survives later discovery changes, accepted identity is saved and
  used for admission, and Abort/empty-name Enter/Escape return to Home.
- Callsign editing follows the original Name-click/Tab focus, empty-name button
  state, leading-space refusal and 23-byte limit, including saved-name
  truncation. Password, account, signup and CD-key controls are hidden for LAN.
  The original prefilled-name constructor focuses its hidden password; Name
  click or Tab restores visible editing. This unusual behavior is preserved.
- Replacing a training modal with an ordinary or pending dialog retires its
  training transition and checkbox state. Checking the startup box does not
  dismiss the modal or allow input through to the menu underneath.

Message text and name acceptance policy reside in ClientCore; artwork, geometry
and fonts come from the installed MDLs. No WinTrek/FedSrv code, renderer code or
wire records are changed in this batch. Authenticated account/public-directory
entry remains in the execution queue.

Linux client/server/checks build coherently with source identity
`dd86869aea0168c6ab60a0dda03377a61204a1feb66d98216205ef873624962f`.
Session, endgame, training and control checks pass, including eight training
dialog combinations, seven modal replacements and the callsign focus, limits,
cancel and endpoint-retention cases. Encrypted purchase and crew checks pass.
Actual Wayland/Vulkan training-entry and terminal runs pass in 240 and 876
frames; the final LAN run passes in 144 frames with zero missing baselines.
All three runs have no Vulkan validation errors. Original-scale checkbox,
welcome and callsign captures are visually checked. The isolated authority is
closed after validation. Logs/captures are `/tmp/allegiance-training-entry-*`,
`/tmp/allegiance-training-launch-*`, `/tmp/allegiance-entry-parity-*` and
`/tmp/allegiance-entry-lan-*`. Platform cross-builds are not repeated for this
portable batch. The remaining queue below is still open.

## Hangar, teleport and docked crew transactions (2026-10-07)

The grouped `teleportpane.cpp`, `DisembarkAndBoard`, server boarding and native
hangar comparison restores these functional differences:

- The authored hangar Command button retains the remembered top-down or
  three-quarter perspective and enters the physical base's sector.
- Selecting the current station as a passenger performs plain disembark and
  restores the pilot's fitting. It does not dispatch a station teleport or play
  the personal-jump sound; the parent and remaining crew stay docked.
- Teleport reboarding releases the old seat before selecting the first equipped
  free turret, including reusing that seat when the ship is otherwise full.
  Native authority preflights this atomically: rejection preserves the current
  seat, inventory and money. Ordinary in-flight turret cycling remains intact.
- Boarding rejects out-of-range identities before narrowing or transmission,
  and authority rejects noncanonical reboarding modes. Successful reboarding
  preserves the original ordered disembark/boarding feedback without an
  intermediate loadout purchase.
- Failed disembark uses the original boarding-failure policy. An intentionally
  silent failure after the pilot enters flight releases its matching waiting
  dialog and returns input to flight. Older receipts cannot release a newer
  transfer or erase a replacement ordinary failure dialog.

The released-seat selection helper is in ClientCore and used only by native
docked reboarding. The existing `NextTurret` path and WinTrek/FedSrv sources are
unchanged in this batch. The mode uses an existing action argument; no wire
record fields or additional round trips are added. Artwork and investment slide
placement remain asset driven. A stale diagnostic that assumed a right-edge
anchor now compares against the installed MDL's actual anchor and displacement.

Linux client/server/checks build coherently with source identity
`9bbcd09c7e624ec285d2f706799061dddc0143f31a08ad98cc651c212b317698`.
Session, teleport and control checks pass after the final edits. The existing
lifecycle check also passed during this batch, covering lost-base/lives,
evacuation, base capture/destruction and crew release policies. A dedicated
encrypted docked-crew check passes full-ship reboarding, first-free selection,
atomic failures, disembark/refit, remaining crew, action identity and the silent
stale-flight rejection, with zero missing baselines. The 700-frame actual SDL
Wayland/Vulkan run passes hangar Command, original split-base travel, F5/F8 map
actions and investment launch/input behavior with no Vulkan validation errors.
Original-scale investment artwork is visually checked. Logs/captures are
`/tmp/allegiance-docked-parity-*`; validation processes exit after their checks.
Platform cross-builds are not repeated for this portable batch. The remaining
queue below is still open.

## Flight roster sorting and list interaction (2026-10-07)

A grouped `TeamPane`, `ListPane` and `TList::InsertSorted` comparison restores
the F6 compact/expanded pane's remaining functional differences:

- Header sorting uses the original case-insensitive name/hull/sector/objective
  values and stable ties. Kills, credits and wings follow the original ascending
  comparisons. Objective sorts legal accepted-command text and its target name,
  rather than the numeric command ID. Passenger sector sorting uses its physical
  sector, while the visible location continues to say `aboard <pilot>`.
- The default `LeaderCompare` in this WinTrek tree compares one leader flag with
  itself and consequently groups humans before drones, retaining member order.
  Native sorting now preserves that actual behavior. It does not add a commander
  reordering that the original comparison never performed.
- A pilot double-click selects only; the original F6 list has no subscribed
  double-click travel handler. Take Me To remains on its authored button.
  Double-right-click does not invent a second context-menu event. Ordinary
  buttons and headers also honor the authored down-action and separate
  double-click policy.
- Blank left/right clicks clear pilot selection and retire its donation, Boot
  and Take Me To actions. Middle clicks do not select pilots. Up/Down/Page
  navigation remains on the original list; Home/End are passed through instead
  of gaining native roster shortcuts.
- Expanded faction tabs select on either button's down event, without capture
  or a second release action. Empty team-list space selects the original
  No Team fallback. Combo drag/release behavior remains separate.
- Forced synchronization of unchanged data preserves pending pointer actions
  and avoids sorting again. A changed dock-duration report also retains header
  and scrollbar capture when there is no pilot selection. Actual loss of the
  selected pilot still cancels its pending actions and context menu.

The comparison policy resides in ClientCore. Text sort keys are projected with
the existing roster refresh and compared without per-comparison allocations;
the native stable sort replaces the original quadratic linked-list algorithm.
MDL layout, artwork and fonts are retained. There are no WinTrek/FedSrv source,
renderer or wire-record changes in this batch.

The final coherent Linux build has source identity
`9ad71247c9bdbe8a750fcc3258b02994a0857fe2eceb9ea0c4c9f6d5ca922d28`.
Player-pane, session, player and control suites pass in 8.07 seconds. The
player-pane suite compares 576 sort cases with the actual original
`TList::InsertSorted`, and exercises actual IGC objective/passenger projections,
blank/middle/double clicks, list keys, team selection, retained capture across
unchanged/changed reports and the authored Take Me To action. The encrypted
remote command-focus check passes. The actual SDL Wayland/Vulkan run passes in
1,335 frames, including all eight headers, blank/double selection, private chat,
collapse/expand, hangar and flight controls, held-key steering and original
Boot capture/cancel/confirm/resume. It has no Vulkan validation errors.
Original-scale compact/expanded captures are visually checked. Logs/captures
are `/tmp/allegiance-f6-parity-*`. Validation processes exit after the checks;
platform cross-builds are not repeated for this portable batch. The remaining
queue below is still open.

## Live investment list and concurrent funding audit (2026-10-07)

Compared `PurchasesPaneImpl`/`PartListItem` in `WinTrek/loadout.cpp`,
`ListPaneOldImpl` in `WinTrek/trekctrls.cpp`, `BaseClient::AddMoneyToBucket`
and FedSrv's `FM_C_BUCKET_DONATE` against the native investment controls.
The full/partial payment rules, predecessor/prerequisite filtering, disabled
technology paths, model cards, hover information, construction-site hints,
whole-card wheel paging and explicit tab/filter resets already follow the
original policies. The staged loadout affordability, teammate-funded purchase,
zero-value Confirm and partial-purchase feedback paths were also compared with
`WinTrek/load.cpp`; no change to those rules was needed in this batch.

Two functional differences are corrected:

- Removing the selected purchase now chooses the preceding surviving card,
  as `ListPaneOld::RemoveItemByIdx` does. Removing its first card clears the
  selection and disables INVEST. Later insertions and repaints retain that
  cleared state; an explicit tab/filter reset selects the first card again.
  The shared ClientCore helper operates on IDs. A membership set is created
  only when a refreshed bucket list actually loses its selected card.
- A concurrent investment that is no longer possible is corrected silently,
  matching the original bucket-refund/status behavior. The classic local
  dispatch no longer invents a purchase-error modal, and an encrypted rejected
  investment no longer invents a failure popup/chat tip. Authority validation,
  account conservation and rejected-action metrics remain intact.

No WinTrek/FedSrv source, renderer, platform or wire-record changes are needed.
The coherent final Linux build has source identity
`40f400c126974e0dfac2ac4692608ee18189acd4b78509b7d2a7189934c995c9`.
The session suite compares 2,304 independent frozen original removal cases,
plus actual original-pane removal/reinsertion/reset behavior. Session and
loadout suites pass in 16.76 seconds. The new encrypted
`native-network-investments` regression passes in 3.43 seconds, exercising
another pilot's funding before the next report, silent correction, unchanged
losing-pilot credits, exact winning-pilot payment and a subsequent valid
investment. Its fixture explicitly accounts for original mission-entry
auto-donation. The existing actual SDL F5 interaction diagnostic passes in
700 frames, covering authored entry/exit, hangar and flight overlays,
launch-under-console and immediate input return. Its captured investment pane
is partway through the original entrance animation, so it is not a complete
static card comparison. Logs are `/tmp/allegiance-investment-parity-*`.
Test processes exit after validation. Platform cross-builds are not repeated
for this portable batch; the remaining queue is still open.

## Shared list input audit (2026-10-07)

Compared `effect/listpane.cpp` with Games, LAN entry, input mapping, lobby,
teleport and postgame controls, including the distinct single/double right-click
subscriptions in `WinTrek/teamscreen.cpp`. Research uses `ListPaneOld` and keeps
its separate investment behavior. This batch corrects the following differences:

- Blank viewport clicks clear selection and its dependent actions. Invisible
  hit regions use the authored viewport; occupied rows retain hit priority.
  Lobby private recipients are cleared without discarding the chat draft.
- Right clicks select Games, LAN, mapping and results rows. Double right clicks
  select lobby pilots/teams without invoking their single-right-click player or
  alliance popup. Actual SDL routing now carries the double-click count.
- Up/PageUp from an empty selection leave it empty; Down starts at the first
  row. Valid endpoints clamp, and wheel navigation reveals rows only when
  necessary. Callers guard empty selection before indexing their data.
  Classic Games and results do not acquire unauthored Home/End shortcuts;
  Games also does not invent Enter-to-join.
- Games removed by refresh or filtering lose their selected endpoint. Its
  later reappearance does not silently restore a retired join target. Sorting
  still preserves selection for an endpoint that remains in the list.

The shared native list helper handles navigation without allocations. Blank
regions add no artwork or renderer work. No WinTrek/FedSrv, IGC, renderer,
platform or wire-record behavior changes are needed.

The final coherent Linux build has source identity
`c0e748442a3eca16fc15361d2466649a1acec83db1b9a458d909544a385e913f`.
The full session suite passes in 5.66 seconds, including 139,425 independent
stepwise navigation comparisons, actual authored blank/occupied hit regions,
selection retirement, popup distinctions, private draft retention and original
key limits. Existing mapping/joystick tests now explicitly establish a first
selection with Down; their wheel check permits an already-visible selection
before requiring scrolling. Player-pane, endgame and control suites also pass
for this batch. The final encrypted browser regression covers real listing,
LAN discovery, first-join ownership, admission refresh, incompatible-data
rejection and stopped-authority expiry.

The actual Wayland/Vulkan postgame diagnostic passes in 250 frames, covering
victory, sorting, scrolling, single/double/blank right clicks, retained text
input, XML Save Stats and return to Games. Its Back assertion now reflects
the existing classic Conquest route. Original-scale initial/scrolled table
captures are visually checked. The only subsequent source edit changes the
mapping test fixture, so this live diagnostic is not repeated. Logs/captures
are `/tmp/allegiance-list-parity-*`. Validation processes exit after the checks;
Windows/macOS builds are not repeated for this portable input batch. The
remaining queue is still open.

## Menu capture and retained highlights audit (2026-10-07)

Compared `engine/menu.cpp`, `engine/popup.cpp`, `effect/combopane.cpp` and
WinTrek's F6/lobby menu subscriptions with the native options, combo, loadout
and player popup hosts. Corrected this related group of functional differences:

- Right and auxiliary button presses outside a menu close its entire cascade,
  as `PopupContainer::PickImage` does. Inside presses leave the menu open and
  do not execute its left-release command. Original read-only Details follows
  the same outside policy; protected confirmations remain captured. SDL routes
  middle/auxiliary buttons through this policy and consumes the dismissal
  press before it can become a flight action.
- A disabled accelerator does not change selection or open another submenu.
  Disabled rows still receive their original hover highlight. Fitting menus
  do not gain wheel navigation. Overflowing loadout menus retain their existing
  scrolling extension so all available parts remain accessible.
- F6 wing/team combo menus retain their hovered or accepted menu row through
  cancellation and reopening. Changing the combo face does not select that
  row in its menu: the initial menu highlight remains the first item, matching
  `ComboPane::SetSelection` and `IMenu::AddMenuItem`. Compact/expanded hosts
  keep independent highlights, reset with the session, in four integers.
- F6 popup borders count as inside for left, right and middle presses. A live
  applicant context retains its selected command across changed reports,
  matching by action and target rather than resetting to the first row.
  Withdrawal still removes the applicant's selection and stale menu.

Menu dismissal restores the filter dialog's draft/edit focus and the lobby's
private recipient/draft without accepting or canceling their parent dialogs.
The session regression exercises those paths, nested Home/flight menus,
inside borders, disabled accelerator/hover behavior, wheel capture and
non-dismissible confirmation. F6 regressions exercise actual authored hit
regions, a nondefault current wing, hovered/accepted/canceled highlights,
all three buttons on borders/outside, and an authority/replica applicant
report. That fixture joins an actual commander, explicitly allows admission
and adds the public membership record at the server's publication boundary.

The final coherent Linux source identity is
`ba4203fdc908fa82530638d7b4e225015d2feb1ed8cf205285c3cb8fe2a7b8ab`.
The final session check passes in 5.70 seconds; endgame and controls checks
also pass. The complete F6 suite passes in 2.91 seconds. Encrypted local
admission checks pass for requests/cancel/accept/reject, Auto Accept, No Team,
Boot bans/locks, readiness, late joins, secure resume and shared public rosters.
The actual Wayland/Vulkan diagnostic passes in 285 frames,
including postgame input plus inside middle presses and outside right/middle
dismissal of real nested menus. Its subsequent source changes are confined
to F6 state and its test fixture, so that unrelated postgame diagnostic is
not repeated. The final `-menus` capture is Games after dismissal, not an
open-menu visual comparison. Logs/captures are `/tmp/allegiance-popup-parity-*`.
No WinTrek/FedSrv, IGC, renderer, platform backend or wire-record behavior is
changed by this batch; platform cross-builds are not repeated. The remaining
queue is still open.

## Docked screen transition audit (2026-10-07)

Compared `WinTrekClient::ChangeStation`, `WinTrek::SetViewMode`,
`LoadoutImpl` destruction and the existing native station/crew recovery
policies. Existing gameplay checks cover allied rearm/rescue, carrier service,
lost-base crew launch, observer relocation, exhausted lives and final-side
elimination. The missing behavior was retirement of the associated UI mode.

- Forced launch, exhausted-life rescue and ongoing-side elimination now
  retire hangar/loadout/teleport/investment views, including their nested
  options, mapping, Help, Details and credits. Pending dialogs and pointer,
  menu and remapping capture are released; retired action receipts cannot
  reopen the former mode. The application handles this in both offline
  simulation and authoritative network reports.
- Loadout drafts are remembered once on retirement, including drafts beneath
  secondary popups. Unchanged snapshots preserve pending actions. Existing
  observers moving between surviving bases keep their read-only popup;
  independent team/main-menu screens and postgame/new-round routing retain
  precedence.
- Asynchronous purchase/transfer/configuration dialogs restore the parent of
  an open main/options menu before replacing it. Previously, clearing that
  menu orphaned its screen and could expose a replacement command menu or
  return to Home. Ghost-state changes also refresh hangar controls without
  requiring a money/economy revision change.

The session suite passes 84 combat-mode retirement combinations and 24
asynchronous-dialog parent combinations. Controls and F6 suites pass. The
full lifecycle suite passes, including actual carrier landings, allied
service, 18 frozen original station evacuations, capture/destruction and
ongoing/final elimination. Encrypted `--case dockedcrews` passes with a full
turret crew, reboarding/disembark, receipt identity and stale-flight failures.
The new Wayland `--dock-view-smoke-test` passes in 130 frames: actual base
destruction releases a nested loadout/options/mapping editor and pending
dialog; exhausted lives recover to command view; a ghost relocates to the
remaining base and closes Details with an actual SDL Enter event. Vulkan
validation reports no errors. Validation instances have exited.

Logs are `/tmp/allegiance-docked-transitions-*`. Client/server/check builds
are coherent at source identity
`918c337dc6159a9fd740a56c7ccca9fd17f40c3b04da1d9a5327e71a4c0dd3ec`.
WinTrek/FedSrv, IGC simulation, renderer and wire behavior are unchanged;
portable UI changes do not trigger another Windows/macOS build. The
remaining queue is still open.

## Find Player audit (2026-10-07)

Compared `FindDialogPopup` and `GameScreen::OnFoundPlayer` in
`WinTrek/gamescreen.cpp`, the installed `gamescreen.mdl` Find dialog,
`LobbyApp`'s case-insensitive exact-name lookup, and WinTrek's wait-cursor
selection. The original Games List Find control now opens the actual
asset-defined dialog rather than remaining disabled.

- The edit receives keyboard focus, retains its name through cancel/reopen,
  supports UTF-8 Backspace and submits on Enter. Its parent game controls
  remain captured. Pending lookup uses the original protected caption;
  success/failure uses the original acknowledgment text. Successful lookup
  resets the game filters and selects/reveals the returned game. Failed
  lookup preserves the prior selection. Late results cannot reopen a
  retired Games screen.
- Browser lookup queries connected player names on known/discovered native
  authorities, including previously discovered endpoints. Matching is exact
  and case-insensitive; paused authorities, dormant reconnect leases and
  unclaimed placeholders are excluded. Searches use the existing bounded
  query concurrency/timeouts, supersede earlier searches and return a
  one-shot result. No pilot or world is allocated for these read-only queries.
- A separate compact pointer-free `PlayerQuery` avoids adding a name field
  to ordinary browser requests. Its response reuses `ServerInfo`; the
  query-only found flag is stripped before exposing ordinary listings.
  Existing connected-peer occupancy traversal also checks the name, without
  an additional roster traversal. Existing WinTrek/FedSrv code is unchanged.
- Pending original message boxes now select the already loaded original
  wait-cursor artwork. This also restores wait-cursor selection during
  connecting, purchasing, transferring and configuring. No cursor asset or
  rendering implementation is replaced.

The session suite passes authored geometry/font, dialog input, retained
focus/name, captions, pending capture, filter reset, game selection and late
response checks. Encrypted `--case browse` passes exact-name/prefix/missing/
empty/overlength cases, paused-server exclusion, replacement searches,
single result consumption, stopped/empty browser retirement and unchanged
pilot occupancy, alongside existing discovery/admission/data-identity checks.
The actual Wayland/Vulkan `--find-player-ui-smoke-test` passes in 930 frames
using a connected `LookupFixture` client, actual SDL clicks/text/Enter/
Backspace, encrypted success/failure, selected game and cancel/reopen.
Dialog, pending, found and not-found captures were visually inspected at
the original 800-by-600 integer scale; Vulkan validation reports no errors.
Temporary validation instances have exited.

Logs/captures are `/tmp/allegiance-player-search-*`. The coherent Linux
client/server/check identity is
`b4e4ad57be3331af68e1184c67281fdca72de783a1c8602a7aa83a9c2b24fc72`.
Public global directory registration and remote multi-game creation remain
unimplemented. Find Player covers browser-known authorities; it does not
provide a global account directory. Portable UI changes do not trigger
another Windows/macOS build.

## Postgame membership and timer audit (2026-10-07)

Compared FedSrv's `RemovePlayerFromSide`, `CFSPlayer::Reset(false)`,
`SaveAsOldPlayer`, `AddPlayerToMission`, and current/departed result assembly.
The native scoring adapter continues to use the original `PlayerScoreObject`.

- Leaving a faction now resets earned score, kills/assists and play/command
  time while retaining deaths/ejections, including direct defections and
  transitions through No Team. No Team players are excluded from the results
  table. Departures from the match retain their disconnected result record.
- Command-role changes retire all old roles before starting new ones. This
  preserves command credit when the same pilot moves from a higher numbered
  side to a lower one in one transaction.
- A retryable transport departure disconnects score timers while preserving
  the existing physical-seat grace period. Authenticated resume reconnects
  them once, without counting the gap or restarting timers on duplicate
  transitions. Frozen reports remain unchanged by later resumes.
- Claiming the initial authority seat discards pre-admission score time and
  reestablishes its command role. The fallback observer `Server seat` is
  excluded from human result records.

The endgame suite passes six direct/No Team/directional transitions with an
actual earned kill, retained life counters and new command credit, plus
resume/retirement timers, late frozen resumes and both placeholder cases.
The existing real IGC scoring, departed-player, XML, shared-result and 400
lifecycle-boundary checks pass. The full session suite also passes.
Encrypted `--case scores` passes public/F6 standings, an actual faction
change, retained lives, identical public postgame records and the original
results table's faction color. Its actual dormant-seat/resume test excludes
2.033 seconds from played time, retains the pilot identity and preserves
frozen results through a later postgame reconnect. The test instances exited.

The coherent Linux client/server/check identity is
`f900bfb9fa7739753ca87703903d4462a237a5bf1eaa99ac9e30c431120149d5`.
Logs are `/tmp/allegiance-score-membership-*`. WinTrek/FedSrv, IGC,
renderer/platform implementations and wire records are unchanged. There is
no added report payload or per-pilot simulation traversal. Cross-platform
builds are not repeated for this portable adapter change. Persistent account
scores and account-backed reentry after the authenticated grace expires
remain separate work; this does not introduce callsign-only identity recovery.

## Core/server identity and crew occupancy audit (2026-10-07)

Compared `CfgGetCoreName`, the original Games List/Create Game/Team Screen
labels and core sorting, and `TeleportPane`/`TeamPane::MannedTurrets`.

- Practice/training and Conquest now share one definition of their actual
  core filename with mission construction. Browser responses report the
  authority's loaded mission core instead of always claiming PCore014.
  Original creation and team screens show the core they actually use.
- Original `[Cores]` labels in `allegiance.txt` are loaded once per asset
  catalog, with case-insensitive keys, original filename fallback and the
  original 24-byte output limit. An artwork-local configuration takes
  precedence over the installed sibling configuration. Games List painting
  and both sorting directions use the displayed label; sorted labels remain
  cached between listing/filter/header changes.
- Authenticated Welcome carries the advertised server name. The connected
  Team Screen uses that identity instead of substituting the network address.
  This adds 32 bytes on join/resume/round change, with no snapshot or simulation
  work. Client/server/check were rebuilt together under the new identity.
- Teleport occupancy and failed-boarding feedback count turret occupants,
  excluding observers. The shared helper walks only the parent's crew;
  WinTrek's existing paths are unchanged. Observers retain ship chat access.
  Fully occupied ships remain listed, and free-but-unequipped slots preserve
  the original distinct caption/sound. Original teleport headers have no sort
  handler; no sorting behavior was added.

Asset configuration checks pass absent/alternate files, BOM/CRLF, section/key
case, whitespace/quotes, duplicate keys, empty names and length limits. The
full session suite passes original configured labels, bidirectional sorting,
creation and local lobby flow. The teleport suite passes observers exceeding
the turret count, free equipped slots, empty/full slots and existing travel
policy. Encrypted `--case browse` passes actual core metadata, advertised name
through both joins and original lobby composition, alongside discovery,
Find Player, admission and stopped-server retirement. All instances exited.
Logs are `/tmp/allegiance-core-crew-parity-*`. The coherent Linux build identity
is `d5ea4654bf163f2fc2d50e1563269e1649d5b8536bb53b494dd327974d175dd0`.

## Original control feedback and community links audit (2026-10-07)

Compared the event sinks in `WinTrek/trekmdl.cpp`, message-box construction,
Help/MML community actions and the main menu's default Web destination.

- MDL compilation now carries original hover, click and selection sound IDs.
  Ordinary buttons, explicit `SoundButtonPane` sounds, bitmap/text header bars
  and combo selections retain their different policies. The original sound
  namespace resolves from IGC's definitions rather than copied numeric IDs.
  Engine-only animated/bitmap buttons remain silent when they have no Trek
  sound sinks; alternate MDLs control the behavior.
- Main, setup, lobby, refit, research, teleport and modal hosts share this
  feedback, as do the independent F6 and Game State consoles. Accepted releases
  and keyboard combo selections emit once. Cancelled releases, disabled or
  captured controls, ordinary right clicks and unsubscribed double clicks do
  not invent feedback. Retained report/layout refreshes do not repeat hover
  sounds. Same-name popup controls override their captured parent's metadata,
  including intentionally silent controls.
- The five installed UI cues are preloaded and played nonspatially. Each host
  uses a bounded queue; metadata binding occurs on layout rebuild, not pointer
  events. Playback starts before potentially expensive frontend actions.
  The existing loadout-slot and investment hover policies remain intact.
- Main-menu Web opens the original forums destination. Help and optional
  asset-defined Game Details Wiki buttons use the original Quick Crash Course
  destination. Installed Game Details has no community buttons; none are added
  to it. Discord and faction voice destinations are preserved.

The full session suite passes ten primitive policies, exported IDs, pointer
and keyboard events, captured Help/MML layering, alternate MML link controls,
retained F6 refresh and Game State dismissal. The MDL script, audio, player-pane
and Game State suites pass. OpenAL checks confirm that all five cues play
without decoding waveforms during interaction. Encrypted browser/discovery,
Find Player and actual original-lobby joining pass with coherent binaries.
The UI-only Wayland run passes startup suggestion/checkbox persistence,
single-button welcome/Escape, lesson selection and return to Home, with Vulkan
validation and original-size artwork. All test instances exited.

Logs and captures are `/tmp/allegiance-ui-feedback-*`; the coherent Linux
build identity is
`54744b7020e72853715baaf68f770c246eae8ad03b9e9668ba4872b8de4acf94`.
WinTrek's execution paths, simulation, renderer and network records are
unchanged by this batch. Portable changes do not repeat platform builds.

## Disabled controls and direct purchase-button events audit (2026-10-07)

Compared `engine/button.cpp`, `PurchasesPaneImpl` in `WinTrek/loadout.cpp`
and the F6 Donate button's normal/right event sources in `teampane.cpp`.

- Original buttons and combo faces remain detectable while disabled. Native
  event metadata now preserves that distinction: disabled controls provide
  authored hover information and intercept clicks without activating or
  allowing an enabled control behind them to receive the click. Normal button
  capture survives disabled presses, and disabled releases retire capture.
  Captured parent controls remain excluded from hover and input.
- Original enabled-enter feedback is observed when a stationary pointer's
  button becomes enabled. Retained layouts are observed once per frame without
  recreating controls, scanning IGC state or repeating unchanged hover sounds.
  Disabled bitmap faces retain their existing original face selection.
- PurchasesPane's directly constructed controls now preserve its explicit tab
  click sound and default positive sound for Invest, Close and both filters.
  This includes the checkbox behavior that differs from the MDL ButtonPane
  factory. Card and Invest information hover retain separate entry/leave
  events. The Invest button's enabled-enter and information-enter sinks remain
  distinct; disabled Invest still displays its selected purchase information.
- This batch initially moved F6 right-button Donate to release. That assumption
  omitted TeamPane's `SetRepeat(.1,.5)` and is corrected by the repeating-controls
  audit below. The original triggers both donation buttons on press, installs
  a timer only for left presses and adds an explicit positive sound in its
  right-event handler.

The full session suite passes direct purchase-button policies, information
hover transitions, disabled capture/activation, stationary enable and retained
refresh, plus a real IGC wallet depletion during F6 Donate capture. The former
right-release test was insufficient and has been replaced. Player-pane,
Game State and MDL script suites pass. The new
UI-only `allegiance-ui-hover-check` compares actual Vulkan pixels: disabled
Launch displays exactly the enabled authored prompt; a captured hangar loses
the prompt and input; an enabled overlapping control cannot receive the
disabled button's hit. Its passing run has no Vulkan validation diagnostics.
Actual SDL training entry passes 240 frames with no missed presents or Vulkan
validation diagnostics. Encrypted discovery, Find Player and original-lobby
joining pass with coherent binaries. All test instances exited.

Logs are `/tmp/allegiance-disabled-hover-*`, with UI-only captures at
`/tmp/allegiance-ui-hover-*.ppm`. The coherent Linux build identity is
`ac488e00ecc135d7b6417269f0d4bb794337275a618fb161b9302a4a17c8a6a9`.
WinTrek, simulation and network records are unchanged by this batch.

## Captured pointer ownership and hover audit (2026-10-07)

Compared `engine/pane.cpp` captured hit testing and mouse enter/leave routing
with the original ordinary buttons, popup menus and authored help branches.

- A held ordinary button owns pointer motion until release. Neighboring
  controls now retain their normal faces and produce no hover sound or help
  transition. The entered button's authored caption remains selected while
  dragging outside; its pressed face still follows whether the pointer is
  inside. Returning inside during capture does not repeat the enter sound.
- Layout refresh restores capture after message boxes and Details layers are
  installed, and binds event metadata to their separately rendered views.
  Modal refreshes preserve the held face and release/cancellation behavior.
  F6 right-button Donate also preserves capture across report refresh despite
  its distinct donation amount.
- Authored slide panes marked `undetectable` no longer drive hover faces or
  help branches while input hit testing excludes them. Popup menu items retain
  their original drag-to-another-item selection and release behavior.
- Disabled captured buttons preserve the original enabled-enter distinction:
  becoming enabled under the pointer emits one cue; enabling outside and then
  returning during capture emits none.

Session, player-pane, Game State and MDL script checks pass, including modal
refresh, menu dragging and captured feedback transitions. The isolated 2D
Vulkan pixel check verifies retained captions, unchanged neighboring faces,
refresh ownership and undetectable overlays with no validation diagnostics.
Actual SDL training entry passes 240 frames with no missed presents and zero
triangles. Encrypted discovery, Find Player and original-lobby joining pass
with coherent binaries. All test instances exited. Ship visibility remains
closed and is outside this audit.

Logs are `/tmp/allegiance-captured-controls-*`; the coherent Linux build
identity is
`546be55104a68ab34d07b1380eb2e8ab914950788def60aef41810a99c5b1189`.
WinTrek, simulation, renderer and network records are unchanged by this batch.

## Repeating controls and shared scrollbars audit (2026-10-07)

Compared `ButtonUIPane`, `PaneButtonFacePane` and `TroughPane` in
`engine/button.cpp` and `engine/controls.cpp`, `CreateTrekScrollPane` and
`ThumbPaneImpl` in `effect/efpane.cpp`, TeamPane's explicit repeat configuration,
and the `TextButtonBarPane` factory in `WinTrek/trekmdl.cpp`.

- Captured F6 controls receive current pointer coordinates. Thumb dragging
  continues beyond the pane, and held arrow/donation repeats pause outside
  their actual bounds and resume on reentry. Feedback capture no longer
  prevents the captured control from receiving motion.
- F6 and the frontend now compile one shared scrollbar implementation. Arrow
  bitmaps use their complete normal and selected images rather than dividing
  a single image into four faces. Thumbs tile and crop the original bitmap,
  with the original eight-pixel minimum. Trough geometry and drag positions use
  the original integer arithmetic. Arrow and trough capture remains silent;
  empty lists retain their original detectable arrows.
- Donate's repeat configuration supersedes the earlier release-only conclusion:
  left and right presses transfer 100 and 500 credits immediately, bounded by
  the wallet. Left holds repeat after 0.5 seconds at 0.1-second intervals. Right
  holds install no timer. Releases only retire capture. Left events use the
  button's sound sink; right events use the handler's explicit positive sound.
  Empty wallets and retired targets cannot produce repeat commands.
- Trough paging repeats after 0.25 seconds; line arrows retain their distinct
  0.5-second initial delay. Text headers use the original white normal and
  yellow/bold pressed or checked faces. Pressing a checked header inverts its
  face; hover and disabled state do not invent PaneButtonFacePane faces.

The full session suite passes 3,240 independent frozen scrollbar-layout cases
and 76,140 integer drag comparisons. The F6 suite passes actual IGC money
conservation for initial and repeated donations, right-single and release
semantics, outside/reentry/disabled timer gates, refreshed capture, full arrow
geometry, thumb dragging outside the viewport and both repeat delays. Game
State and MDL script checks also pass. The isolated Vulkan UI check verifies
authored text-header faces and separate full-image arrow selection, alongside
the existing captured/disabled hover checks, with no validation diagnostics.
The installed normal/selected arrow pixels are identical; a distinguishable
resident-image fixture additionally proves down-image selection. Captures are
visually inspected at original scale. No game scene or ship-render check runs.

The encrypted console regression passes group and explicit-wing delivery,
commander donation toggles and authoritative waypoint routes. Client, server
and checks are built coherently with identity
`fda412950d175a95fe6742168933dd77f16b67618008424c274a51e7b89528b1`.
Logs are `/tmp/allegiance-repeated-controls-*`; all instances exited. Changes
are confined to portable native UI adapters and regression checks; WinTrek,
simulation, network records and platform backends are unchanged. Platform
cross-builds are not repeated.

## Cross-panel mouse capture audit (2026-10-07)

Compared `GroupImage::HitTest/Button/RemoveImage`, captured `Pane::Button`,
`ButtonUIPane`, `TroughPane`, and the separate `SectorInfoPane` and
`SectorMapPane` handlers. This closes a functional gap in the top-level native
event dispatch, beyond the previously checked capture within each pane.

- One shared capture dispatcher now runs before hit testing other console
  panels and newly opened popups. Motion, wheel suppression and button edges
  remain with the held frontend, F6, Game State or map control. Releasing over
  another layer cannot leave a donation timer or thumb drag active, dismiss that
  layer, or leak a flight action. Stationary hover feedback also stays with the
  owner. Unavailable views, movies, slides, session retirement and focus loss
  retire their capture; focus cancellation does not synthesize an outside
  popup press.
- Primary and secondary edges retain the original distinct policies. Ordinary
  buttons release capture on either left or right release; map and trough
  drags release only on the left edge. A secondary press is delivered to the
  held control, including mixed-button Donate and its existing timer, rather
  than selecting a different panel. Disabled Donate presses create no timer.
  A released Game State close cannot retain a stale release target.
- Map drags retain motion and outside release even when another UI layer makes
  the map inactive for ordinary hit testing. Their original right-button
  sector-chat action still works. The separate map expansion image is
  release-triggered without capture or a required matching press, as in
  `SectorInfoPane`; the previous native press requirement is removed.

The complete session, F6, Game State and MDL suites pass. Real IGC money
transfers check cross-panel/modal releases, mixed buttons, timer behavior and
focus cancellation using the production dispatcher. The isolated 2D Vulkan
check passes actual map expansion/drag/secondary/release routing and the
existing control pixel comparisons with no Vulkan validation diagnostics.
The real SDL training entry passes suggestion/checkbox, persistence, welcome,
lesson selection and Home return in 240 frames at 165 Hz (one missed present,
zero triangles). The encrypted console regression passes with coherent client,
server and check binaries. All instances exited; no game scene or ship-render
check was run for this audit.

The coherent build identity is
`e9b475c9865eb858e62050c33ddd79752c0ff135cacabd8898fa00f96f73caa4`.
Logs and UI captures are `/tmp/allegiance-cross-layer-capture-*`. Changes are
confined to native UI adapters, event dispatch and regression checks. WinTrek,
simulation, network records and platform backends are unchanged, and portable
changes do not trigger repeated Windows/macOS validation.

## Classic keyboard and foreground text audit (2026-10-07)

Compared `IKeyboardInput`'s default handlers, `ButtonUIPane`, `ListPane`,
`EditPane`, TeamScreen/GameOverScreen focus setup, the process keyboard-filter
order in `zlib/window.cpp`, and `OnTrekKeyFilter(TK_MainMenu)`.

- Classic screens now stop after their own authored keyboard handlers rather
  than falling through to the experimental Tab/arrows focus ring and
  Enter/Space button activation. This policy follows the compiled original
  view, covering the screen family without more per-screen exceptions. List,
  mapping, edit, menu and confirmation handlers retain their explicit keys;
  the optional native interface retains its keyboard navigation.
- One mapped main-menu key opens the original menu above the lobby/results
  chat EditPane. Previously the native `wantsText` guard required the first
  Escape to deactivate chat, exposing the incorrect generic button fallback.
  Popups, mapping capture and flight chat composition keep their own input.
  Closing the menu preserves the draft, recipient and edit focus.
- Retained background chat state no longer accepts keyboard input while Help
  or Credits owns the foreground. The real SDL check reproduced Enter in Help
  sending and clearing the lobby draft; the text handler now also requires
  current text eligibility. Merely hiding the caret/SDL text input was
  insufficient. Regression checks cover both foreground screens and their
  return to the private draft.

The full session suite passes 55 screen/key cases across Home, training,
hangar, lobby/pilots, results, Help, Credits, games, teleport and research,
plus six popup/mapping capture and private-recipient restoration paths.
Existing list/edit/confirmation/mapping checks and F6, Game State and MDL
suites pass. The new `--classic-keyboard-smoke-test` verifies actual SDL
single-key lobby/results menu opening, draft/focus restoration, Help, Credits
and original Home/training button-key behavior without starting a game. Its
220-frame Wayland/Vulkan run passes with no validation diagnostics, no missed
presents at 165 Hz and zero triangles. The installed Help asset includes its
authored Close/Wiki/Discord buttons, which remain mouse actions.

Encrypted lobby checks pass configuration, team changes, readiness, countdown,
reconnect, victory and next round with coherently rebuilt binaries. All test
instances exited. Logs are `/tmp/allegiance-classic-keyboard-*`; final passing
checks use the `-verified` suffix. Build identity is
`a4a326b54a40cc010d85772de2bd6e2616086b952d2c45786e196b508825d985`.
Changes are confined to native UI/event adapters and regression checks;
WinTrek, simulation, wire records and platform backends are unchanged.
Windows/macOS builds are not repeated for this portable batch.

## Hangar console action audit (2026-10-07)

Compared all eight `HangarScreen` button sinks, `SetViewMode` and the mapped
F4/F5/F6/F8, mini-team and Game State handlers. The authored Team button already
opens the expanded F6 pane rather than a replacement team screen. The remaining
functional difference was dismissal of competing console panes through mouse
actions.

- Authored Investment, Teleport, Loadout and Command actions now carry the
  original competing-pane dismissal into the main dispatcher. Team dismisses
  Game State while preserving its compact/expanded toggle behavior. Help, Quit
  and a pending Launch retain their resident panes. Blocked controls perform no
  dismissal. The optional native interface retains its existing behavior.
- Mouse actions and mapped console keys use one dismissal adapter. Closing a
  player pane also retires its held donation timer and pointer capture. Training
  updates its underlying overlay mask, so the next script/UI synchronization
  cannot reopen a dismissed Game State pane.

The real-IGC pane regression passes 104 offline/network, hidden/compact/expanded
roster, Game State and modal button cases, plus a held donation retirement.
An actual authored training mission checks that Help retains Game State and
Team clears both its pane and script flag. The complete session, existing F6,
Game State and MDL suites pass. The headless training/pane run uses
`ALSOFT_DRIVERS=null`; a restricted PulseAudio connection hung the first audio
teardown, so that owned test was interrupted and rerun successfully.

The coherently rebuilt encrypted lobby regression passes configuration,
membership, readiness, countdown, reconnect, victory and next round. The
UI-only SDL/Wayland/Vulkan regression passes 220 frames at 165 Hz with no
validation diagnostics, no missed presents and zero triangles. All instances
exited. Build identity is
`1d9eea6f5b6d16f3f4b346275c2f4f7f1de38c69835514c482a3030598a4fe54`.
Logs are `/tmp/allegiance-hangar-console-*`; final passing logs use `-complete`
for SDL/network/build, `-null` for panes, and `-final` for session/Game State/MDL.
Changes are confined to native UI/event adapters and checks. WinTrek,
simulation, wire records and platform backends are unchanged; Windows/macOS
builds are not repeated for this portable batch.

## Combat view and input lifetime audit (2026-10-07)

Compared `screen`, `SetViewMode`, `OverrideCamera`, `EndOverrideCamera`,
`ConsoleData::OnSwitchViewMode`, `TrekInput::ClearButtonStates` and the
quick-chat menu close sink. This extends the hangar-button audit to changes
caused by gameplay and authoritative reports.

- One view observer covers hangar, loadout, flight, command, override and
  independent interface screens. Mapping, options, Help, credits and Details
  resolve their underlying view instead of becoming artificial mode changes.
  The existing draft/modal retirement now also handles docking, command-view
  changes and override completion. Independent lobby/results screens retain
  their precedence.
- Old player/Game State panes no longer survive ordinary view changes. Override
  entry retains the old pane state while the backdrop changes; completion
  clears the destination's banishable panes, matching the original sequence.
  Initial setup preserves authored training flags, and a repeated observation
  cannot close a new F6 pane opened later in the same SDL event batch.
- All pointer owners retire on a view replacement, including map capture.
  Command selection/order drags and their cached picking projection retire
  too. Held/pulsed mouse actions cannot activate the new view until released.
  Physical keyboard Boolean input remains held: the original
  `ClearButtonStates` deliberately does not clear those keyboard inputs.
- Mouse-command composition ends on an ordinary switch. Quick-chat menus
  close through their original quit policy; wing popups close while typed chat,
  command and shell drafts retain their recipient and text. These operations
  run before subsequent SDL events and after simulation changes. Unchanged
  frames incur no additional mouse-state query or layout rebuild from this
  adapter.

The new headless real-IGC check passes 294 view/composition cases and 30 popup
parents, including capture, drags, keyboard/mouse differences, popup retirement
and same-batch controls. It follows an actual purchase/launch, override
completion, station docking and both command-view directions without a graphics
window or renderer. The existing 104 hangar cases and complete F6, session,
Game State and MDL suites pass, including all 84 prior forced-mode
retirement/draft/receipt cases. Headless audio uses `ALSOFT_DRIVERS=null`.

The final UI-only SDL/Wayland/Vulkan run passes 220 frames at 165 Hz with zero
triangles, no missed presents and no validation diagnostics. Encrypted console
checks pass group/explicit-wing commands, commander donation toggles and
authoritative waypoint routes. All instances exited. The coherent build
identity is
`616b0421310981b193932d96b7d8795042c1f58dca431c1eb3c16c989151013e`.
Logs are `/tmp/allegiance-view-transitions-*`, with `-final` for the final
build/SDL/network runs. Changes are confined to native UI/input adapters and
checks; WinTrek, simulation, wire records and platform backends are unchanged.
Windows/macOS builds are not repeated for this portable batch.

## Public roster and remote account audit (2026-10-07)

Compared the original `TeamPane::OnSelectPlayer`, compact/expanded player
painters and `PlayerInfo` money, donation and passenger-name sources.

- F6 now retains received credits and automatic donation routes for teammates
  whose physical ship is absent from the replica. These values already exist
  in the faction report. Donation targets use a reusable 16-bit ID array,
  allocated once per replica, and reset only the previously populated slots
  when a full private report replaces them. No wire records or payload grow.
- Public roster membership supplies live alliance eligibility even without
  ship/status observations. A changed alliance immediately changes the
  original Donate button's enabled state; commander routing remains restricted
  to teammates. This no longer depends on a remembered ship's alliance flag.
- Observer/gunner location text resolves a known parent's current public
  name without requiring that parent's physical ship or historical status.
  Missing public parents retain the original sector-name fallback. Resolution
  reuses the roster's ID index and adds one linear projection pass.
- Compact kill/death counters retain WinTrek's full brightness for one's own
  faction even if the ship-status record is stale. Docked names remain bold
  red; observers and gunners aboard a docked ship retain their own state and
  ordinary name style.

The new headless matrix covers 90 combinations: own/allied/hostile membership,
all five reported states, current/stale information and absent/present/renamed
public parents. Each checks both original F6 layouts and actual pointer-selected
Donate/AutoDonate gates, with no physical parent/passenger replica. It also
checks known account/wing/donation data, foreign-account privacy and subsequent
metadata retirement. The encrypted wing/roster regression now checks received
remote credits and donation routes, plus retirement, alongside faction changes
and authenticated resume. Both pass. Existing player-pane, session, teleport,
Game State and MDL suites pass. No rendering/ship-visibility tests were reopened.

The coherent Linux client/server/check identity is
`60e453b5d217675e35c27261eb24e18cc43b547c8953546f9d6030558099ea1d`.
Logs are `/tmp/allegiance-roster-state-*`, with `-verified` for the final
build/player/network checks. Changes are confined to native roster/report
adapters and tests. WinTrek, IGC, wire layouts and renderer/platform code are
unchanged; Windows/macOS builds are not repeated for this portable batch.
Full recovery combinations and persistent account services remain in the queue.

## Flight feedback and ripcord controls audit (2026-10-07)

Compared `WinTrek::TK_Ripcord`, `BaseClient::FM_S_PLAYER_RESCUED`, the ripcord
message handlers, docking/relaunch/deployment handlers and FedSrv's rescue and
ripcord callbacks. Ship visibility remains closed; this batch does not alter
the renderer or repeat the ship-visibility checks.

- The ripcord key now chooses the current order target, then the accepted
  order target only when the current target is absent, then the current sector.
  Physical targets require the original visibility predicate; remote ship
  targets use current received ship-status sectors. Explicit sector requests
  can retarget an active ripcord rather than cancelling it. Original parent,
  flight, flag and hull restrictions remain in force.
- Ripcord activation uses the original critical console tip, including the
  nearest-sector fallback caption. Same-target requests leave the countdown
  and feedback alone. Initial activation reaches the source ship's crew;
  active retarget feedback reaches only the requesting pilot. Cancellation,
  invalidated destinations, carrier energy failure, forbidden ships and denied
  requests use their original cues. Successful arrival retains the jump cue
  without an added completion message.
- Both rescue participants receive the original wording, priorities and cues:
  the rescuer's noncritical acknowledgement and ship-attached rescue sound,
  and the rescued pilot's critical acknowledgement and unpositioned jump
  sound. Rescue credit and participant feedback precede the no-restart check,
  matching FedSrv. Bystanders receive neither participant's private cues.
- Removed native-only docking, carrier-rearm, ripcord-completion and expendable
  deployment console messages. Their actual service, location, creation,
  inventory, cinematic and original sound actions are retained.
- Private notification sounds now retain their original source-model emitter
  on replicas. A spare sound-event flag distinguishes that attachment from
  unpositioned private cues; no wire record fields or payload sizes grow.
  Restored cue IDs are included in the native sound preload list.

The new headless `allegiance-flight-feedback-check` exercises actual IGC rescue,
lost-base rescue ordering, ripcord intent, activation, idempotence, cancellation,
retargeting, nearest-sector fallback, arrival, restrictions, denial, source-crew
routing, destination invalidation and chaff creation/quantity/sound. Participant
and crew checks serialize and decode complete authority reports before replica
delivery, including emitter attachment and private audience isolation. No
graphics window or scene/ship-visibility assertions are used.

The new check and existing F6, teleport, carrier/service and MDL suites pass,
including 705 frozen carrier/allied-service comparisons. Encrypted GNS
allied-station rearm and console/waypoint regressions also pass. The Linux
client, server, network check and affected functional checks build together;
all test instances exited. The coherent build identity is
`f2b20c218fd3e0e9bca84752da45d36e8df23154873701ed5425b7d77c6e5883`.
Logs are `/tmp/allegiance-flight-feedback-*`, with `-build-final` for the final
coherent build. Production changes are confined to a small ClientCore feedback
policy and native gameplay/report adapters. WinTrek, FedSrv, IGC and platform
backends are unchanged. Windows/macOS builds are not repeated for this portable
batch. Further recovery combinations and persistent account services remain
in the queue.

## Combat, objective and service feedback audit (2026-10-07)

Compared `BaseClient::FM_S_KILL_SHIP`, `FM_S_GAIN_FLAG` and
`FM_S_STATION_DESTROYED`, FedSrv's kill, flag-delivery, technology-recovery and
side-elimination callbacks, the IGC building-completion callback and the
original ammunition-resupply chat. The extracted `ClientCore/CombatFeedback.h`
contains the original presentation policies; native callbacks retain the
actual IGC destruction, recovery, objective and production actions.

- Kill/ejection tips now distinguish victim, attacker and other pilots with
  the original wording, colors and priorities. Important drones retain their
  faction-qualified captions and own-faction destruction sounds, including
  miner, carrier, constructor, minefield-layer and tower-layer cues. The
  original nonzero-damage notification gate and permanent-drone-death versus
  pilot-ejection rules remain intact. Removed added ejection/restart guidance.
- Station loss retains the attacker/unknown-source caption, sector name,
  owner colors, observed-station audience and original own/allied/enemy cues.
  The faction-elimination announcement uses the original HQ Everyone chat.
- Flag/artifact pickup retains original critical tips, recipient factions,
  colors and sounds, including the original side-zero flag-warning gate.
  Delivery uses the original Team chat and voice: collector and flag-owner
  factions for flags, collector only for artifacts. Counts, respawn and victory
  continue through the original objective code.
- Docking names each genuinely new recovered part before applying the combined
  technology unlock and records the original technology-recovery score credit.
  Already unlocked parts produce no repeated recovery tip. Removed the added
  station-completion message; the existing IGC callback supplies the original
  completion Team chat and voice. Ammunition resupply now uses its original
  drone-authored Team chat.
- Common tips are formatted once per distinct caption/audience and captured
  and encoded once per faction category, rather than once per recipient.
  Private victim/attacker overrides remain private. A spare notice flag marks
  shared tips; the existing sender/voice fields hold excluded actor IDs for
  this record kind, so wire records do not grow. Shared announcement cues and
  tips travel together, independently of faction-map refresh cadence.
  Unchanged feedback captures survive publication cycles. Sorted composition,
  acknowledgement and replay suppression preserve private/common delivery;
  fresh subscribers and faction changes skip historical shared feedback.
- Increased the report dependency bound to include feedback alongside all
  eight projectile partitions, faction state, viewed and physical sectors,
  public roster and retained results. Both synthetic composition and an actual
  encrypted remote-command/lobby run cover this 14-arena combination.

The new scene-free `allegiance-combat-feedback-check` passes 504 frozen original
kill/ejection comparisons and 19 objective audience/caption/cue cases. Actual
IGC callbacks and serialized authority/replica reports cover pod recovery,
drone privacy, exact tips/cues, sharing across recipients and publication
cycles, acknowledgement/replay, new-audience history suppression, flag delivery,
named technology unlocks and station-loss audience isolation. The arena check
also verifies one encoding for 1024 recipients of one shared feedback category
and rejects mixed shared-tip/private-chat records. This is a synthetic sharing
check, not a new large-match performance measurement.

Combat, arena, flight-feedback, goals and endgame checks pass on the final build.
Encrypted GNS combat feedback, packet-loss replay, lobby/maximum-arena and
mission-lifecycle checks pass. F6, teleport, carrier/service, MDL and audio
regressions also pass during this batch, including 705 frozen carrier cases.
Updated stale goals fixtures to wait for the already implemented deferred pod
refit and allow the already restored postgame voice-only quick chat; command
mutation gates remain checked. Updated station-loss fixtures to the original
unknown-source caption. All build and test instances exited.

The coherent Linux client/server/check identity is
`987f5953f51eed8ec6a63b2ba6edd08f562e36a67fcc1a7148bfd780bde99c9c`.
Logs are `/tmp/allegiance-combat-feedback-*`; `-edge-complete` identifies final
functional/encrypted runs and `-edge-build-complete` the coherent build. Both
new edge regressions failed before their fixes and pass afterward. This batch
changes native feedback/report adapters, the extracted ClientCore policy and
checks; WinTrek, FedSrv, IGC, renderer and platform backends are unchanged.
Windows/macOS builds are not repeated for portable changes. Ship visibility
remains closed; no scene or ship-visibility checks were reopened. The remaining
functional, visual and account-service work below is still outstanding.

## Recovery, crew and evacuation audit (2026-10-07)

Compared FedSrv's fatal-pilot recovery, allied-pod docking, station-transfer
and base-evacuation flows with WinTrek's station-change and death-override
completion handlers. This batch fixes native recovery state and feedback;
ship visibility remains closed.

- Exhausted network pilots now automatically enter the original command seat.
  Existing and cold replicas select the death sector when available, with the
  restart sector as fallback. A single view intent synchronizes the authority;
  there is no additional periodic record or larger payload.
- Clearing the exhausted pilot's physical sector no longer clears its command
  observation. Death recovery, lost-base evacuation and station transfer keep
  the viewed sector separate from restart-base membership, as FedSrv and the
  original TELEPORT_ACK handler do.
- Docked replicas complete the local death override on the presentation clock.
  They continue sending receipts without manufacturing flight inputs or
  advancing docked IGC simulation. The original personal-jump cue plays once
  when death view returns to the hangar/command seat; silent prediction replay
  defers the cue until an actual prediction/display frame.

The new scene-free `allegiance-recovery-check` passes 38 cases: four exhausted
pilot/command and local/remote-sector combinations; six offline/authority
allied-pod returns with retained, destroyed or captured last bases; sixteen
captain/two-gunner/observer loss reports for ejection, fatal death, disabled pods
and exhausted lives; and twelve capture/destruction evacuation reports.
They check preserved identities, original life/ejection counters, independent
crew recovery, live crew/equipment preservation, exhausted-observer relocation,
last-own-base fallback, remembered fitting and wallet conservation. Display-only
completion also explicitly preserves the IGC clock and reported simulation time.

The encrypted `--case recovery` check passes actual automatic command entry,
death-sector retention, override/cue completion, ghost mutation gates, base-loss
evacuation, canonical transfer acknowledgement and authenticated resume without
replayed cues. The new remote-sector transfer and docked override checks failed
before the corresponding fixes and pass afterward. Existing encrypted allied
service, docked-crew and explosion-lifetime checks pass, as do teleport, loadout,
flight/combat feedback and F6/player-pane regressions. No window, renderer or
ship-visibility checks were reopened.

The Linux client, server and checks were rebuilt with coherent identity
`40a269491cce531508407c319bc943259e7c98d7f3a46dff28ff9538e87704ef`.
Logs are `/tmp/allegiance-recovery-*`; `-final` identifies final recovery and
encrypted runs and the coherent build. All test/build processes exited.
WinTrek, FedSrv, IGC and platform backends are unchanged in this batch;
Windows/macOS builds are not repeated for portable adapter changes.
These checks cover the stated recovery matrix, not every hull/faction visual
combination or account/moderator service listed below.

## Active commands and observed allied credits audit (2026-10-07)

Compared FedSrv's active chat-command branch and donation audiences, the zero
money in `QueuePlayerInfo`, BaseClient's `FM_S_MONEY_CHANGE` and own-side account
bootstrap, and both original F6 money painters. The compact painter includes
allied humans; the expanded painter shows credits only for one's own faction.

- Compact F6 now shows the received allied credit estimate, including negative
  values. Foreign accounts start at zero, as original PlayerInfo records do.
  Only donations involving the viewer's faction change that estimate. Foreign
  paydays, purchases and same-faction transfers remain undisclosed; unrelated
  allied factions receive no donation information. Own credits remain the
  authoritative balance and enemy account captions remain absent.
- Shared faction pilot records carry cumulative observed transfers. A client
  subtracts its initial baseline, so skipped reports remain correct without
  replaying an event history, while a later subscriber starts with zero.
  Baselines survive authenticated reconnect and alliance changes. Records
  retain their 48-byte layout; the previously unused flags slot now carries an
  account generation. No recipient-specific report or periodic message is added.
- Account generations survive hull/crew replacement but change when an account
  is retired/reset. They reset both the foreign account's baseline and a reset
  viewer's observations. This handles leaving and rejoining the same faction
  between publications, even though the physical pilot and callsign survive.
  Unchanged faction peers retain their own received history. The new regression
  reproduced the stale-baseline bug before the generation fix and passes now.
- The active `#autoaccept`, `#autobalance`, `#list`, owner `#ban` and ballot
  handlers already have corresponding native behavior. Steam `#who`/`#kick`/
  moderator `#ban` chat branches are commented out in this original tree;
  they are not missing active commands. The actual Steam moderator context-menu
  actions still need authenticated account support. Corrected a stale console
  fixture to require the exact mutiny tally before its leadership notification,
  following `MutinyBallot::OnPassed`, rather than requiring it to remain the
  last message after the already restored leader feedback.

The new scene-free allied-credit check passes actual serialized IGC donations,
signed donor/recipient audiences, a third ally and hostile observer, omitted and
duplicate reports, distinct subscriber baselines, reverse/own-team transfers,
alliance memory, hidden account resets and generation changes. Independent
lifetime cases cover public departure, viewer/owner changes and 32-bit counter
wrap. The synthetic arena case verifies one encoding for 1024 subscribers;
this is not a new large-match performance measurement. The existing 90-case
roster matrix now verifies compact allied credits and the expanded own-only gate
across current/stale status and missing/renamed physical parents.

Six headless checks pass: allied credits, players/economy, F6/player pane,
console/ballots, arenas and recovery. Five encrypted GNS cases pass: credits,
ballots, mission chat, money/order requests and remote wing/account metadata.
The credit case covers four factions, late admission, reverse transfer,
authenticated resume and same-batch leave/rejoin. Linux client/server/checks
were rebuilt with coherent identity
`0deb6866d4e5698de7df770307567cda09075f5355a124f59ece4c5e46c89160`.
Logs are `/tmp/allegiance-allied-credits-*`; `-final` identifies the final runs.
All processes exited. WinTrek, FedSrv, IGC, renderer and platform backends are
unchanged; no ship-visibility checks or portable Windows/macOS rebuilds were
reopened. Full visual and authenticated account-service work remains below.

## Remote Create Game and multi-mission hosting audit (2026-10-07)

Original references: `WinTrek/CreateGameDialog.h` server/core painters,
selection compatibility and `RefreshOkButton`; `clintlib/clintlib.cpp`
`CreateMissionReq`; lobby mission-creation/capacity handlers. The original
client asks a selected authority to create a named mission, then joins as its
owner. The previous native dialog had only an offline creation target.

- The original authored dialog now lists real compatible hosting authorities
  alongside its explicit Local game row. It uses the asset's server/core hosts,
  columns and scrollbars, fourteen-pixel rows, blue selection, full-server red
  counts, unsupported-core dimming and the original ping color thresholds.
  Server/core incompatibility clears the other selection; removed authorities
  retire selection. OK is hidden unless both selections are usable and the
  selected authority has capacity. Default names remove the appended rank using
  the original callsign-prefix rule. Existing local creation remains available.
- The existing encrypted browse socket returns an authority directory with
  live mission names, occupancy, rules and join gates. Find Player searches all
  hosted missions. Child endpoints retain the queried authority's host address;
  a directory or creation response cannot redirect the client to another host.
  Browsing still allocates no pilot or world baseline.
- `ServerHost` keeps the entry listener and original mission stable, and creates
  separate IGC authorities, GNS listeners and publication domains for additional
  missions. The server shares its existing asset/model cache and advances all
  occupied missions at the established cadence. No flight, rendering, input or
  report-encoding architecture is replaced. Service admission pause applies to
  created missions too; existing combat continues.
- Creation reserves ownership for the requesting callsign and a random token.
  Another pilot cannot claim that game before its creator arrives. Once claimed,
  ordinary guests join normally and reconnect uses the existing session token.
  Build/content gates, exact name limits, supported-core checks and actual
  listener failures precede publication. Busy child ports are skipped.
- Request identities make a repeated creation return the same reservation;
  changing a request under the same identity is rejected. Cached outcomes are
  bounded and expire. Unclaimed missions retire after thirty seconds; used
  missions retire when all pilots and reconnect-grace seats depart. Replaying an
  expired reservation does not return a recycled listener or create another game.
- Direct records have explicit initialized layouts and no implicit padding;
  all five new record types pass the existing poisoned-storage check. Creation
  and directory records are cold control traffic, not periodic pilot messages.

The encrypted, scene-free `--case create` check covers the authored dialog gates,
rank suffix, local fallback, a maximum-length game name, a busy listener,
creator/guest admission, simultaneous mission ownership, unsupported cores,
service pause, mismatched builds/data, malformed requests, empty names, request
replay, cross-mission player search, joined/unclaimed retirement and expired
reservation refusal. Synthetic port plans cover the discovery-port conflict
and the 65535 boundary without allocating extra missions. Existing browse,
lobby and admission cases also pass, as does bounded actual server startup. LAN
fixtures now wait for and expire their own authority, allowing unrelated servers
and concurrent validations to remain live. All validation processes exited.

Linux client, server and network check use coherent identity
`ac4f13e2e13215e69fee6abc1c88d68fb8cfd87a38c5ef88aa449fcf823d8e3f`.
Logs are `/tmp/allegiance-create-*-final.log`. WinTrek, FedSrv, IGC, renderer
and platform backends are unchanged. Portable Windows/macOS builds were not
repeated. Ship-visibility work remains closed.

The hosting capacity is sixteen missions including the entry mission, bounded
by the remaining port range. With default entry port 27500, additional missions
use available UDP ports 27502–27516, reserving 27501 for LAN discovery;
deployments must allow those game ports
as well as the entry port. Remote creation currently offers the supported
`static_core` conquest core. Public global directory registration, authenticated
persistent accounts/ranks/squads, broader core catalogs and complete visual/state
comparison remain in the queue. Known endpoints and automatic LAN discovery
work without the legacy Club service.

## Selected-core catalog and original core presentation audit (2026-10-07)

Original references: `FedSrv/FedSrv.CPP::LoadStaticCoreInfo`,
`WinTrek/CreateGameDialog.h` core rows, `WinTrek/trekigc.cpp` core labels and
official-core flags, and the original Game Details core/server fields.
The native creation path previously selected `static_core` regardless of the
installed catalog, and its lobby, civilization list and saved equipment banks
assumed the scenario's default core.

- `ClientCore/CoreCatalog.h` reads the original count-and-basename `cores.txt`
  format, bounded by the original thirty-two-core mask. It rejects malformed,
  duplicate, unsafe and excess entries. Asset lookup verifies that every listed
  core is installed; unrelated map and historical `.igc` files are not advertised.
  The current Steam catalog contains `ac_19`, `ac_20`, `PCore016`, `wc_04`,
  `rc_04` and `zone_core`. Local creation additionally retains `static_core`.
- The original dialog now paints and selects each supported core, preserving
  catalog order and server/core compatibility gates. Remote requests resolve
  filenames case-insensitively to the authority's canonical entry. Mission,
  directory, replica and reconnect records carry that actual core. A live
  mission cannot change static cores through its ordinary settings dialog.
- IGC loads the selected core before exposing civilizations, starter hulls,
  available parts or prosperity constants. Lobby names and faction controls
  follow it; equipment banks use `loadouts_<actual-core>.dat`. Live frontend
  population reads the already loaded IGC catalog rather than reloading it.
  Authored training and practice retain their required default cores.
- Optional `allegiance.txt` core names and official flags follow original
  case-insensitive, first-key and literal-`true` rules. The Create Game rows use
  the original `iconscorescountbmp` badge, including selected-row painter order.
  Game Details uses the friendly name, original green official-core value and
  the queried server name rather than its numeric endpoint.
- A regression exposed owner reassignment while the owner's seat was still
  in authenticated reconnect grace. The authority now retains that owner until
  the existing grace/retirement or explicit role-transfer policy releases it.
  Reconnecting to a created mission restores owner controls in its bootstrap.

The scene-free catalog check passes eight real cores (six advertised plus both
defaults), forty-eight faction selections, ninety-four actual hangar definitions,
core-specific equipment banks, labels, badges, selection and refusal cases.
Each advertised core also passes byte-for-byte Windows-layout static export
roundtrip, definition identity and ammo/fuel pack checks. The expanded asset audit
passes 412 models, 441 images, 1,101 meshes and 534,021 triangles with zero failures.
Existing saved-loadout restoration and optional core-configuration checks pass.

Five encrypted GNS cases pass: the new six-core directory/creation/reset/resume
case and existing remote creation, browsing/player search, lobby and admission.
They verify six simultaneous created authorities and replicas, canonical core
identity, same-core reset, cross-core refusal and immediate resumed ownership.
Linux client/server/checks use coherent identity
`afcdedef698d2ec2b064e4f571de22a235c40c21e916683463518a3379ef72a3`.
Logs are `/tmp/allegiance-cores-*`; `-final` identifies the final functional runs.
All validation processes exited. This batch changes no WinTrek, FedSrv, IGC,
renderer or platform backend. No ship-visibility checks or Windows/macOS builds
were repeated. Broader visual/state comparison and authenticated account/global
directory work remain in the queue.

## Original creation and neighboring dialog input audit (2026-10-07)

Original references: `WinTrek/CreateGameDialog.h` construction, `SetContainer`,
`OnChar` and compatibility handlers; `effect/listpane.cpp` selection/button/wheel
events; `WinTrek/gamescreen.cpp` Filter/Find handlers and official-game painter;
`WinTrek/trekigc.cpp::CfgIsOfficialServer`; Logon, Team Settings and MML popup
keyboard handlers. This comparison found several differences in the native
Create Game dialog, despite its working mission creation path.

- Fresh original server/core lists have no selected item. Native original-mode
  creation now starts the same way, hides OK until a compatible pair is selected
  and resets both list scroll positions on reopening. The optional native
  interface retains its prior default selections.
- Create Game forwards Return to its EditPane without subscribing to the edit's
  string event; it does not create a mission. Native now preserves that behavior.
  Filter, Find Player and Team Settings retain their separate explicit Return
  handlers. Existing diagnostic workflows now use the original creation button.
- Creation lists select immediately on either left/right down, including double
  clicks, without capture or repeated release actions. Blank list space clears
  only that list's selection. Wheel input follows the original next/previous
  item policy, keeps the name field's focus and reaches overflowing rows.
  Unsupported selections clear the opposite list for local targets as well as
  remote authorities; a remote-only core cannot remain paired with Local game.
- `[OfficialServers]` reads the original name/address mapping with its
  case-insensitive lookup/comparison, first-key precedence and 24-byte output
  limit. Create Game paints the original server badge after selection fill.
  Games List paints its official-game marker only for an official server and
  core, before the original development/lives overlays. Native endpoint ports
  are excluded from the original host-address comparison; IPv4 and IPv6 work.
  Score-count and object-model filters retain their separate original meanings.

The scene-free `allegiance-original-dialogs-check` passes four button/double-click
combinations, real blank hit regions, overflowing server/core lists, fresh
reopening and adjacent Return handlers. It also runs the existing actual local
mission/lobby workflow, training-entry dialog matrix, 55 screen/key cases,
139,425 frozen list-navigation cases, 3,240 scrollbar geometry comparisons and
76,140 drag positions. Configured official markers and selected-row order pass
with independently overridden original configuration. Asset/core checks and
MDL compilation pass. Three encrypted GNS cases pass: remote creation, six-core
creation/reset/resume, and browsing/player search/LAN.

Linux client/server/network checks use coherent identity
`b88d28e5945944c54ca506eb2bee75c1eeb5e969bd9a49e0800ab4ba23327a39`.
Logs are `/tmp/allegiance-dialogs-*`. All validation processes exited. This batch
changes no WinTrek, FedSrv, IGC, renderer or platform backend; no ship-visibility
checks or portable Windows/macOS builds were repeated. Full visual/state and
authenticated account/global-directory work remain below.

## Original loadout editor and slot audit (2026-10-07)

Original references: `WinTrek/load.cpp::HullMenu`, `GetNextHullType`,
`OnMenuCommand`, `OnButtonDefault`, `SlotClicked`, `Slot::UpdateLayout` and
`Slot::Paint`; `clintlib.h::ReplaceLoadout`/`BuyDefaultLoadout`; and
`WinTrek/loadout.cpp::PurchasesPaneImpl` investment/filter/hover handlers.
This pass addresses editor behavior and original slot painters. Ship visibility
is closed and was not revisited.

- Hull choices retain the staged editor hull when it is unavailable or obsolete,
  rather than retaining the physical trade-in hull. Next/Previous traverses the
  original core order from that staged hull, with the original enable policy.
- Selecting the current hull closes its menu without rebuilding equipment,
  consulting presets, remembering another draft or playing the hull-change
  sound. This preserves deliberately emptied slots and ammunition quantities.
- Default and equipment-slot controls remain usable on an unavailable staged
  hull, as in the original. Individual stock, mount and purchase rules remain
  authoritative; editing a retained hull does not grant access to its stock.
- Original slots use a fixed 133-by-37 surface and input area. Alternate artwork
  keeps its native dimensions and clips at that surface. Launcher/pack quantities
  retain their original right alignment and small font. Turrets paint gunner or
  `<open>` text only when a weapon is mounted. These raw DrawString labels use a
  single line and surface clipping, including partial edge glyphs, rather than
  wrapping/truncating long names to an invented text-box width.

The scene-free original-dialog check now includes docked workflows and the
loadout editor matrix: 1,599 staged hull-menu states and 3,198 next/previous
traversals across the six installed catalog cores plus `static_core`, including
1,587 unavailable staged states. Current-hull no-op selection and live-ship,
money, RNG and temporary-ship invariants pass. An isolated asset-cache fixture
checks 12 oversized cards against the original fixed surfaces/hits, empty and
mounted turret labels, long gunner names and right-aligned quantities. Existing
original menu, local-game, training-entry, keyboard, list and docked lockdown
checks pass. Frozen original loadout purchase/restoration and saved-bank tests
pass. Encrypted purchase and investment cases pass, including queued stock
withdrawal, canonical completion, reconnect, atomic launch and concurrent
investment completion. The final purchase case also passes with the final
coherent client/server build.

Linux client/server/network checks use coherent identity
`67829c918f59f290e7bcaf1ce76ba90e1cf22fd5176b5e9e4f40539cd0f2d7a3`.
Logs are `/tmp/allegiance-loadout-editor-*`; `-final` identifies final builds and
checks. This batch changes no WinTrek, FedSrv, IGC, renderer or platform backend,
and adds no simulation or per-frame preview work. No GUI/ship-visibility checks
or portable Windows/macOS builds were repeated. Full faction/state visual
comparison and authenticated account/global-directory work remain below.

## Direct surface text painter audit (2026-10-07)

Original references: `teamscreen.cpp` team/player painters; `gameoverscreen.cpp`
player/side painters, sort/header handlers, chat and Back; `trekinput.cpp`
mapping painter; `introscreen.cpp` LAN painter; `teleportpane.cpp` destination
painter; `loadout.cpp` purchases/cards/part-info; `teampane.cpp` compact/expanded
player/team painters; `mappreview.cpp` map text; and `engine/menu.cpp` captions.
Existing lobby admission/leadership, stable sorting, postgame defaults, chat
routing, countdown and next-round behavior were checked against these handlers.
No new differences were found in those reviewed action policies. Persistent
account/rank/moderator services remain separate outstanding work.

The text comparison found a repeated painter mismatch: direct original
`Surface::DrawString` text was still using native box wrapping or whole-character
truncation. `OriginalControl::surfaceText()` now explicitly selects one raw line
with glyph clipping at the supplied surface rectangle. This shared declaration
is applied to lobby team/member/applicant text, mappings, LAN names/counts,
teleport rows, postgame cells and chat lines, purchases/cards/part-info, F6 player
and team text, map-preview labels, classic popup captions and inline chat popup
captions. The existing loadout slot policy uses the same declaration. MDL
StringPane, StringImage, EditPane and authored HUD text retain their distinct
interpretation, fonts, line breaking and clipping rules.

Postgame has an additional original distinction: only player names/ranks and
team names acquire per-column clip rectangles. Numeric statistics and position
indices draw against the whole row surface after those clips are restored.
Native now preserves that behavior rather than clipping every numeric column.
This matters for long scores and alternate sets with narrower authored columns.

The UI-only Wayland/Vulkan check compares actual host text to independent
unbounded DrawString placement clipped at original surfaces: 217 strings across
eight hosts (lobby, mappings, LAN, postgame, teleport, investment cards, investment
info and options menus). It includes long names, 1024/1024 occupancy, large
statistics and deliberately narrow independently overridden MDL columns.
All pixels match; the original numeric/name/rank clip distinction is also
asserted. Existing disabled-hover, capture, button/header/arrow pixel and map
input checks pass with Vulkan validation enabled and no reported errors.
This check constructs no Game, world geometry or model preview; ship visibility
was not revisited. Pixel captures are `/tmp/allegiance-ui-hover-*-surface-*.ppm`.

Headless original-dialog/loadout/docked workflow, MDL compilation, F6 player-pane
and postgame scoring/1024-and-2048-row/next-round/export checks pass. Encrypted
GNS lobby configuration, team changes, readiness, countdown, reconnect, victory
and next-round checks pass. Client/server/network checks share identity
`902291ad49f7d84e9b386999de7a4cf500fab4e029339cf6c83444df41c33127`.
Logs are `/tmp/allegiance-surface-painters-*`. All validation processes exited;
the UI window closed normally. This batch changes no WinTrek, FedSrv, IGC,
renderer or platform backend and adds no preview/simulation work. Portable
Windows/macOS builds were not repeated. Remaining scope follows.

## Original slideshow menu and lifecycle audit (2026-10-07)

Compared the original `Window` keyboard-filter ordering,
`WinTrek::OnTrekKeyFilter(TK_MainMenu)`, `ShowMainMenu`, `StartQuitMission`,
`DoQuitMission`, `DoInputConfigure`, and the three `Slideshow` hosts against the
native event loop. The native slideshow previously intercepted every key before
the global mapped main-menu command. Its Mission quit action was unreachable;
simply exposing that action would also have left the slides/narration alive and
sent an introduction without a Game to the wrong destination.

The original interface now has an explicit slideshow host, with no new artwork
or controls. The existing compiled slide image remains live behind the original
menu, options, Help, input mappings and confirmation primitives. The mapped
main-menu key and screenshot command precede ordinary slide advance. Foreground
popups own their input; closing them resumes the current slide. Unmapped keys
and mouse-button down events still advance it immediately. Narration and its
original automatic-advance timer continue while a popup is open.

`Mission` is available during both introduction and debriefing, with the original
"Quit the Current Training Mission?" confirmation. Cancel resumes the slideshow.
Accept destroys the slide/audio cue, cancels pending simulation startup and
returns to the original lesson selector, including when no Game exists yet.
Normal mission startup and debrief completion retire slideshow menu state;
completion also removes a foreground popup instead of leaving its controls over
the lesson selector. Existing live-game quit and popup behavior is retained.

Validation:

- Headless original-dialog checks cover introduction/debrief menu availability,
  absence of live-mission information entries, options/Help/mapping capture and
  return, training quit/cancel, and retirement of the Mission entry. The full
  existing creation, authored training, keyboard/list, loadout and docked workflow
  suite also passes.
- A new `--training-slides-menu-smoke-test` runs the actual SDL/Vulkan host with
  **no Game or world**, using the installed multi-slide weaponry introduction
  and welcome debrief. It remaps the main-menu key to Ctrl+F12, exercises Help
  and input mappings, verifies ordinary Escape/middle-click advance, both quit
  directions and cancellation, and checks debrief completion with a popup open.
  It passes after 240 frames, with Vulkan validation and zero scene triangles;
  the window closes normally. The final original menu/lesson-selector capture
  was visually inspected at native integer scale.
- Rebuilt Linux client/server/network checks coherently. The encrypted lobby
  configuration, team changes, readiness, countdown, reconnect, victory and
  next-round regression passes. Build identity:
  `e9c58e45b83b6ed59ece6c23a1d49a0643a649973c50b7464c3ff84484bea837`.

Logs: `/tmp/allegiance-slideshow-menus-{build,dialogs,sdl,network}.log`.
Capture: `/tmp/allegiance-slideshow-menu-smoke.png`. All validation processes
exited. No renderer, WinTrek, IGC, networking behavior or platform backend was
changed; Windows/macOS rebuilds were not repeated for these portable UI changes.
Full restoration remains incomplete; the remaining scope follows.

## Original mouse modes and pilot cursor audit (2026-10-07)

Compared `WinTrek::OnTrekKey(TK_ToggleMouse)`, its override-view filter,
`UpdateMouseEnabled`, `SenseJoystick`, `TrekInput::ClearButtonStates`, and
`ConsoleData::Button`/`PickObject`/`PickShip` against the native input host.

- `ClientCore/MouseInput.h` preserves the original toggle precedence. Space
  dismisses Team/Expanded Team/Game State/Teleport and enables mouse steering;
  a fresh Investment pane instead releases its pointer ownership without
  changing the mouse preference. Otherwise it toggles steering. Override views
  use the direct toggle and clear mapped mouse buttons without losing held
  keyboard or physical joystick input.
- Classic "Use Mouse As Virtual Joystick: Off" disables mouse steering. On
  uses the original accumulated, radially clamped joystick. Visible cursor
  ownership no longer disables keyboard/controller axes, throttle or camera
  controls. Mouse versus physical-axis selection, roll-modifier handling and
  flip-Y follow `SenseJoystick`; the optional modern UI retains direct relative
  mouse support. Options displays the live mouse mode, persists an explicit
  option change, and avoids unnecessarily reconfiguring physical controllers.
- The original free cursor now reuses recipient selection/order dispatch in
  pilot views as well as command view. Projected model centers/radii and IGC
  command eligibility determine picks. Left-click selects command recipients
  without changing the weapon target; Shift changes the group. An empty point
  quits composing and retains recipients, while an empty rectangle clears them.
  Both buttons initialize the waypoint ground anchor, so left-click can finish
  a composed Go-to command. Right-click sends the existing object/waypoint
  order. Pointer capture and selection/height feedback follow these gestures
  in either view. Middle/extra buttons and wheel bindings remain independent
  of whether mouse steering is enabled.

Validation:

- 1,048,936 independent frozen-original toggle/axis comparisons pass, plus the
  existing virtual joystick, acceleration, keyboard, autopilot and flight-input
  checks. The installed-map check includes cursor-owned left/right buttons
  without suppressing mapped middle-button or wheel commands.
- Actual headless original-UI checks cover 24 pane/mouse-mode combinations,
  capture retirement, fresh Investment's two-stage toggle, runtime options and
  controller isolation. Actual IGC/chat dispatch is checked from pilot and
  command views, including projected picking, weapon-target separation, Shift
  groups, retained recipients, left-button waypoints, right-button object orders,
  empty rectangles and inactive-screen gates. No game scene is rendered.
- The existing UI-only SDL/Wayland/Vulkan keyboard smoke passes after 220 frames
  with validation, zero scene triangles and normal window closure. Encrypted
  input and stock-Scout throttle regressions pass, including 90/180-degree turns,
  braking, vector lock/Shift, docked input and reconnect at 10 Hz reports.

The Linux client, server and network check were rebuilt coherently with identity
`2087376f590a279ca4f1d86e1c188ee168858155895a124f9fa06e6c77b91318`.
Logs are `/tmp/allegiance-mouse-control-{build,dialogs,controls,keyboard,network-inputs,network-throttle,ui}.log`.
All validation processes exited. This batch changes native input/UI adapters and
adds shared original-input policies; WinTrek, IGC, network records and platform
backends are unchanged. Windows/macOS rebuilds were not repeated for portable
changes. Ship visibility remains closed and was not retested.

## Public discovery and original online entry audit (2026-10-07)

Closed the missing public-registration backend behind the original Internet
games and Create Game server lists. `allegiance-directory` is an independent,
asset-free GNS service; `allegiance-server --directory HOST:PORT` registers the
authority's existing entry listener. The client's existing `servers.txt` can
contain directories or direct authorities, without a new default screen,
control scheme or additional original-game button.

Registrations contain native, pointer-free build/content identities and a
listener port. The directory constructs the endpoint from the transport's
observed numeric source address, then verifies the actual listener with the
existing encrypted, build/data-gated browse handshake before publishing it.
IPv4/IPv6 formatting, bounded registration/probe work, malformed requests,
incompatible data, inaccessible listeners and directory-forwarding loops are
handled. Leases renew periodically; disconnects and timeouts withdraw entries.
Registrar retries and operator status are independent of simulation and clients.

`RegistryTable` encodes one immutable endpoint report per membership change;
heartbeats and duplicate publishers reuse it. Browsers merge independent
directory memberships, retain their four-probe concurrency bound and retrieve
normal metadata/core catalogs/missions directly from authorities. Withdrawing
one directory does not remove another directory's membership or explicit saved/
live LAN endpoints. Selecting a discovered server for creation no longer
silently converts it into a permanent saved endpoint. Registry membership is
merged after message-local endpoint iterators have been retired.

Validation:

- Synthetic checks cover 256 unique authorities, deterministic raw records,
  cached reply identity, duplicate publishers, capacity/ownership, lease and
  disconnect retirement, observed IPv4/IPv6 addresses, proof-before-publication,
  malformed/incompatible/unreachable/late proof, forwarding-loop rejection,
  registrar renewal, lost acknowledgements and independent retry.
- The encrypted `--case directory` check discovers an authority from directory
  endpoints alone, populates actual original server/core controls with the real
  installed catalog, creates and joins a separate hosted mission, and finds its
  pilot through a fresh directory-only browser. It covers two directories,
  duplicate-free shared membership, independent withdrawal, saved endpoints,
  uninterrupted existing clients, re-registration and confirmed data rejection.
- Existing encrypted direct/LAN browsing, Find Player and remote multi-game
  creation/reservation/retirement checks pass. Original-dialog checks pass.
- Both actual command-line entry points start, the standalone server logs a
  successful registration, and both exit cleanly on shutdown. Packaging scripts
  include the new directory binary and pass syntax checks. No Windows/macOS
  rebuild or game-scene/ship-render check was repeated for this portable batch.

The Linux client/server/network/directory binaries were rebuilt coherently with
identity `ddc27bc7283bed65479c2dfe8812e70fb9f12acd0e1ded2545fec8258d58b4b8`.
Logs are `/tmp/allegiance-directory-{build,check,network,browse,create,dialogs,cli,server-cli}.log`.
All validation processes exited. WinTrek, IGC and renderer/platform backends are
unchanged. Setup is documented in `native-frontend.md` under Server discovery.
This supplies a self-hosted service; a deployed community directory/seed address
is still needed. Stable game identities/deduplication across saved DNS and IPv6
aliases remain a separate functional audit item.

## Original browse identity and selection audit (2026-10-07)

The original lobby maintains `MissionInfo` objects by mission cookie
(`BaseClient::GetLobbyMission`); a selected mission is not identified by its
current address. Native browse rows previously used addresses alone, duplicating
aliases and losing selections on route changes. A mission/server restart at the
same endpoint could also inherit a selection and admit it to a different game.

Each native hosted mission now has a random 128-bit lifetime identity, independent
of IGC randomness and round changes. An authority advertises its entry mission's
identity; separate hosted missions have separate identities. Directory probes
require a nonzero identity and a matching initial mission. Actual observed GNS
remote endpoints supply join routes and relative created-game ports.

Browse rows are compacted with hash tables rather than quadratic address scans.
Aliases retain first-seen ordering and newest metadata/routes; newer replies at
a reused listener retire its stale identity. Original Games, LAN and Create Game
selections follow identity across route changes, retire on removal/restart and
do not resurrect on rediscovery. Create drafts/core choices remain independent.
Find Player merges an aliased result into the existing mission row.

Original Games/LAN joins carry the selected identity through the callsign dialog
and into `Hello`. Authorities reject a mismatched mission before allocating a
pilot. Direct-address joins remain supported; `Welcome` binds subsequent
reconnects and round changes to the admitted mission. These identities add 16
bytes only to browse/join/welcome records, not simulation reports or inputs.

Validation:

- Headless checks cover 5,120 aliases, newest metadata/routes, stable ordering,
  listener reuse, original Games/LAN/Create selection, removal/reappearance,
  retained drafts/core choices and Find Player merging. Original-dialog checks
  pass without rendering a game scene.
- Real encrypted IPv4, IPv6 and mapped-IPv4 probes identify the same mission;
  coalescing returns one row. A stale identity is refused before either pilot or
  replica allocation. Direct joins receive the advertised identity.
- Encrypted direct/LAN browsing, Find Player, public-directory expansion,
  remote multi-game creation/reservations and next-round lifecycle checks pass.
  Synthetic public-directory checks and raw-record default/padding checks pass.

The Linux client/server/network/directory binaries were rebuilt coherently with
identity `d18b67de4bb734b253a51015b28d1842e912ecd46e1597f3681ad9c5aaf0bca4`.
Logs are `/tmp/allegiance-browse-identity-{build,dialogs,directory,network,public,create,lifecycle}-final.log`.
All validation processes exited. WinTrek, IGC, renderer and platform backends are
unchanged. Portable changes did not trigger Windows/macOS rebuilds. Ship
visibility remains closed and was not retested. Numeric IPv4/IPv6 aliases are
supported; hostname resolution and a deployed community directory/seed remain
open work, not claimed features.

## Hostname discovery, joining and lifecycle audit (2026-10-07)

Closed the numeric-only restriction in the native online-entry backend. Existing
saved endpoints, public-directory registration, Create Game requests and direct
joins now accept hostnames as well as numeric IPv4/IPv6 addresses. No original
screen, control, artwork or configuration surface was added or changed.

`endpoint_resolver` validates endpoint syntax and resolves UDP addresses with the
platform's `getaddrinfo` on two process-wide workers. Pending work is bounded;
cancelled requests are retired and workers hold only result state, never a UI,
transport or client pointer. Closing an entry or exiting does not wait for an
uncancellable OS lookup. Windows uses Winsock; macOS/Linux use the native socket
facilities. Resolution returns at most eight unique numeric endpoints.

`hostname_transport` keeps a stable logical client peer through resolution and
address attempts. Later addresses race at 250 ms intervals, so an unreachable
preferred IPv6 route does not stall a reachable IPv4 route. The first connected
attempt wins; losers and late callbacks/messages are retired. DNS errors,
five-second resolution timeouts, transport failures, cancellation and reconnects
retain their distinct retry/error behavior. Borrowed GNS messages are filtered
and translated in place; batched sends retain shared payloads and lane metadata.
There is no unrelated cap on connected peers.

All server traffic and numeric direct joins retain the raw GNS path. Original
Games/LAN/Create lists still use observed numeric endpoints and stable mission
identities. Hostname creation replies derive the child port from the actual
connected authority, and reconnects remain bound to the admitted mission.
Find Player checks compare mission identities across address families.

Validation:

- Synthetic checks cover endpoint/default-port parsing, malformed input,
  pending resolution, stagger deadlines, fallback/open failures, one winner,
  numeric route/quality reporting, borrowed-message ownership, shared batched
  sends, permanent/transient errors, cancellation/teardown and late-result/
  callback immunity. The actual OS resolver resolves `localhost` asynchronously.
  A 1,024-peer numeric fixture incurs no DNS work and tears down every peer.
- Encrypted browse checks resolve lowercase/uppercase hostname, IPv4, IPv6 and
  mapped-IPv4 aliases to one mission, perform an identity-bound hostname join
  and reconnect, and preserve pilot/mission identity and original ownership.
- Encrypted public-directory checks use hostname registration/discovery and
  coalesce authorities advertised through different IP families. Original
  server/core controls, creation, joining, Find Player, independent withdrawal
  and registration recovery pass. Remote-creation checks issue hostname
  requests and verify numeric created-game routes and separate mission IDs.
- Public-directory synthetic checks and original-dialog checks pass. The new
  resolver/transport/GNS-adapter objects cross-compile with Clang against the
  Windows MSVC SDK and macOS 26 SDK. Full platform builds/runtime checks were
  not repeated for this backend-only batch.

The final Linux client/server/network/directory build identity is
`a29b2fa5126bbd83544a42f11a0d775c6985fbeb04430e7c48eae6ff224d0e0e`.
Logs are `/tmp/allegiance-hostnames-{build,check,browse,public,create}-final.log`,
`/tmp/allegiance-hostnames-{directory,dialogs}.log`, and
`/tmp/allegiance-hostnames-{windows,macos}-objects.log`.
All validation processes exited. WinTrek, IGC, simulation reports, renderer and
frame pacing are unchanged. A deployed community directory/seed remains external
setup work; the native service and hostname-capable entry path are implemented.

## Compiled HUD pane and image layout audit (2026-10-07)

This batch addresses shared MDL interpretation rather than adding individual
widget layouts. The reference points are `engine/controls.cpp` ImagePane layout,
`engine/image.cpp` ClipImage/JustifyImage/StringImage, `engine/modeler.cpp`
PaneImageFactory and `effect/efimage.cpp` BlendColorImage. The separately
implemented `OriginalUi` adapter supplies state-dependent comparison records.

The compiled HUD path now supports nested ImagePane children, the PaneImage
coordinate bridge, BlackPane backgrounds, ClipImage, BlendColorImage and both
axes of JustifyImage. Each fixed pane clips its descendants to its own extent;
oversized text or nested children cannot widen the parent or escape its surface.
Composed backgrounds retain their origin when their content starts farther
right or below it. Image groups use their declared bounds for justification,
including the full extent of a gauge whose visible fill is smaller. Empty
switches do not manufacture color overlays. Clipped images retain the matching
texture coordinates, and overlays preserve WinTrek's premultiplied color fill
through the native renderer's source-alpha quads.

Layout scopes compile into indexed spans alongside the existing arithmetic
and painter instructions. Evaluation traverses these flat records; it creates
no legacy Image/Pane objects, searches no MDL names, loads no assets or fonts,
and creates no GPU resources. Text height used for group bounds is cached until
the text changes, using the resident font and original line-breaking policy.
Existing text anchors and font choices are retained. Native solid rectangles
use the existing quad renderer. WinTrek and IGC receive no changes.

Validation includes 96 independent adapter comparisons over nested panes,
partial/empty clips, all justification axes, changing strings and tint values;
texture coordinates and painter order are checked as well as visible geometry.
A separate assertion fixes the full-width anchoring of a half-filled gauge.
All six installed HUD styles and 18 compiled HUD surfaces pass, as do all 456
inventory style/row/readiness cases and the existing alternate-description and
namespace-lifetime checks. Original-dialog/loadout workflow checks also pass.
Encrypted browsing, hostname joining, identity binding and Find Player checks
pass with the rebuilt network library. The UI-only Wayland/Vulkan keyboard
check passes with validation enabled: 220 frames, no reported validation errors,
no missed presents and zero scene triangles. All validation processes exited.

For each of the six styles, a synthetic benchmark evaluates seven resident
HUD/inventory programs over 2,000 changing-state samples. Median evaluation
cost is 0.97–2.75 microseconds; p99 is 1.05–3.17 microseconds on this machine.
Output addresses remain stable and text buffers retain their reserved capacity.
These numbers exclude compilation, GPU drawing and gameplay simulation; they
are current cost measurements, not a before/after speedup claim.

Logs are `/tmp/allegiance-mdl-layout-{build,check,dialogs,browse,ui}.log`.
The coherent Linux build identity is
`3e655b7313088f7bfd68b19dda54b1ad221f33c97b302bb3086661f076013368`.
Windows/macOS builds are not repeated for these portable compiler changes.
Further authored-host, alternate-asset and visual/state comparisons remain in
the queue below; this is not a claim of full UI equivalence.

## Compiled crew and console adapter audit (2026-10-07)

The expanded MDL painter vocabulary exposed two adapter gaps: CrewPane treated
every non-text record as a bitmap, while ConsolePane discarded solid fills.
CrewPane also interpreted all text as StringImage, losing StringPane's distinct
single-line behavior, and cached InTurretPaneImage once with HUD style zero.
These paths could therefore lose or misinterpret otherwise supported authored
content, especially in alternate asset sets.

Both display hosts now use one shared conversion for text, bitmap and solid
rectangle records. It preserves imported font arguments, pane/image text mode,
single-line behavior, justification, color, blending, UVs and inherited clips.
Preview/inventory records remain with their dedicated scene/inventory hosts.
InTurretPaneImage now uses a compiled resident program for each selected HUD
style, like the other crew images. No per-frame MDL or asset traversal is added;
unchanged crew and console views retain their controls and output storage.

The comparison also exposed a bounds detail from StringImage::CalcBounds:
empty text retains its declared width and zero-height baseline. Those baselines
participate in group bounds, including a color fill over a partly empty critical
tip stack. The VM now preserves that behavior while continuing to exclude empty
clipped bitmap regions. Installed HUD anchors and gauge crops remain unchanged.

An isolated alternate dialog.mdl exercises nested keyed panes, fixed-size
clipped StringPane text with an 18-pixel bold imported font, partial image clips,
solid fills, style-dependent colors and the actual crew/console adapters. All
160 independent record comparisons pass: six HUD styles, pilot/observer/gunner,
1x/2x integer scales, flight/loadout, hidden/shown and differing chat heights.
Sixteen UI-only Vulkan pixel comparisons also match the separate authored
adapter exactly. The test creates no Game, world geometry or model preview.
Existing hover/capture/header/arrow/map routing and eight direct surface-text
pixel checks pass with Vulkan validation enabled and no reported errors.

Original-dialog/loadout workflows, console chat/ballot/tip expiry, all six HUD
styles/18 surfaces, 456 inventory states and encrypted browse/join/Find Player
checks pass. Resident seven-program HUD evaluation measures 0.96–2.73 us median
and 1.03–2.82 us p99 over 2,000 samples per style on this machine; these are CPU
evaluation costs, excluding GPU drawing and simulation. All validation processes
exited and the UI test window closed normally.

Logs are `/tmp/allegiance-display-adapters-{build,dialogs,mdl,console,pixels,browse}.log`;
pixel captures are `/tmp/allegiance-ui-hover-*-adapter-{actual,reference}.ppm`.
The coherent Linux build identity is
`f51660a51f88a8a5c1adbb4f4472198d9262f4d7b0543597ab2e3d3c0b675796`.
WinTrek, FedSrv, IGC, platform backends and frame pacing are unchanged. Portable
Windows/macOS builds are not repeated for this batch. Wider visual/state and
alternate-asset coverage remains in the queue below.

## Translated global keys and Help ownership audit

The native host previously dispatched Help only after requiring a Game and
after ordinary menu input. This lost the original global key behavior on the
main menu and training selection; a remapped Help key could advance a training
slide. Help also hardcoded physical F1/Escape closure even when those keys had
been remapped. WinTrek instead runs translated global keys after actual popup
capture and before ordinary screen input. Its Help image is independent of the
PopupContainer.

`ClientCore/GlobalKeyFilter.h` now expresses the original dispatch/pass/consume
rules for Help, MainMenu, Screenshot, chat paging, Quick Chat and the override
cursor toggle. Native dispatch uses the installed logical command and its
aliases, retaining real popup and mapping-capture priority. WinTrek keeps its
existing handler. Help can toggle before a Game exists, retains its parent when
the main menu opens over it, and leaves unbound physical F1/Escape alone.
Lobby/results chat paging and Quick Chat remain reachable behind Help; a real
Quick Chat popup then takes keyboard ownership. Consumed printable global keys
also consume their separate SDL text event, preserving a restored chat draft.

The policy passes 28,672 logical-key/context comparisons against a frozen
OnTrekKeyFilter oracle. Headless original dialogs pass, including nested
popup/modal ownership and retained private recipients/drafts. The UI-only SDL
checks pass 340 frames for default/remapped/aliased Help, unbound F1, printable
shortcuts, main-menu priority, key capture and parent restoration, and 240
frames for introduction/debrief Help, remapped main menu, ordinary slide
advance, quit/cancel and foreground completion. Both create no Game or world
geometry and report zero triangles; Vulkan validation reports no errors.
Installed keymap, MDL and console checks and encrypted browse/join/Find Player
checks also pass. All test windows/processes exited.

At this point original Help anchor/motion and ordinary background focus still
remained; the following independent-overlay audit completes those gaps. Ship
visibility is closed and was not changed or retested. WinTrek,
FedSrv, IGC, rendering/platform backends and frame pacing are unchanged; portable
Windows/macOS builds were not repeated. The coherent Linux build identity is
`483c32d12df0ea95846bef2b451c8fd12c73885bf74256512620980dcb5d4832`.
Logs are `/tmp/allegiance-global-filter-{build,dialogs,keyboard,mdl,console,ui,slides,browse}.log`.

## Independent Help and popup layer audit

Help now remains an independent authored pane instead of replacing the native
screen. Its original image-space positions are off `(-1300,30)` and on `(30,30)`;
`ClientCore/HelpMotion.h` retains the half-second linear motion and reverses
continuously from the current position. The native host converts Y-up image
coordinates to the bottom-left screen anchor and applies the existing whole
integer scale policy. The installed 1300x700 image and its Close/Discord/Wiki
controls are retained once. Animation changes only the scalar translation,
without MDL/asset traversal, layout reconstruction or per-frame control allocation.

Opening/toggling Help leaves the actual screen, keyboard focus and continuous
input intact. Lobby text editing/sending and ordinary slideshow advance remain
available. Close and web buttons use their authored geometry, sound sinks and
retained mouse capture; release outside and focus cancellation do not replay
actions. OnHelp retains the original positive-button cue. Actual popups still
own translated keys and mouse input before Help.

The image stack was checked against GroupImage::AddImage/Render/HitTest:
screen/HUD below Help, PopupContainer above Help. Native popup composition now
uses a shared cached foreground boundary for menu/quick/combo popups, Controls,
Credits, LAN discovery, game filter/create/find-player dialogs, callsign,
team settings and briefing. Nested backdrops retain that boundary; captured
button indices and Credits scrolling stay with the foreground. This also fixes
the LAN dialog replacing its real parent with an incomplete raw intro layout.

TrekInput::TranslateKey also records raw held state before command filtering.
Native input now preserves that order: a consumed Help chord can coexist with
a continuous Any-modifier bind on the same physical key. The UI-only SDL check
verifies both simultaneous holding and release using an isolated focus context,
without creating a Game or dispatching flight controls.

Validation passes: 360 independent motion/reversal samples; six anchors/hit
regions across three viewport sizes and 1x/2x requests; three Vulkan pixel
comparisons against a separately translated authored root; five direct popup
states and a nested Credits/menu/mapping boundary; 28,672 original global-key
comparisons; focused editing/sending, authored sounds and capture. The 480-frame
SDL menu test and 270-frame training test pass with zero world triangles and no
Vulkan validation errors. The existing UI pixel/capture/surface/compiled-adapter,
original-dialog/loadout, MDL, console, installed-keymap and encrypted
browse/join/Find Player checks pass. All validation processes/windows exited.

Logs are `/tmp/allegiance-help-overlay-{build,dialogs,keyboard,mdl,console,ui,slides,pixels,browse}.log`;
Help captures are `/tmp/allegiance-ui-hover-help-phase-{25,50,100}-{actual,reference}.ppm`.
The final coherent Linux build identity is
`c8a7e42240cfed7826533c7d989cec9ced42573344e975dd66ff9c26463df35c`.
WinTrek, FedSrv, IGC, renderer/platform backends and pacing are unchanged in this
batch. Portable Windows/macOS builds were not repeated. Further alternate-asset
and screen/state coverage remains below; this is not full UI completion.

## SDL mapping event audit (2026-10-07)

The foreground popup route discarded SDL modifier flags before calling the
original key-mapping dialog. Both popup and ordinary menu routes now use one
adapter that preserves Shift, Control and Alt, including left/right variants.
Conflict confirmation still replaces only the exact chord. Modifier-only
presses remain ignored, as in TrekInput's editor.

Gamepad menu-key synthesis is restricted to the optional modern interface.
Classic controls retain raw joystick capture; a second gamepad event cannot
synthesize Enter and restart capture after the raw button finished it.

The UI-only `--classic-input-routing-smoke-test` passes through the actual SDL
loop: eight modifier combinations, modifier-only press/release, exact conflict
cancel/replace, whole-dialog cancellation, persisted mappings, gamepad
non-navigation and GUID-backed button capture. Its 200 frames create no Game,
render zero world triangles and report no Vulkan validation errors or missed
presents. Original-dialog, installed-keymap and virtual-joystick checks pass.
Linux client/server/network-check/directory builds are coherent. Logs are
`/tmp/allegiance-input-routing-{build,ui,dialogs,keyboard,controller}.log`.
No WinTrek, simulation, renderer or platform code changed in this batch.

## Full-roster and player painter audit (2026-10-07)

Compared TeamScreen::PlayerPainter and both teampane painters with the native
records. Lobby name/rank/status clips now use their original 14-pixel row
rectangle intersected with the list viewport. Wing text retains the inherited
list clip. Host/commander marks use original integer centering. Expanded F6
names use the original 120x20 clip and locations use a 15-pixel row clip;
compact names and the other unbounded fields retain their original behavior.
These are cached native painter changes; WinTrek is unchanged.

The new full-lobby matrix covers six factions of 256 pilots and 48 applicants:
63 sort/view combinations, 17,424 clipped virtual rows, complete keyboard
access, duplicate/long names, seven selection-preserving refreshes and seven
context-menu/private-recipient retirements. Seven Vulkan row comparisons match an independent implementation
of the original painter, including every faction and No Team. Expanded and
compact F6 metadata and pixels also pass. The new row-clip assertion failed on
the previous implementation and passes after the fix. Original-dialog checks,
including 1,599 hull menus and 3,198 traversal cases, pass. No world rendering
is used in these new checks, and Vulkan validation reports no errors.

Logs are `/tmp/allegiance-full-lobby-{build,dialogs,pixels}.log`; the failing
regression is `/tmp/allegiance-full-lobby-clips-before.log`. Client, server,
network-check and directory Linux builds share identity
`885604dd8f911d757a8c472df81f772216d2448e2d58d64d107ef55423959b7c`.

## Authored loadout calibration audit (2026-10-07)

WinTrek reads the hull and equipment gauge maxima from `hullinfo.mdl` and
`partinfo.mdl`; the native panel still used copied stock values. The native
producer now supplies compact raw IGC metrics, and the presentation applies
the active asset calibration. This includes all eight hull metrics, equipment
signature/mass, weapons, missiles, probes, mines, chaff, shields, cloak duration
and afterburner speed/endurance. Unscaled percentages and the original unused
pack/lock/on-rate declarations retain WinTrek's behavior.

Calibration is cached once per asset namespace and reset when that namespace
changes. The change removes string-map construction from the game metadata
producer and adds no per-frame MDL lookup. No WinTrek, FedSrv, simulation,
renderer, network or platform policy changed.

An independent reference from `WinTrek/load.cpp` passes 1,980 part/hull/global
attribute comparisons and 24,660 numeric fields. All nine equipment categories
pass 27 stock/altered/reloaded authored panel comparisons, including hull
signature reciprocals and the derived cloak/afterburner maxima. Eighteen
independent Vulkan comparisons also match exactly. These checks render only
the authored UI root; they do not render world geometry or preview ships.
Original-dialog and existing purchase/restoration/loadout checks pass, with
no Vulkan validation errors.

Logs are `/tmp/allegiance-loadout-calibration-{build,dialogs,loadouts,pixels}.log`.
The coherent Linux client/server/network-check/directory identity is
`89f78abc8529dc78b468b4f367913fe50b0067a09970a5b05be7cb9d3f5d3ef8`.
Portable Windows/macOS builds were not repeated for this asset-presentation
change. Ship visibility remains a closed issue and was not revisited.

## Reported boarding and teleport header audit (2026-10-07)

WinTrek's teleport pane lists docked teammates using PlayerInfo's reported
status, hull and name. The native replica now follows that policy independently
of physical ship replicas. Current public names take precedence; active own
private accounts cover local pilot fixtures and reconnect grace without live
peers. Retired, foreign, stale, flying and non-turret destinations are excluded.
The current server already publishes docked ship replicas; this change removes
the UI's dependency on those physical objects rather than fixing a universal
absence of remote docked replicas.

Reported gunner occupancy is counted once for all destinations, excluding
observers. Full ships remain listed, and the authority still validates boarding.
Boarding captions and the original failure-message/sound distinction also use
reported metadata. Offline and authority transfer rules are unchanged.

The authored destination headers previously navigated back to the teleport pane
and rebuilt it. Their original callback is empty. Although the artwork contains
checked faces, the original factory disables toggling and tab behavior. Native
header clicks now preserve the list, selection and authored root. Left and right
presses retain the original capture/pressed-face behavior; only left release
produces the original click sound. Canceled releases do neither.

Regression checks pass 180 status/faction/hull/membership combinations without
physical target or crew replicas, three active-account cases, pending boarding
and rejection feedback, live target retirement, and 1,024 reported teammates
plus the viewer. The latter includes 128 carriers, observer-free occupancy and
original Page Up/Page Down access to the complete list. All four inert headers
pass left/right capture, canceled-release, sound and unchanged-root checks.
Four independent Vulkan comparisons use explicit source bitmap slices, including
a test-only distinct pressed-frame copy because the installed normal and pressed
header images have identical pixels. Installed assets are unchanged, and these
comparisons render no world geometry.

Existing offline teleport and encrypted docked-crew regressions pass, including
full-ship/first-free reboarding, atomic rejection, current-station disembark/refit,
remaining crew, causal completion and stale-flight feedback. The encrypted test
reported 0.096 ms server p99 and zero continuous correction at 10 Hz snapshots.
No WinTrek, FedSrv, simulation, renderer or wire-record changes were made.

Logs are `/tmp/allegiance-remote-teleport-{build,dialogs,offline,network,pixels}.log`;
the initial failing report-only regression is
`/tmp/allegiance-remote-teleport-before.log`. The final coherent Linux
client/server/network-check/directory identity is
`42ee48704c219710742be1953fec34c758c3aa2cd7420331a90042c0bf818815`.
The encrypted test preceded the final test-only bitmap diagnostic changes;
runtime sources are identical. Portable Windows/macOS builds were not repeated.

## Original button input and setup callbacks (2026-10-07)

Compared the shared native button routes with `engine/button.cpp`,
`engine/frameimage.cpp`, `effect/combopane.cpp`, TrainingScreen and
`newgamescreen.cpp`. Ordinary original buttons now retain right-button capture
and their pressed bitmap without firing the left callback or click sound.
Disabled buttons still capture; ordinary double-clicks do not. Animated buttons,
combos and scrollbar thumbs retain their separate left-only input policies.
The actual SDL capture dispatcher passes the release button through, including
menus, Help, F6 and Game State. Existing right-donation behavior is preserved.

The MDL compiler now retains the authored toggle argument. A checkbox's right
release can change its local checked state without invoking the host callback;
setup Accept reads those actual checks, and Cancel discards them. Training's
shared double-click callback launches the selected lesson on either button,
without the normal single-click sound. Modal right input cannot dismiss the
Details popup beneath it, including while a request is pending.

Game-type recognition now follows the original callback wiring instead of
recomputing on every layout. The related mission combos and the Development
and Invulnerable Stations left callbacks update the preset label; silent right
toggles and Eject Pods do not. An alternate MDL with identical checkbox faces
and a false toggle flag is correctly interpreted without changing a C++ host.
Retained widget overrides are allocated only on user input, restored on layout
rebuild and retired when authoritative checked state changes.

Checks pass 135 controls across eleven hosts, 44 setup right-toggle/Accept/Cancel
cases, preset callbacks, alternate artwork, both-button training double-clicks,
modal and Help input. Independent Vulkan comparisons match explicit source
bitmap slices for distinct right-button down/up faces; all existing UI hover,
roster, loadout and overlay comparisons pass with validation enabled. Actual
Wayland/SDL input routing passes its modifier/gamepad and secondary-button
suites in a 200-frame UI-only run. Keyboard, F6 player-pane and original setup
checks also pass. No world rendering was used for the new button comparisons.

Logs are `/tmp/allegiance-button-input-{build,dialogs,pixels,sdl,keyboard,players,setup}.log`.
The coherent Linux client/server/network-check/directory identity for this
batch is `a1e828b597ce70909ebf08417ad3606bce9111438cdd5ab2b0a1c026a2587c66`.
The final Details modal guard was included in the final dialog run; the SDL and
pixel runs preceded that isolated guard. WinTrek, FedSrv, simulation and wire
records were unchanged in this batch. Portable platform builds were not repeated.

## Original team activity and fixed-world settings (2026-10-07)

The six authored `activeTeamButtonPane` controls were visible but bound to an
inert fallback. They now use the original TeamScreen/FedSrv policy: visible
only to the mission owner before start in a non-fixed-world game, checked from
reported team activity, enabled only for empty teams, and hidden beyond the
configured side count. Occupied seats include ghost/reconnecting humans at
the authority. Right release retains the widget's separate silent toggle;
left release sends the actual checked intent and side identity.

The authority validates owner, stage, world policy, side ID, boolean intent and
human occupancy. Inactive teams cannot accept join requests. Activity updates
preserve the prepared world and human identities, normalize affected alliance
groups and update the original Allow Empty Teams rule. Settings preserve the
authority's current activity; a client cannot bypass the checkbox validation
by forging activity in a configuration record. When play begins, inactive
sides' prebuilt bases and drones are retired without combat explosions or
spurious victory. New rounds restore all sides and their bases and reset the
explicit checkbox policy to requiring players. Untouched native solo/AI
scenarios retain their existing empty-team policy.

Offline setup uses the same authored buttons, empty-seat gates, blank inactive
team rows and original minimum-player behavior. Actual local game creation
removes inactive bases/drones while retaining the original faction seats.
Fixed-world lobbies also restore the original Settings/Details distinction:
editable settings and team activity are hidden, Details is read-only, and the
authority rejects configuration during a fixed-world game or countdown.

Bootstrap reuses the Session record's reserved byte for six inactive-side bits
and the minimum-player policy. Session remains 484 bytes; no record grows.
Existing team reports already carry activity, and an unused lobby flag carries
the fixed-world gate. There is no additional periodic message or per-recipient
encoding. Checkbox changes and start cleanup are event work. WinTrek and
FedSrv source/runtime paths are unchanged.

Checks pass 80 owner/stage/fixed-world visibility matrices, occupied-seat gates,
both-button intent, original row removal, policy/bootstrap round trips and a
real offline four-team world. The encrypted four-team check passes the actual
authored checkbox-to-server path, owner/occupancy/ID/value/world/countdown
rejections, inactive admission rejection, alliance normalization, forged-policy
settings preservation, start cleanup and next-round restoration. Active-base
assertions distinguish the authority's full world from a replica's legitimate
faction visibility. Existing encrypted alliance and lifecycle checks, original
dialogs, the full session suite, setup and Vulkan UI comparisons pass. The broader session fixtures now explicitly select server/core and click
Create. Return in the original Create Game edit remains inert, as WinTrek's
unsubscribed string event requires; empty/long-name checks still use the actual
button and original acknowledgment dialogs.

Logs are `/tmp/allegiance-team-activity-{build,dialogs,network,alliances,lifecycle,setup,pixels,session}.log`.
The final coherent Linux client/server/network-check/directory identity is
`dd732fba2357fed6eb0ade84fd081f2e5ed44eb989039582d0e90002e0852548`.
The encrypted activity/alliance and dialog checks use the final runtime;
the last build changes only creation-flow test fixtures. Lifecycle, setup and
pixel checks preceded the isolated fixed-world Settings gate, which is covered
by the new UI matrices and encrypted rejection check. The full session suite
passes on the final build. Portable platform builds were not repeated for
these portable changes.

## Original JOIN, No Team and admission parity (2026-10-07)

TeamScreen's JOIN gate now follows `UpdateButtons`: selecting another active
side or No Team enables the control; side locks and defection restrictions are
validated by the authority. Pending requests retain Cancel. The separate
`OnButtonJoin` active-side check is also preserved for a No Team pilot viewing
an occupied eliminated faction, including the AFK-only wake-up. Empty inactive
selections refresh to No Team remotely. Native local setup requires a pilot
seat and retains that seat when its selected empty side is disabled. Local
alliance choices use actual activity rather than absent/stale network reports.

Voluntary departure now follows FedSrv's `CS_QUIT_SIDE`, separately from
`C_POSITIONREQ`. Locked sides and disabled defections do not prevent leaving
for No Team. The retained faction history still prevents a defection and
allows rejoining the original faction once admission is unlocked. The
original non-dismissible "Quitting current team..." message waits for the
authority report incorporating its action, not just an early receipt. Stale
completion/failure cannot dismiss or overwrite a newer modal. Personal ready
updates follow FedSrv at every stage, including JOIN's pre-departure AFK wake-up
and postgame. Ended missions allow departure while preserving frozen results.

Authenticated No Team reconnects restore server-owned membership and faction
history. Initial connections still require a valid requested faction; the
No Team resume exception cannot create an unauthenticated seat. No wire record
or periodic report grows. Pending departure receipts reuse the existing
transaction/publication mechanism. WinTrek/FedSrv runtime paths are unchanged.

Checks cover 64 original JOIN formula combinations, 288 authored UI
stage/lock/defection/request/own/viewed-side combinations, inactive selection,
local inactive alliance choices and causal modal replacement. The encrypted
admission check covers the real authored No Team action, AFK wake-up, receipt
before publication, locked admission, remembered-faction reentry, direct and
No Team defection rejection, No Team reconnect, invalid initial No Team hello,
fresh late admission, postgame departure and unchanged frozen scores. The
full session suite and encrypted admission check pass on coherent Linux source
identity `ab414b0499a3d818756d3133361de7c1db9c13096ff52403ab71cf2f0fd153d9`.
Original dialogs and the existing encrypted lobby/lifecycle checks also pass
on that final build. Portable Windows/macOS builds were not repeated.
Logs are `/tmp/allegiance-team-admission-{build,session,network,dialogs,lobby,lifecycle}.log`.

## Original lobby controls, admission feedback and countdown parity (2026-10-07)

The grouped comparison against `TeamScreen`, `FedSrv` and `CFSMission` restores
Previous Scores presence from actual result records, the original locked-open
game policy for Lock Lobby/Auto Accept, independent Lock Sides, and the owner's
Randomize button at every stage. Randomize outside setup is silently ignored
by the authority, as in FedSrv. The authority accepts commander Force Ready
updates during play while the original UI retains its disabled control.
Locked-open policy survives configuration and next-round world replacement.

Explicit request cancellation now displays the original non-dismissible
"Canceling request..." message. It uses the existing action/publication
transaction: an early receipt cannot dismiss it before the canonical roster
incorporates cancellation, and stale completion cannot dismiss a newer dialog.
Join failures and commander rejection use the original reason-specific message
boxes. Auto Accept also reports applicants rejected by its admission recheck.

Unranked admission now follows original count/rank-one balance rules, including
Enforce Balance's minimum-side test, the ordinary two-newcomer allowance,
countdown-stage enforcement and exclusion of exhausted-life ghosts. A new
No Team arrival or a pending/failed request leaves a running countdown intact;
an actual side transfer retains the existing countdown cancellation policy.
Authenticated rank weighting remains part of the execution queue below.

Checks cover 64 owner/commander/stage/open-policy/result control combinations,
14 independently specified admission boundaries, cancellation capture and
stale completion, plus real encrypted reject/cancel, locked-open configuration,
running balance, ghost exclusion and countdown admission flows. The full
session and original-dialog suites, encrypted admission/mission-chat/lifecycle/
randomize checks and original UI Vulkan pixel/input comparisons pass on the
coherent Linux identity
`cd3f96f3738847a18b12d8df4a8d40f60954714523380b7f646f909f8b587177`.
Vulkan validation reports no errors. No wire record or periodic report grows;
no WinTrek/FedSrv runtime source was changed in this batch. Portable
Windows/macOS builds were not repeated. Ship visibility remains closed.
Logs are `/tmp/allegiance-lobby-policy-{build,session,network,dialogs,missionchat,pixels,lifecycle,randomize}.log`.

## Automatic-start controls and reconnect lobby parity (2026-10-07)

Compared WinTrek's `TeamScreen::UpdateStatusText` and FedSrv's mission/side
readiness handling with native lobby controls and authenticated reconnect
leases. Automatic-start missions now disable manual Start while retaining the
original ready status. The authority's automatic-start and locked-open policy
also reaches the replica's IGC mission parameters, rather than only its UI.

A disconnected No Team visitor no longer cancels a countdown, blocks faction
readiness or vetoes host settings during reconnect grace. Settings preserve
that visitor's authenticated lease across world replacement, clearing its old
world baselines, requests and readiness without extending the grace deadline.
The client now distinguishes same-match resume, where pilot identity must stay
fixed, from new-match resume, where the rebuilt authority assigns new IDs.

Disconnected faction pilots retain the existing countdown/settings protection.
The authority reports the affected factions individually, so a single missing
faction produces the original named-team "IS NOT READY" status, with unaffected
teams still ready. Several blocked factions use "ONE OR MORE TEAMS" / "ARE NOT
READY". These policy/membership flags use existing lobby flag storage; wire
records and periodic report sizes do not grow.

All 128 automatic-start/six-faction reconnect UI combinations pass. The new
encrypted `--case lobbygrace` regression covers real dormant No Team and faction
connections, countdowns, settings, world replacement, cleared old requests,
authentication, specific ready-side/status reports, rejected faction-grace
settings and identity-preserving same-match resume. Full session/original-dialog
suites, existing encrypted admission/lifecycle checks and original UI Vulkan
pixel/input comparisons pass on coherent Linux identity
`6ea2b6c45b044bb0fefb9ada8389355d77bde6e3304cb307de79785cf2b130db`.
Vulkan validation reports no errors. WinTrek/FedSrv, renderer and platform
backends were unchanged in this batch; portable Windows/macOS builds were not
repeated. Logs are `/tmp/allegiance-lobby-grace-*-final.log`.

## Training-screen re-entry and progression (2026-10-07)

Compared `TrainingScreen` construction, Back, lesson selection and Launch with
native screen navigation. Clicking a lesson without launching, returning to the
main menu and entering training again previously retained the clicked lesson.
True entry now selects the original independent next-lesson progression.
Returning from options, mappings, Credits, Help or a canceled confirmation keeps
the existing lesson selection. Progression remains execution-local, advances
only when launching its current lesson, skips removed TM7 and ends at TM8.

The source-reference regression covers all 49 selected-lesson/progression pairs
through the authored Back button and main-menu welcome, all seven launches,
out-of-order replays, final-lesson repetition, same-screen refresh, popup and
nested mapping returns, confirmation cancellation and a fresh execution.
Full Linux session and original-dialog suites pass. Client, server, directory
and network-check binaries were rebuilt coherently with identity
`c494571c31a8306e94f8cbfdb93512f7de944e70fb83f51baf260bce5c81c7eb`.
This batch changes only native frontend behavior and its reference tests;
WinTrek, simulation, rendering and platform backends were unchanged. Portable
Windows/macOS builds were not repeated. Logs are
`/tmp/allegiance-training-entry-{build,session,dialogs}.log`.

## Complete installed training-artwork comparison (2026-10-07)

Added a separate reference evaluator for the training MDLs' actual primitives:
`ImportImage`, `GroupImage`, `TranslateImage` and `StringImage`. It reads the
authored expressions directly, computes image geometry/layer order independently
of `OriginalUi`, and splits text using the frozen original `BreakLine`. It shares
the asset decoder and final quad renderer with the client; this verifies image
composition and text layout rather than independently revalidating those shared
components. Free Flight uses the same arena assets as TM6.

All thirteen introduction/debrief sequences pass: 75 slides, 109 image layers
(30 keyed overlays), fifteen text blocks and 83 text rows. Sound ordering,
800x600 canvas, unchanged UV/tint/blending and absence of replacement controls
or model previews are checked. All 75 full-canvas Vulkan pixel comparisons also
pass, alongside the existing original UI pixel/input and original-dialog suites.
The long introduction text slide was visually inspected at original resolution.
Vulkan validation reports no errors; the UI window closes normally.

The team/game-setting audit also confirmed native non-Club name/password,
skill/squad gates and pregame authority against `NewGameScreen`, `TeamScreen`
and FedSrv. Account-backed Club features remain in the queue below. Runtime
source was unchanged in this batch. Client/server/directory/network-check were
rebuilt coherently with identity
`9a8c7a7c9ef8aa2a8a3669f0898c85fd60ef9d54c95f79671b1f9cf0d17ee774`;
portable platform builds were not repeated. Logs are
`/tmp/allegiance-training-artwork-{build,dialogs,pixels}.log`.

## Original weapon controls and authenticated pilot context (2026-10-07)

The input audit found functional differences in the original next/previous gun,
explicit gun and group-fire routes. Cycling preserved group mode in the local
HUD but submitted a toggle to the authority. This could ungroup a grouped ship
or group a single-equipped-gun ship on the server while its HUD indicated the
opposite. Explicit same-gun toggles also differed when the client and server's
last physical input frames contained different selected hardpoints.

The client still uses the shared original `BaseClient`/WinTrek selection policy.
Its existing weapon action now carries the chosen bounded hardpoint and group
mode; the authority applies that intent rather than reinterpreting the keypress.
Snapshots restore both fields, including the latent hardpoint retained while
grouped. The group field replaces unused record storage: the snapshot remains
264 bytes, and neither action nor periodic input records grow. There is no new
binding, UI control, protocol version negotiation or custom record serializer.

Accepted selections remain pending until a report's action watermark incorporates
them, because a receipt may precede that report. Reconciliation applies only the
latest unincorporated selection, without resending it or repeating its sound.
Original group/ungroup cues remain local to the initiating client; the authority
no longer broadcasts another copy to that pilot or other observers.

The same audit reproduced a separate authority-context error. Weapon selection
and autopilot could use another pilot's cached docked HUD state after switching
to the authenticated caller. Their guards now query the caller's actual station.
This permits a flying pilot to select weapons and engage/disengage autopilot
while the authority's current view is docked, and retains docked-call restrictions.
Other station-dependent reads in this file were checked: they belong to refreshed
presentation/step consumers, rather than these command authorization paths.

The new integration regression covers 48 single/sparse/group next/previous cases,
actual local/server firing bits, inventory highlights, stale-report replay,
client-ahead same-gun grouping, eight fresh-replica bootstraps, distinct docked/
flying authority contexts and malformed intents/records. Three regressions were
first observed failing: grouped cycling, lagged same-gun grouping and the cached
docked guard. The encrypted `--case weapons` check covers real multi-gun and
single-gun fittings, original toggles, queued bursts, one local cue, absence of
foreign cues, resume, canonical correction and autopilot/selection with a
different docked authority view. It is registered as `native-network-weapons`.

The broader encrypted general-play check had a stale assertion for the former
native "Docked at" narrative. It now checks WinTrek's original private docking
and welcome-home sounds, once per pilot, and absence of that added narrative.
The runtime docking path was unchanged.

Full Linux controls/session/original-dialog suites and encrypted weapons,
general-play, throttle and projectile checks pass on coherent client/server/
directory identity
`e452a3131d4e86157e0dd24890384e1fea100afcf5d26472e1b5e9842abacf17`.
WinTrek/FedSrv, rendering, frame pacing and platform backends were unchanged in
this batch. Portable platform builds and closed ship-rendering diagnostics were
not repeated. Logs are `/tmp/allegiance-weapon-selection-*.log`.

The asset-namespace audit also found that all 954 civilization records across
the 99 installed static cores select `dialog`; it identified no missing alternate
HUD namespace in these assets and did not trigger a speculative HUD rewrite.

## Original loadout presets, station stock and feedback (2026-10-07)

Comparing `LoadoutImpl::OnKey`, `RefreshLoadout`, `BuyHull`, `OnButtonDefault`
and `BaseClient::GetLoadoutLink` exposed three functional differences. The
original digit shortcuts search the selected bank for exactly the editor's hull;
a bank containing only another hull falls back to the selected hull's defaults.
The native shortcut had instead fitted the most recent foreign hull's equipment
onto that hull. The classic editor now uses an exact lookup and a shared frontend
staging route. An existing optional native fitting route retains its separate
most-recent fallback. This adds no keybinding or control.

Restoring an exact cached fitting now follows the original station policy:
upgrade the saved hull through its successor, or choose the first buyable hull
and default equipment when that cached hull has been withdrawn. Upgraded cached
fittings fill preferred mounted equipment without inventing default cargo. A
missing cache still invokes the original Default policy for the selected hull.
The temporary preview remains separate from purchases and the owned ship.

Default no longer duplicates the MDL button's change-loadout cue. Hull changes
now play `BuyHull`'s original mount cue; Next/Previous additionally retain their
authored change-loadout click, while choosing the current hull remains silent.
The added hull-cycle assertion reproduced the missing mount cue before the fix.

The original-dialog suite passes 80 cases across all ten banks, two hulls,
empty/foreign/matching banks and both cache orders, plus real IGC successor and
stock-withdrawal fixtures, invalid-bank rejection and actual Default/hull-menu
sound counts. Its existing 1,599 hull menus and 3,198 traversals now also check
the original cue counts. Browsing preserves the live fitting, credits, simulation
random state and mission membership. Full Linux loadout, session, controls and
original-dialog checks pass. The encrypted purchase check also passes quantities,
causal completion, rejection, stock withdrawal, reconnect, atomic launch,
docking/refit, fatal recovery and teleport replacement.

Client, server, directory and network-check were rebuilt coherently with identity
`0c5d2d8fa741ad87c0fcc22f44753403202e7d1b5f3327a366ee89d69c8fa3f1`.
Logs are `/tmp/allegiance-loadout-parity-*.log`; the failing regression is
`/tmp/allegiance-loadout-feedback-red.log`. This batch changes no WinTrek/FedSrv
runtime code, wire records, rendering, pacing or platform backends.

## Original investment feedback and research-card surfaces (2026-10-07)

The audit of `BaseClient::AddMoneyToBucket` and
`WinTrek/loadout.cpp::PartListItem` found three differences: network investment
clicks waited for a report before changing the wallet/bar, model cards used an
extra black color key, and long titles were painted underneath their prices.

Investment clicks now apply the original bounded IGC payment immediately and
send the actual amount spent. Failed sends leave both wallet and bucket intact.
Canonical reports replace the prediction, then replay only outstanding payments
in order, without sending again or advancing production. Pending payments retain
their faction context. Shared faction captures can precede private action
receipts, so the server report acknowledgement waits until its referenced
faction capture includes the payment. This retains shared capture/encoding and
requires no wire-record growth. The regression reproduced the missing immediate
feedback before the fix.

Model cards retain their authored alpha, matching `LoadImage(false)`; missing
cards, selection highlights and brackets retain their original color-key policy.
Price/status text now precedes the title on the row surface, matching original
overlap behavior. The artwork audit passes all 246 distinct cards across
`static_core` and `PCore014`, including 28 cards with distinct black-key pixels,
plus original text order, anchors and fallback behavior. Settled-pane Vulkan
pixel comparisons pass on Wayland with validation enabled.

Headless economy, controls, session, loadout and original-dialog checks pass.
The new funding checks cover amount boundaries, failed sends, limited-wallet
bursts, repeated stale reports, faction context and production isolation. The
encrypted investment check covers concurrent funding, silent correction,
staggered faction/private captures, burst conservation and pending reconnects;
encrypted weapon-selection and purchase regressions also pass.

Linux client, server, directory and network-check were rebuilt together with
identity `dcd556f0e3ec2427aa3d24f6dc833711b42f620903164305a8dd37f4b3833ffa`.
Logs are `/tmp/allegiance-investment-feedback-*.log`; the failing regression is
`/tmp/allegiance-investment-prediction-red.log`. This batch changes no
WinTrek/FedSrv runtime code or renderer/platform/pacing behavior. Immediate
cash-donation and income-donation feedback remain a separate audit item.

## Remaining execution queue

This is an incomplete restoration, not a claim of full original UI equivalence.

| Priority | Original surface | Remaining work / completion criterion |
|---|---|---|
| 1 | Loadout (`loadoutpane`, `partinfo`, `hullinfo`, `WinTrek/load.cpp`) | Staged browsing, original IGC purchase/launch, quantities, partial-purchase feedback, hull/cargo cascades, original placement/prices, optional per-hull art, turret labels and original per-hull preset lookup are implemented. The original slot handler opens a menu on either mouse button's down event; it does not implement drag/drop. Remembered fitting, owned/unavailable equipment, lost-slot restoration, options-popup preservation, automatic docking/fatal recovery, initial-mission custom-zero replacement, teleport replacement and disembark callbacks are implemented and checked against original policies. Direct offline station changes also trigger deferred restoration. Current-station disembark and atomic released-seat reboarding preserve original fitting, remaining crew and first-free turret selection. Original carrier/allied rearm distinctions, encrypted solo/crewed allied service, non-dismissible pending purchase input, stock withdrawal after a queued request and immediate station-context preview retirement are implemented and checked. The recovery matrix now covers lost/captured last bases, exhausted lives, captain/two-gunner/observer loss and base evacuation. Complete all faction/hull/part visual comparisons and remaining allied-base/crew combinations beyond that matrix. |
| 2 | Lists and popups (`efpane.cpp`, `inputmappane`, `teamscreen`) | Input, roster, games, chat and overflowing part-menu scrollbars, full list access, selection/clipping and mapping transactions are implemented. Lobby/chat channel histories, wrapping, mapped paging, unread flashes and read-position/draft retention are restored. Stable lobby IDs, original leader/mute/Boot/applicant player popups, role columns, admission controls and team-settings-lock gates are implemented; complete moderator actions with their authority/session support. Flight F6/mini-team panes, closed-pane sighting/retirement history, compact-pane dock-duration text, turret swap/promotion shortcuts and separate Chat/Dock/Launch menus are restored. Flight Boot confirmation, defection-dependent mission/side removal, all-faction bans, original notifications, reconnect retirement and occupied-crew preservation are implemented. Server-side faction history bootstrap, human death/restart ordering, unobserved drone retirement and the authoritative detected/stealth indicator are implemented. Compact allied credits now retain observed signed donations with per-subscriber baselines and account generations; expanded credits remain own-only. Compare further allied, observer and exhausted-life status combinations; money-request/order display and original console-tip feedback are restored. Original F6 header values/order, blank and double-click selection, list keys, team-tab down actions and refresh-safe pointer capture are restored and checked. Original authored and directly constructed control sounds, disabled hover/capture and original left-repeat/right-single donation events are restored and checked. Full-scale faction/No Team sort, clipping, reachability and live selection/context retirement are checked, with seven independent Vulkan roster comparisons and expanded/compact F6 clip comparisons. Further crew/status visual combinations remain. Original votes, prompts, mapped Y/N, mutiny/resign/draw authority, next-round reset, `#autoaccept`, `#autobalance`, `#list`, No Team-only owner `#ban`, immediate/single typed echoes and local-only individual self chat are implemented; complete authenticated Steam moderator context-menu actions and persistent account policy. The published `CommandFinishTM` binding has no WinTrek handler; the separate debug Skip Goal branch is explicitly NYI. Preserve this reserved behavior rather than inventing a lesson skip. View Base now dispatches the original command and camera fallback. Mapping backdrops retain the original popup parent and live flight view; cached foreground boundaries cover the restored popup hosts as they are restored. |
| 3 | Configuration (`WinTrek::ShowOptionsMenu`, menu.cpp) | Original options/Escape popup navigation, global mapped main-menu priority above hangar/investment, immediate application, supported visual/input/sound/chat controls, default chase preference, independent debris density and five original smoke/performance states are implemented. Chat line/font/lobby filter options are restored. Authored HUD variants and their command/status/panel/crew surfaces are restored. Inventory row composition/statistics now use core-selected MDLs and shared original IGC policies. Original brightness controls, startup gamma/preference import, actual native graphics-device state, force-feedback auto-centering, supported antialiasing and original texture-limit cycling are implemented. Complete further visual comparisons and physical force-feedback/platform-GPU validation; retain the present-time pacing policy. DirectSound restart/hardware controls have no direct native equivalent. Native-resolution and paced-presentation status rows remain separate from D3D-exclusive mode controls. |
| 4 | Game/team setup (`newgamescreen`, `teamscreen`) | The twenty-one mission combos, presets, supported checkboxes, staged accept/cancel, host authority, generated-map preview, native admission/defections/count-imbalance policy, original pre-start statuses/timers, Flush/Enforce Balance and generated faction briefing/countdown are implemented. Maximum-length actual mission creation, stable core faction identity, Team Ready/AFK parity, stale-session cleanup, accepted team-name prompts, briefing cancellation, public offline human rosters and rendered original local entry are implemented and checked. Original catalog/file-backed previews, map/team validation, generated-map restoration, custom-map briefing names and pending remote configuration are implemented and checked. Original goal/admin validation, inactive-team reduction/cancel, authored empty-team activation with owner/occupancy/world/stage authority, alliance cleanup, start removal and next-round restoration, fixed-world Settings/Details gates, actual prepared/authority home sectors, literal server-authored story briefings, shared briefing encoding, reconnect/team changes and load-independent countdown timing are implemented and checked. Original Details/previous-score/request cancellation, ready non-owner commander AFK, Accept/Make Leader selection clearing, draft retention, JOIN/CANCEL wake-up and team-specific voice links are restored and checked. The original conquest owner team-right-click solo/group/Remove/Cancel alliance menu, live in-place authority/replica rules, ready withdrawal, delayed-report immunity, canonical bootstrap and staged-settings preservation are restored and checked. Complete authenticated ranked balance and squad-game team settings. Keep native-only scenario/participant setup reachable through explicit additional actions. |
| 5 | Network entry (`introscreen::ServerDialog`, `gamescreen`) | Original games artwork, real GNS queries of known endpoints, current occupancy/admission/rules, sorting/filtering/scrolling/selection and real joining are restored. Local fixtures are removed from public rows; the original creation dialog offers explicit local creation and real compatible authorities, with authored selection/capacity/core gates. Automatic LAN discovery, the original selectable server list, scrolling, double-click callsign entry, saved identity/admission and stopped-authority expiry are restored. Original Find Player dialog, connected-player exact-name lookup across known/discovered authorities, pending/result feedback, filter reset and selection/reveal are restored. Remote multi-game creation, creator reservation, request idempotence, full-directory player search and mission retirement are implemented and checked. Installed `cores.txt` catalogs, actual selected-core authority/replica state, civilization/equipment controls, friendly names/official badges and separate equipment banks are restored and checked. Public registration, listener verification, shared directory replies, multiple-directory membership, withdrawal/reconnect and original creation/player-search expansion are implemented and checked. Stable mission/authority identities, numeric IPv4/IPv6 alias coalescing, original refresh-safe selections, Find Player alias merging and identity-bound joining/reconnects are implemented and checked. Asynchronous hostname discovery/registration/creation/joining, address fallback, cancellation and identity-preserving reconnect are implemented and checked; provision a deployed community directory/seed address. Native directory/discovery is separate from legacy DirectPlay/Club services. |
| 6 | Help/confirmation/teleport/research | Credits, Help, message boxes, teleport, investment, flight player and inline chat/command consoles are restored. Original cursor artwork/hotspots and training callouts are restored. Original model-card presence/prefix resolution and the two-core decode audit are restored. Complete per-state visual comparisons across card models. Crew panes are restored, including the authored Glass variant. Lobby/postgame quick chat is restored; complete full roster/crew inventory combinations and remaining visual comparisons. Command-camera entrance interpolation, original default sector framing/zoom retention, deferred Take Me To focus after physical reports, remote crew/station focus, pending-focus supersession/retirement and replica camera-input gates are restored and checked. Hangar Command retains the remembered perspective; matching silent transfer failures release pending dialogs without dismissing newer or ordinary failures. The dynamic-host batch removes the identified copied MDL geometry/style values across active hosts. Complete remaining original painter semantics, visual/state combinations and alternate asset coverage; unreachable legacy Help navigation/MML topic widgets are separate from the current keyboard-image Help root. Preserve currently working gameplay actions during replacement. |
| 7 | Full visual/interaction comparison | Compare all screen states, enabled/disabled controls, hover prompts, keyboard focus/shortcuts, training transitions, dock/refit/relaunch, remote lobby and session teardown. Original local-entry diagnostics now cover actual multi-seat game creation, briefing cancellation, hangar roster toggling and quit/cancel. Participant creation remains a test fixture or explicitly optional native surface; the default flow does not add original-game controls that never existed. Complete further alternate-core and full-roster comparisons. |
| 8 | Postgame (`gameoverscreen`, `WinTrek/gameoverscreen.cpp`) | Original artwork, tables, header sorts, scrollbar/chat, Save Stats, original IGC scoring and next-round/Team Lobby flow are restored. Authority-owned automatic start/restart, minimum-player cancellation, shutdown draining/admission, restart permission, results-delay/briefing transitions and ended-mission Back are restored and checked. Original side-change score resets, both command-transfer directions, authenticated reconnect-grace timers, departed-player retention and placeholder exclusion are restored and checked through encrypted reports and the original results table. Complete authenticated persistent ranks/rank multipliers, account-backed reentry after grace expiry and squad-game variants. Compare objective/discovery/repair score events across more real missions; keep the live `/` Game State console separate. |

Window/platform backend validation is recorded separately in
[native-windows.md](native-windows.md) and [native-macos.md](native-macos.md). Windows OS, monitor/input/gamepad testing
and exact presentation latency measurements remain part of the community playtest.
