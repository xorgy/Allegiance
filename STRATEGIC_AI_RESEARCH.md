# Allegiance strategic AI research

Research recorded on 1 October 2026 for the native Allegiance port. This document
summarizes the original command structure, team operations, visibility and
communication, and the proposed basis for competent AI teams that a human can
join as a pilot or commander. The target is nine to twelve simultaneous teams
with roughly 100–200 AI pilots per team, while preserving human limits on
attention, observation, memory, communication and flight skill.

The historical evidence includes a 2004 player guide, community training and
reference material, and the maintained original client/server code. It establishes
roles and mechanics, rather than a statistical account of peak-era matches.
Public games and organized squads differed; cores and balance changed over time.
The operational sequences and AI architecture below are identified as synthesis
or proposals. A complete strategic AI has not yet been implemented by this work.

**Command structure**

There was one official team leader per team: two in a typical two-team match,
and up to six under the original six-team limit. A roster of 100 pilots did not
create additional official command slots. The server stores one leader identity
per side in [fsmission.cpp](src/FedSrv/fsmission.cpp).

Several people could help command. Command view is available to ordinary pilots,
and tactical orders to human recipients are requests they can accept or reject.
Practical leadership therefore extends beyond the official team-leader flag.
The [2004 player guide](https://www.supercheats.com/pc/walkthroughs/allegiance-walkthrough01.txt)
explicitly separates strategic command, investment and tactical command.

| Role | Responsibility |
| --- | --- |
| Team leader or strategic commander | Choose the overall plan, allocate pilots, prioritize expansion and defense, coordinate attacks, and exercise team-management permissions. |
| Investor | Manage donated income and fund research, constructors, miners and other purchases. The strategic commander can also fill this role. |
| Tactical commander or assistant | Watch a front or operation, organize participants, request reinforcements, and relay developments. |
| Mission or ship leader | Direct a particular strike, capture attempt, defense group or crew, and make immediate decisions during that operation. |

Investment is not inherently restricted to the team leader: the server allows
players to fund eligible team projects with their own money. Pooling donations
gives an investor practical control of spending. See the project-funding handler
in [FedSrv.CPP](src/FedSrv/FedSrv.CPP).

Wings are flat assignment and communication groups. The code defines ten:
command, attack, defend, escort, search, alpha, bravo, charlie, delta and echo.
Multiple people can belong to the command wing without becoming additional
official leaders. Membership and team-leader authority are separate mechanisms.
See [the wing names](src/Igc/common.cpp) and [wing assignment](src/FedSrv/FedSrv.CPP).

Historically, a squad also meant a persistent player organization, similar to a
clan. Its shared practice and trust are distinct from the temporary flight formed
for one operation. The community's [squad description](https://www.freeallegiance.org/FAW/index.php/Squads)
emphasizes cohesion and teamwork. There is no evidence here for a universal
fixed squadron size or formal hierarchy used by every full server.

**Available information and notifications**

Ships, stations and probes contribute to the team's shared scanner picture.
Human scouts do not have to manually report each contact before it becomes
available to teammates. This automatic sharing is part of the human game and
belongs in the AI's permitted information model. The
[official game description](https://store.steampowered.com/app/700480/Microsoft_Allegiance/)
describes this visibility system.

Available information is distinct from information a player has examined. The
sector map provides an overview; command view, selections, roster and research
panes provide detail where the player directs attention. A commander cannot
continuously inspect every sector in detail.

The original warning and event paths expose the following categories:

| Information | Notifications or interface information |
| --- | --- |
| Discovery | Newly discovered stations, asteroid types and aleph connections, with sector information. |
| Routine contacts | Enemy fighters, miners, builders and conflict in a sector. Command-wing members receive these lower-priority sector warnings too. |
| Vulnerable assets | Miners, constructors and carriers at risk. |
| Strategic threats | Detected bombers, carriers, capital ships, assault ships and transports; station bombing, teleport and capture threats. |
| Progress and outcomes | Research completion, ship losses, station destruction or capture, and sectors secured or lost. |
| Human messages | Orders, acknowledgments, sightings, intentions, repair requests and requests for assistance. |

The [sector threat classifier](src/Igc/common.cpp) selects a prioritized category
from sector asset flags. The [warning delivery policy](src/ClientCore/ClusterWarnings.h)
issues notifications as that category changes. A miner-at-risk warning can mean
a detected enemy fighter shares its sector; it does not necessarily mean the
miner has taken damage. Actual damage-related sounds and reports are additional
signals. Discovery messages are given greater prominence for command-wing
members in [appmsg.cpp](src/clintlib/appmsg.cpp).

Visibility distinguishes current observation from retained discovery. Known
predictable infrastructure can remain known after sensor coverage disappears.
An enemy ship's last-seen sector or hull report need not describe its current
state. See [visibility policy](src/ClientCore/Visibility.h) and
[player status](src/clintlib/clintlib.h).

**Missions and temporary groups**

The following sequences are a synthesis of documented roles and game mechanics.
The suggested compositions illustrate complementary jobs; they are not fixed
historical establishments.

| Operation | Participants and execution |
| --- | --- |
| Exploration and reconnaissance | Scouts discover connections, resources and infrastructure. Continued reconnaissance watches enemy approaches and preparations after the initial map is explored. |
| Sensor coverage and denial | Establish useful probe coverage, maintain it, and remove enemy surveillance. Coverage supports early warning; denial can conceal an attack or concentration. |
| Miner and constructor hunting | Small raiding groups attack vulnerable economic assets and delay income or expansion. Their objective remains economic pressure rather than unrelated dogfighting. |
| Expansion protection | Escort a constructor through threatened sectors, secure its destination and protect construction until usable infrastructure exists. |
| Outpost capture | Coordinate shield suppression, a troop transport's approach and protection of the capture attempt. Follow capture with repair, defense and restored sensor coverage. |
| Base bombing | Assemble the bomber crew, escorts and support; establish route and timing; protect the bomber long enough to inflict decisive station damage. |
| Capital-ship operation | Combine pilot and gunners with repair support, reconnaissance and fighter screening. Choose positions that serve an objective and allow reinforcement and retreat. |
| Interception and defense | Stop the dangerous component of an attack, especially its bomber or transport, while preserving the defended station, constructor or miner. |
| Reinforcement and recovery | Return pilots and ships to useful positions, rescue pods when worthwhile, replenish support and restore the force after an operation. |

Capture and destruction use different capabilities. Boarding requires the
configured weakened-shield condition and entry into the station garage in
[shipIGC.cpp](src/Igc/shipIGC.cpp). Heavy transport EMP, specialized capture
weapons, anti-base missiles, seismic weapons and aleph resonators create different
operational possibilities depending on core and technology. The community
[missile reference](https://www.freeallegiance.org/FAW/index.php/Missiles)
documents examples; its balance values belong to its stated core and must not be
assumed universal.

A roster of 100 pilots is not a fleet of 100 independently maneuvering ships.
Commanders, bomber gunners and capital-ship crew also occupy player positions.
Crew assignment therefore competes with other uses of the team's pilots.

**Match strategy**

The following strategic interpretation follows from the economy, visibility,
movement and station mechanics. Its relative priorities depend on map, core,
faction and player competence.

An opening needs exploration, income protection and an expansion/research plan.
A discovered resource or connection creates an operational question: can the
team exploit it, protect the constructor, and support the resulting base?
Expansion spends money and attention as well as occupying map space.

Recurring strategic approaches include:

- Economic pressure: hunt miners and constructors, disrupt resource access, and
  make the opponent commit pilots to protection.
- A technology window: exploit a useful capability before the opponent develops
  or organizes an effective response.
- Map and reinforcement advantage: establish infrastructure that improves
  reinforcement and makes opposing approaches easier to monitor or defend.
- Concentration and diversion: assemble enough strength for one meaningful
  objective while pressure elsewhere occupies defenders.
- Defensive attrition: survive efficiently, preserve income and counterattack
  while the opposing force recovers.

In a conquest match, fighting must serve station capture/destruction and eventual
elimination. A group can win a dogfight while its team loses a critical
constructor or bomber. Escorts can accomplish their mission despite ship losses
if the protected strike succeeds. Other configured victory conditions require
their own mission priorities rather than assuming conquest is universal.

**Communication**

The built-in facilities include typed chat, canned voice messages and orders.
Recipients include team, wing, individual, ship crew, sector and selected group;
the channels are defined in [IGC](src/Igc/igc.h). Crew communication and local
tactical traffic can be separated from team-wide coordination.

Organized community play also used external voice communication. Later training
material documents TeamSpeak/Mumble push-to-talk bindings in its
[controls guide](https://freeallegiance.org/FAW/index.php/Cadet_I/Remapping_controls).
This does not establish which software was universal at the original release.

Useful reports communicate interpretation, readiness or intent that radar alone
does not convey. Illustrative messages include:

- Bomber entering sector X; need interception.
- Constructor route clear; escort ready.
- Attack group assembling at this base; wait for transport.
- Capture complete; need repairs and probes.

These are proposed examples, not quotations from a historical match transcript.
Coordination requires acknowledgment, staging and local initiative, rather than
continuous individualized instructions for every maneuver.

**Proposed AI organization and fairness requirements**

Use a strategic authority for each team, mission leaders for local operations,
and pilots or crew members executing particular jobs. A human commander replaces
the strategic authority while retaining competent subordinates. A human pilot
joins an operation with an understandable objective and teammates who cooperate.

Keep four stages distinct: team information available, information examined by
this actor, remembered assessment, and communicated interpretation. Shared sensor
data is legitimate automatic information sharing. Detailed interpretation still
requires attention to a view, panel, selection or report that a human could use.

Each actor needs an attention driver that accounts for looking at a sector or
panel, inspecting contacts, reading messages, making decisions and changing focus.
Flying, fighting, commanding and communicating compete for that budget. Attention
rates and reaction limits need calibration from play and recordings; this
research does not establish numerical human limits.

Memory should preserve discovered topology and infrastructure while allowing
enemy location, composition and intent estimates to become uncertain with age.
Retaining a last observation must not silently update it from authoritative
world state. Hypotheses about unobserved movement should retain their provenance
and uncertainty.

Communication should use actual recipient groups and consume time to send,
receive and interpret. A group order can legitimately address many pilots;
individualized instructions cost much more attention. Automatic scanner sharing
and explicit reports are separate mechanisms. Giving every actor unrestricted
access to every other actor's current analysis would defeat these limits.

Human orders should retain precedence within their scope until completed,
withdrawn or invalidated. Local initiative should help execute the order.
Necessary exceptions, such as an unavailable target or immediate survival action,
should produce an explanation or report rather than repeated unexplained
contradiction. Command authority and outstanding commitments need explicit
ownership when a human joins, leaves or changes seats.

Perception and flight control must use permitted observations and bounded human
reaction and aiming capabilities. Server ownership alone does not make an AI
fair: authoritative positions, hidden targets and future collision outcomes must
not enter its strategic or dogfighting decisions as privileged knowledge.

**Proposed efficient implementation and validation**

Produce semantic observations through the same visibility and presentation
rules as human views. This can represent a virtual screen without rendering a
separate framebuffer for every AI. Geometry, range, projection, readable detail
and the chosen view still constrain what an actor can examine.

Maintain persistent missions with an owner, participants, objective, staging and
readiness, success/failure conditions, commitments and reports. Those records
provide a stable unit for planning, delegation, debugging and narrative demos.
They should record what each decision was based on so that privileged-information
leaks and unreasonable attention can be audited.

Run strategic and mission decisions on slower, staggered schedules and relevant
events. Flight control runs more frequently, with its own perception/reaction
limits. Reuse per-side visibility and spatial summaries, bound queues and
histories, and avoid each pilot scanning the whole mission every frame.

Nine to twelve teams imply roughly 900–2,400 pilots at the requested roster
sizes, with a small number of strategic authorities and mission leaders.
Expanding beyond six sides requires auditing arrays, masks, alliance
representations, mission parameters, presentation and eventual protocol fields;
changing the maximum constant alone is insufficient. Existing ship identity
capacity does not by itself establish performance at this scale.

Validation should cover objective completion, cooperation with human command
inputs, honest information provenance, realistic attention and communication,
bounded combat skill, and sustained CPU/memory cost. Benchmark representative
expansion, reconnaissance, raids, defended captures, bombing, crewed capital ships
and multiple fronts. Dense practice wingman battles do not establish competence
at winning a complete strategic match.
