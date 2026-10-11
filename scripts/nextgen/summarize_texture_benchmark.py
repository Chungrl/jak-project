#!/usr/bin/env python3
"""Summarize a selected PresentMon stream and optional capture-window VRAM CSV.

Standard-library only; does not capture telemetry or access game/pack assets.
Use PresentMon --v1_metrics for the default MsBetweenPresents column.
"""

import argparse
import csv
import json
import math
from pathlib import Path
import statistics


def read_csv(path, required):
    with Path(path).open(newline="", encoding="utf-8-sig") as source:
        reader = csv.DictReader(source)
        fields = reader.fieldnames or []
        if len(set(fields)) != len(fields):
            raise ValueError(f"{path}: duplicate column names; export unique headers")
        missing = set(required) - set(fields)
        if missing:
            raise ValueError(f"{path}: missing columns: {', '.join(sorted(missing))}")
        rows = list(reader)
        if any(None in row for row in rows):
            raise ValueError(f"{path}: extra CSV fields; use comma separators and dot decimals")
        return rows


def number(value, context):
    try:
        result = float(value)
    except (TypeError, ValueError) as error:
        raise ValueError(f"{context}: expected a number, got {value!r}") from error
    if not math.isfinite(result) or result < 0:
        raise ValueError(f"{context}: expected a finite nonnegative number")
    return result


def percentile(ordered, fraction):
    """Linear interpolation at (n - 1) * fraction, without rounding inputs."""
    position = (len(ordered) - 1) * fraction
    low, high = math.floor(position), math.ceil(position)
    return ordered[low] + (ordered[high] - ordered[low]) * (position - low)


def summarize_frames(path, process_id, swapchain=None):
    rows = read_csv(path, ["ProcessID", "SwapChainAddress", "MsBetweenPresents"])
    selected = [row for row in rows if row["ProcessID"] == str(process_id)]
    if swapchain is not None:
        selected = [row for row in selected if row["SwapChainAddress"] == swapchain]
    if not selected:
        raise ValueError("No frames match the requested process/stream")
    chains = {row["SwapChainAddress"] for row in selected}
    if len(chains) != 1:
        raise ValueError("Multiple swap chains; select one with --swapchain")
    values = [number(row["MsBetweenPresents"], f"frame {index + 1}")
              for index, row in enumerate(selected)]
    # PresentMon can emit an initial zero when no previous present is available.
    initial_zero = values[0] == 0
    if initial_zero:
        values = values[1:]
    if not values or any(value == 0 for value in values):
        raise ValueError("No positive frame intervals, or zero after the initial frame")
    ordered = sorted(values)
    tail_count = math.ceil(len(values) * 0.01)
    mean = statistics.mean(values)
    return {
        "process_id": process_id,
        "swapchain": next(iter(chains)),
        "metric": "MsBetweenPresents (presentation interval, not GPU render time)",
        "frames": len(values),
        "excluded_rows": len(rows) - len(selected),
        "initial_zero_skipped": initial_zero,
        "interval_total_s": sum(values) / 1000,
        "mean_ms": mean,
        "median_ms": statistics.median(values),
        "p95_ms": percentile(ordered, 0.95),
        "p99_ms": percentile(ordered, 0.99),
        "max_ms": ordered[-1],
        "mean_fps": 1000 / mean,
        "one_percent_low_fps": 1000 / statistics.mean(ordered[-tail_count:]),
        "over_16_667_ms": sum(value > 16.667 for value in values),
        "over_33_333_ms": sum(value > 33.333 for value in values),
        "over_50_ms": sum(value > 50 for value in values),
        "percentile_method": "linear interpolation at (n - 1) * p",
        "one_percent_low_method": "1000 / mean(slowest ceil(n * 0.01) intervals)",
    }


def summarize_vram(path, column, unit):
    rows = read_csv(path, [column])
    values = [number(row[column], f"VRAM sample {index + 1}") for index, row in enumerate(rows)]
    if not values:
        raise ValueError("VRAM CSV has no samples")
    factor = {"MiB": 1, "MB": 1000000 / 1048576, "GiB": 1024, "bytes": 1 / 1048576}[unit]
    return {
        "column": column,
        "input_unit": unit,
        "samples": len(values),
        "median_mib": statistics.median(values) * factor,
        "peak_mib": max(values) * factor,
        "scope": "user-selected sensor; allocation is not proof of physical residency",
        "window": "caller must restrict samples to the same capture window",
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frames_csv", type=Path)
    parser.add_argument("--process-id", required=True, type=int)
    parser.add_argument("--swapchain", help="Exact SwapChainAddress if multiple streams exist")
    parser.add_argument("--label", required=True, help="For example baseline-spargus-warm-r1")
    parser.add_argument("--vram-csv", type=Path)
    parser.add_argument("--vram-column", help="Exact unique sensor header")
    parser.add_argument("--vram-unit", choices=["MiB", "MB", "GiB", "bytes"])
    parser.add_argument("--output", type=Path, help="Write UTF-8 JSON instead of stdout")
    args = parser.parse_args(argv)
    if args.process_id <= 0:
        parser.error("--process-id must be positive")
    if any([args.vram_csv, args.vram_column, args.vram_unit]) and not all(
            [args.vram_csv, args.vram_column, args.vram_unit]):
        parser.error("Specify --vram-csv, --vram-column and --vram-unit together")
    try:
        result = {"label": args.label, "frames": summarize_frames(
            args.frames_csv, args.process_id, args.swapchain)}
        if args.vram_csv:
            result["vram"] = summarize_vram(args.vram_csv, args.vram_column, args.vram_unit)
        encoded = json.dumps(result, indent=2, allow_nan=False) + "\n"
        if args.output:
            args.output.write_text(encoded, encoding="utf-8")
        else:
            print(encoded, end="")
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
