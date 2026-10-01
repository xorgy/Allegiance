#!/usr/bin/env python3
"""Summarize optional native presentation traces; no third-party modules."""
import argparse
import csv
import statistics

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("trace")
parser.add_argument("--skip-frames", type=int, default=500,
                    help="Discard this many initial frame IDs in each swapchain segment")
args = parser.parse_args()
with open(args.trace, newline="") as stream:
    rows = [{key: int(value) for key, value in row.items()} for row in csv.DictReader(stream)]
rows = [row for row in rows if row["id"] > args.skip_frames]
if not rows:
    raise SystemExit("No presentation feedback after the selected warmup")


def summary(name, samples):
    samples = sorted(samples)
    if not samples:
        return
    def percentile(fraction):
        index = fraction * (len(samples) - 1)
        low = int(index)
        return samples[low] + (samples[min(low + 1, len(samples) - 1)] - samples[low]) * (index - low)
    print(f"{name}: median {statistics.median(samples):.3f}, "
          f"p95 {percentile(.95):.3f}, p99 {percentile(.99):.3f}, "
          f"min {samples[0]:.3f}, max {samples[-1]:.3f} ms")


print(f"{len(rows)} reported presentations; compositor/display feedback, not physical input-to-photon")
summary("Sample to presentation", [(r["displayed_ns"] - r["sampled_ns"]) / 1e6 for r in rows])
summary("Sample to present submission", [(r["submitted_ns"] - r["sampled_ns"]) / 1e6 for r in rows])
summary("Sample to queue completion", [(r["ready_ns"] - r["sampled_ns"]) / 1e6 for r in rows if r["ready_ns"]])
intervals = [(b["displayed_ns"] - a["displayed_ns"]) / 1e6 for a, b in zip(rows, rows[1:])
             if a["segment"] == b["segment"] and b["id"] == a["id"] + 1]
summary("Presentation interval", intervals)
if intervals:
    print(f"Mean presentation rate: {1000 / statistics.mean(intervals):.3f} Hz")
for segment in sorted({r["segment"] for r in rows}):
    part = [r for r in rows if r["segment"] == segment]
    print(f"Segment {segment}: {part[-1]['misses'] - part[0]['misses']} new pacer misses")
