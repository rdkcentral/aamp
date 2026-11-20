#!/usr/bin/env python3
"""Aggregate nmqe-player run summaries across repeated runs.

Usage: tools/summarize_runs.py runs/*.jsonl [--csv out.csv]

For each primary measure it prints the sample count, median, p10/p90, mean and
a bootstrap 95% confidence interval of the median. Runs without the measure
(e.g. stable live edge never reached) are counted as missing, not as zero.
"""

import argparse
import csv
import json
import random
import statistics

MEASURES = [
    "tune_to_first_frame_ms",
    "tune_to_stable_playback_ms",
    "tune_to_stable_live_edge_ms",
    "post_tune_catch_up_ms",
    "stalls",
    "stall_total_ms",
    "time_weighted_live_edge_ms",
    "max_live_edge_ms",
    ("live_edge_ms", "p50"),
    ("steady_live_edge_ms", "p50"),
    ("receive_to_present_ms", "p50"),
    ("av_offset_sink_input_ms", "p50"),
]


def load(paths):
    runs = []
    for path in paths:
        with open(path) as f:
            for line in f:
                try:
                    event = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if event.get("cat") == "summary" and event.get("ev") == "run":
                    event["_file"] = path
                    runs.append(event)
    return runs


def value(run, measure):
    if isinstance(measure, tuple):
        dist = run.get(measure[0]) or {}
        v = dist.get(measure[1]) if dist.get("n") else None
        return None if v is None else float(v)
    v = run.get(measure)
    # Scalar measures use -1 for "not reached".
    return None if v is None or v < 0 else float(v)


def bootstrap_median_ci(values, rounds=2000, seed=1):
    generator = random.Random(seed)
    medians = sorted(statistics.median(generator.choices(values, k=len(values))) for _ in range(rounds))
    return medians[int(0.025 * rounds)], medians[int(0.975 * rounds) - 1]


def quantile(values, q):
    s = sorted(values)
    pos = q * (len(s) - 1)
    lo = int(pos)
    hi = min(lo + 1, len(s) - 1)
    return s[lo] + (s[hi] - s[lo]) * (pos - lo)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("files", nargs="+")
    parser.add_argument("--csv")
    args = parser.parse_args()

    runs = load(args.files)
    print(f"{len(runs)} runs")
    rows = []
    for measure in MEASURES:
        name = measure if isinstance(measure, str) else f"{measure[0]}.{measure[1]}"
        values = [v for v in (value(r, measure) for r in runs) if v is not None]
        missing = len(runs) - len(values)
        if not values:
            print(f"{name:40s} n=0 missing={missing}")
            continue
        lo, hi = bootstrap_median_ci(values) if len(values) > 1 else (values[0], values[0])
        row = {
            "measure": name, "n": len(values), "missing": missing,
            "median": statistics.median(values), "p10": quantile(values, 0.1), "p90": quantile(values, 0.9),
            "mean": statistics.fmean(values), "median_ci95_lo": lo, "median_ci95_hi": hi,
        }
        rows.append(row)
        print(f"{name:40s} n={row['n']:3d} missing={missing:2d} median={row['median']:9.1f} "
              f"p10={row['p10']:9.1f} p90={row['p90']:9.1f} CI95=[{lo:.1f}, {hi:.1f}]")
    if args.csv:
        with open(args.csv, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()) if rows else ["measure"])
            writer.writeheader()
            writer.writerows(rows)


if __name__ == "__main__":
    main()
