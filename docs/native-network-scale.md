# Native human-roster network scaling

The target of four factions with 256 pilots each passes the encrypted UDP
evaluation on this Linux machine. Pilots are distributed uniformly across the
original 29-sector large HiHigher map, with active throttle, turning and gunfire.
The simulation runs at 60 Hz and publishes at 10 Hz. Original visibility,
collision, damage, crowded-sector penalties, equipment and IGC movement remain
in effect.

## Results — 2026-10-03

Machine: AMD Ryzen AI MAX+ 395, x86-64, Linux 7.2.7-arch1-1, Release build.
Evaluations run sequentially, without concurrent builds or profiling. GNS uses
normal per-connection encryption and the optional supplied-buffer io_uring
backend. A known-capacity loopback fixture fixes the connection send-rate bounds
at 32 MiB/s; production transport defaults are separate.

| Roster | Original sectors | Duration | Server work p99 / max | Simulation p99 | Missed 60 Hz deadlines | Application down / up |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 + 128 | 4 | 20 s | 3.00 / 3.62 ms | 1.48 ms | 0 | 23.45 / 0.94 MiB/s |
| Four × 64 | 8 | 20 s | 3.14 / 3.55 ms | 1.53 ms | 0 | 15.74 / 0.95 MiB/s |
| Four × 128 | 8 | 20 s | 6.02 / 6.83 ms | 2.66 ms | 0 | 55.73 / 1.87 MiB/s |
| Four × 128 | 16 | 40 s | 5.55 / 5.96 ms | 2.73 ms | 0 | 32.81 / 1.86 MiB/s |
| **Four × 256** | **29** | **40 s** | **9.82 / 11.83 ms** | **5.09 ms** | **0** | **76.00 / 3.57 MiB/s** |

The first three rows precede the final faction-global publication staggering;
they record the requested progression, rather than an identical final-build
scaling comparison. Sector density matters: the eight-sector 512-pilot run
finished with 469 pods because of the original crowding rule. The final
29-sector target retained 994 combat ships and 30 pods after 40 seconds, with
12,057 models at its peak. It is a sustained firing workload, not a measurement
of a mostly dead fleet.

In the target case, 1,023 codec agents exercised independent encrypted sessions,
input delivery, arena decoding and baseline consistency. One additional client
used the real headless `Net::Client`, IGC replica and prediction path, including
F2 switching to another sector and back. All together these are 1,024 connections.
Codec pilots received at least 399 of 400 scheduled reports; the real replica
received 401 including bootstrap. There were no missing delta bases, rejected
messages or input drops. Codec report-gap age p99 was 98.8–100.5 ms and
clock-probed authority age p99 was 93.1–95.7 ms. Real-client step p99 was 3.99 ms.
No renderer was included in this capacity evaluation.

Server work covers polling, simulation, capture and publication admission on
the simulation thread. Compression/encoding runs on the publication worker;
GNS encryption/socket work runs on its service thread. Client freshness and
baseline checks are therefore required alongside the main-thread timings.
Clock-probed authority age uses minimum-RTT clock-offset estimation and has up
to one simulation tick of timing uncertainty. Report-gap age samples time since
the last fully received report; neither metric is input-to-present latency.

Traffic counts include application envelopes/fragments before GNS packet
headers, AEAD overhead and retransmission. Target downstream composition was
5.43 MiB/s strategic map, 26.24 physical state, 40.64 projectile membership and
anchors, and 3.58 private state. Shared arena history occupied 64.64 MiB, and
bootstrap contributed a separate 58.09 MiB. Internet congestion behavior,
physical-NIC throughput and 1,024 rendered clients were not measured.

Machine-readable results are generated locally by `tools/network-scale.py` in
its `--output` directory. Run output is not tracked; reproduction commands are
listed below.

## Shared reports and bounded publication

The authority captures immutable value records for visibility categories:

- Faction-global known map, roster, orders, economy and strategic status.
- Full physical state for a viewed sector and faction. F2 viewing another sector
  also retains the pilot's physical source-sector state for local prediction.
- Sector projectile membership and deterministic trajectory anchors, divided
  into eight stable identity buckets.
- Per-pilot input receipts, private events and notices.

These use original side visibility and cached known observations. Hidden hostile
models are never exported. Remote flying ships carry strategic status, not
precise physical pose, equipment or continuously changing ore/energy/health.
Viewed/source-sector overlays restore full physical state. Known treasure/drop
map information remains available, matching the original server policy.

`ArenaEncoder` encodes canonical state once per domain revision. Healthy peers
share its normal delta; lagging peers share a repair delta for their baseline
cohort. Eight raw revisions and periodic checkpoints bound the history.
Unchanged categories reuse their existing revision. Projectile membership uses
shared chains; its lower-priority delivery does not block required flight state.
Projectiles, treasure motion and static-object spin use analytic anchors rather
than continuous position updates. Changes to captured fields invalidate those
anchors.

`Server::Publisher` owns encoding and dispatch preparation. Admission retries
are bounded to a simulation tick, with one outstanding publication per pilot.
Encoding happens in capture order before admission, so retrying an older job
cannot reverse an arena's revision history. Immutable encoded blocks survive
retries without recapture or re-encoding. Client histories and server baselines
track current subscriptions; sector re-entry receives a self-contained repair.
Startup and fragmented-message storage have explicit byte/count bounds.

Sector publication phases and faction-global capture phases spread the 10 Hz
work across simulation ticks. This preserves each recipient's cadence while
avoiding a simultaneous full-map capture burst. Reducing projectile partitions
from 32 to eight cut identical 29-sector synthetic traffic from 121.08 to
77.62 MiB/s and p99 for simulation with inline publication from 15.97 to 12.25 ms.
The integrated worker architecture produces the lower final critical-path time
shown above; the synthetic timing includes inline compression and is not the
same metric.

The final 512-pilot run uses large HiLo (16 sectors), preserving approximately
the target's per-sector population. It retained 500 combat ships and 12 pods,
with 6,072 peak models. Every codec pilot received all 400 scheduled reports;
report-gap age p99 was 98.7–99.4 ms. The real IGC client received 401 reports,
completed both F2 stages and had step p99 1.83 ms. No baseline failures, rejects
or input drops occurred.

## Simulation structure

The main burst cost was retirement/ejection work, not thread priority. Native
cluster retirement now rejects already-unlinked models before repeating hit-test
retirement and linear membership searches. Treasure IDs and static definition
IDs have direct, non-owning indexes while original owning lists and iteration
order remain intact. Collision hull names use a case-insensitive hash lookup.
Capture indexes maintain stable network-identity order so model swap removal
does not shift an entire record stream and inflate its deltas. Crew notification
routing uses direct source/crew relationships rather than scanning every pilot
on each hit.

These changes retain the original collision and gameplay decisions. Changes in
shared Windows sources are guarded by `ALLEGIANCE_NATIVE`; WinTrek keeps its
existing paths. No Windows performance benchmark is claimed.

## GNS work

The companion changes live in the adjacent `../GameNetworkingSockets` checkout
and are
independent of Allegiance schemas. `SteamNetworkingSharedPayload` retains one
immutable plaintext allocation across independent recipient descriptors. Receive
messages are adopted by the native transport instead of copied into a second
plaintext allocation.

The optional Linux reactor implements multishot `recvmsg` with supplied buffers,
owned vectored `sendmsg`, bounded completion dispatch and cooperative service
fairness. Cancellation and descriptor-reuse tests cover buffer/receiver lifetime.
It defaults off and retains the classic socket/manual-polling fallback. This is
**not hardware zero-copy RX**; NIC ZCRX needs compatible kernel/protocol/hardware
queues and a separate physical-NIC evaluation. Async sends still own copied
packet-builder bytes until completion.

Session AEAD remains per recipient. Shared symmetric authentication would let a
recipient forge authoritative domain traffic. A future optional shared-ciphertext
extension needs server-only origin authentication and membership/key epochs;
its design boundary and reactor contracts are documented in
`docs/linux-udp-reactor.md` in that GNS checkout.
Encryption was not a leading authority-profile cost, so the current target does
not require that security/wire change. No classic-versus-io_uring speedup is
claimed without a controlled comparison.

## Remaining density stress and validation scope

Four × 256 on eight sectors passes simulation deadlines (p99 8.48 ms, max
10.53 ms), but fails the report-freshness criterion: report-gap age p99 reaches
229–380 ms. Original crowding synchronizes large ejection/drop bursts; the
required reliable world lane then delays newer physical state behind those
records. Baselines remain consistent, but the stalls are visible. Keep this case
as a failing stress evaluation. A follow-up should isolate bulky strategic/
creation traffic from current physical-state dependencies while retaining
explicit receipt, creation and event ordering. Simply dropping reliable state
would regress consistency. The crowding gameplay penalty was not changed.

The native Release CTest suite passed its 43 checks after adapting the miner
replication check to inspect the mining sector through command view. Off-sector
strategic reports intentionally omit exact ore/activity state. Native ASan arena,
core, health, throttle and model-lifetime checks and the full encrypted
replication/prediction/construction suite pass. GNS Release crypto,
shared-payload, io_uring, fairness and encrypted connection tests pass; its classic
backend also passes crypto, ownership, fairness and encrypted IPv4/IPv6 connection
checks. GNS ASan ownership/reactor/fairness checks
pass with leak detection. Generated protobuf objects were uninstrumented for
that run: the installed protobuf teardown check crashes even in a minimal
protobuf-only ASan program. That limitation is separate from transport ownership
coverage and is not hidden by claiming a completely sanitized dependency stack.
The native ASan runs instrument the application/IGC objects against Release GNS;
their legacy process-global caches were excluded from leak checking. The focused
GNS sanitizer run uses leak checking.

## Reproduce

Build the companion GNS checkout with `USE_IO_URING=ON` and install its CMake
package, then configure Allegiance's native build against it. The current
machine's package is `/tmp/allegiance-gns-scale-install`.

```sh
cmake --build build/linux-release --target allegiance-network-scale \
  allegiance-network-load allegiance-network-check allegiance-network-arena-check -j8

# Fast synthetic capture/encoding evaluation, no sockets.
build/linux-release/src/native/allegiance-network-scale \
  --teams 4 --pilots 256 --map 9 --large-map --ticks 900 --warmup 60

# Initial four-sector case, then the sustained final target.
python3 tools/network-scale.py --case 2x128 --seconds 20 \
  --output /tmp/allegiance-network-initial
python3 tools/network-scale.py --case 4x128 --map 8 --large-map \
  --replica --seconds 40 --output /tmp/allegiance-network-middle
python3 tools/network-scale.py --case 4x256 --map 9 --large-map \
  --replica --seconds 40 --output /tmp/allegiance-network-target

# Deliberately crowded stress case; freshness currently fails.
python3 tools/network-scale.py --case 4x256 --seconds 20 \
  --output /tmp/allegiance-network-crowded
```

The runner fails on simulation deadline misses, rejected/input-drop counters,
missing arena bases, fewer than 95% of expected reports, or report-gap p99 above
180 ms. `--replica` additionally requires the real IGC client to complete both
F2 stages. All agents are signalled together at the measurement boundary so
sequential teardown cannot inflate the later agents' freshness measurements.
