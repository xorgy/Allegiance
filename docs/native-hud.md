# Native HUD implementation and parity notes

The visual baseline is the installed `Artwork/dialog.mdl` default CortUI Classic
(style 0), not the original D3D rendering architecture. The MDL file is a reference
for artwork selection, gauge cropping, relative placement and visible states.
`hud.cpp` composes this skin directly, with a Vulkan instanced quad stream and a
small 3D viewport. There is no legacy UI evaluator, software text surface or D3D
compatibility layer.

## Functional coverage

| Original area | Native offline implementation | Remaining parity |
| --- | --- | --- |
| Center gauges | Six curved gauges, damage flashes, speed, signature, experience, state icons | Additional station/command modes |
| Target display | Ship/station information, original gauge art, current-aspect 3D model, lead/range/lock/reload | Remote-sector targets and queued/accepted multiplayer orders |
| Peripheral radar | Original per-type detail modes, relay-aware weapon lead, damage-bucket threats, angular selection/lock brackets, objective carriers, name/range and hull/shield | Proximity/priority overlap avoidance and complete command-specific order symbols |
| Inventory | Actual mounted equipment, selected guns, ammunition, missile progress; working gun selection, cloak, chaff/dispenser controls | Drag gestures and complete original inventory status presentation; menus support cargo swaps/drops/manual reload, purchases and persistent loadout banks |
| Sector map | Actual sector nodes, links, current sector and ship/station counts in classic artwork | Expand/pan and full commander interactions; visibility filters the available sectors and contacts |
| Communications | Local simulation notifications and bounded event log | Team/global/private chat, voice and networking |
| Other panels | Native help/death/dock overlays, local contacts, startup/scenario/lobby/settings/pause/results screens and hull/equipment hangar | Full team/teleport and narrated training flows; research, remote orders and team scores now have native screens |

The status values come from IGC, including normalized capacity, lock, current
simulation-time reload, experience multiplier and target orientation. UI actions
return typed commands to the native frontend; input is captured for flight and
released for cursor interaction. A click on an overlay control does not also fire
a gun. UI framebuffer coordinates account for SDL logical-window scaling.

Inventory rows reserve separate shortcut, name and status columns. Mounting and
arming use a 64-by-14 logical-unit progress box within the 18-unit row, replacing
the status while busy; missile lock remains in the target display. The progress
fill and label are native geometry and font coverage, not baked bitmap text.

## Rendering and resolution

A `HudQuad` contains NDC bounds, texture coordinates, tint and texture/operation
bits. The pipeline uses premultiplied output with additive sprites encoded as
zero destination-attenuating alpha. Texture selection stays bindless and ordering
is preserved. Lines are single procedural quads, not a chain of point sprites.

The target model is inserted at a recorded offset in the UI stream. Background
quads precede it; foreground gauge and label quads follow it. Scissor/viewport
restrict the model to its panel, and only that depth rectangle is cleared. All
rendering uses the current frame's resident buffers and the existing dynamic
rendering pass.

Layout is anchored to screen edges/center in logical units. A user scale is
limited by available width/height; circles keep their aspect ratio. Text uses
Fontconfig-selected sans-serif regular/bold faces and FreeType coverage at the
actual pixel size. Coverage is stored as linear R8 (2 MiB), with white RGB
provided by the texture view swizzle. Compact cockpit/inventory labels use
separate 11-unit regular/bold sizes, while help and larger panel text retain
their own sizes. Bounded labels align by visible capital height and truncate
before horizontal alignment. Only the final physical font heights are rounded,
so font and artwork scaling share the same layout scale.
The fixed-size atlas is replaced in place when those pixel heights change;
resizing does not accumulate descriptors, font caches or GPU images. Bitmap art
retains its original resolution and therefore still softens when enlarged.

## Route to a modern skin

Keep the snapshot/command boundary and layout units. Replace decorative artwork
and clipped gauges with analytic arcs, strokes and fills in the native UI shader;
retain textures for artwork that benefits from them. Move panel constants into a
native skin/layout description when a second layout is introduced. Add explicit
minimum sizes, grouping, collapse and accessibility options as panels grow.

The current font path is already independent of the old bitmap font. Add
HarfBuzz shaping and a bounded glyph cache for broader localization. Evaluate
MSDF only where arbitrary zoom/rotation or reuse across many scales justifies
it; fixed-size cockpit text currently gets precise hinted coverage without
per-frame rasterization. New panels can share the same instance stream without
restoring the old image graph.

A complete replacement interface still requires the gameplay/services listed in
the table. See [standalone frontend coverage](native-frontend.md) for menu/session behavior.
The current skin provides a running flight reference while those gaps
are implemented, rather than claiming the whole legacy client is complete.

## Weapon indicators and peripheral radar

`ClientCore/WeaponIndicator.h` shares original healing/damage target allegiance and
lead availability from hull, turret, own-side relay station or relay ship. Allied
relays retain the original behavior: they do not confer lead. Cockpit projection
uses an aim direction; chase/missile view uses a projected ship aiming point.
`AimIndicator.h` shares hit-color curves and angular sizing. Zero-radius and
perpendicular external-aim cases return a finite value. Losing the weapon or
target clears in-range state immediately.

`RadarPresentation.h` now supplies original per-object mode rules to both clients.
Target mode keeps nearby hazards, relevant enemies, orders and objective carriers;
Default suppresses unneeded ranges without hiding every neutral object. Treasure
usefulness comes from original technology, launcher compatibility, gun/ammo and
fuel/afterburner rules. Native historical stations retain their observed pedestal
capability. Threat marks/flash are tied to the local source ship's actual damage
history, not an enemy's private current order. Native authority has multiple damage
trackers, so buckets are explicitly matched to the local victim.

Selection and lock brackets use the original angular radius, clamp and four-corner
geometry; partial lock contracts toward the target. Core radar icons preload at
session setup. Tests compare frozen original relay/weapon policy and 122,880 radar
mode/type/range/importance cases, then exercise live IGC contact metadata and damage.
The 1,100-frame Vulkan combat/HUD capture was inspected with validation clean. One
165-Hz deadline was missed during the scripted resize/font-scale transition; steady
flight had none in that run. This does not establish Windows build/performance or
full HUD equivalence.
