#!/usr/bin/env python3
"""
process_testlogs_improved.py

Improved and more robust version of the script you provided. It:
- Parses testlog JSON files in a folder (recursively).
- Handles a few common JSON layouts (top-level single-key wrapper or direct samples dict).
- Aggregates network-wide metrics and per-node metrics (mean + std).
- Avoids destructive "pop" operations when fields are missing in some samples.
- Converts values to native Python floats to guarantee JSON serializability.
- Provides better logging and error handling.
- Adds CLI options for output path and verbosity.

Usage:
    python process_testlogs_improved.py -f /path/to/folder -o processed_results.json --verbose
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import sys
from pathlib import Path
from typing import Any, Dict, List, Mapping, Tuple

import numpy as np
from rich.logging import RichHandler


logger = logging.getLogger("process_testlogs")


def safe_mean_std(values: List[float]) -> Tuple[float, float]:
    """Return (mean, std) as python floats for a list of numeric values.

    Empty list -> (0.0, 0.0).
    """
    if not values:
        return 0.0, 0.0
    arr = np.asarray(values, dtype=float)
    return float(arr.mean()), float(arr.std())


def parse_testlogs(folder: Path) -> Dict[str, Dict[str, Any]]:
    """Walk `folder` recursively and load JSON files.

    Returns a mapping experiment_name -> samples_dict where samples_dict is a mapping
    sample_id -> sample_data (dict). The loader tries to be flexible with two common
    formats found in your original script:
      1) { "experiment_name": { "0": {...}, "1": {...} } }
      2) { "0": {...}, "1": {...} }

    If several files contain the same experiment_name the sample mappings are merged.
    If sample ids collide they are prefixed with the filename stem to avoid overwrites.
    """
    results: Dict[str, Dict[str, Any]] = {}

    for root, _, files in os.walk(folder):
        # Lets omit the manifest which is manifest.json
        json_files = [f for f in files if f.endswith(
            ".json") and f != "manifest.json"]
        if not json_files:
            continue
        logger.debug("Scanning %s: %d json files", root, len(json_files))

        for fname in json_files:
            fpath = Path(root) / fname
            try:
                with fpath.open("r") as fh:
                    doc = json.load(fh)
            except json.JSONDecodeError as e:
                logger.error("Skipping invalid JSON %s: %s", fpath, e)
                continue
            except Exception:
                logger.exception("Failed reading %s", fpath)
                continue

            # Determine samples dict and experiment name
            if isinstance(doc, dict) and len(doc) == 1:
                top_key, top_val = next(iter(doc.items()))
                if isinstance(top_val, dict):
                    experiment_name = str(top_key)
                    samples = top_val
                else:
                    experiment_name = fpath.stem
                    samples = doc
            elif isinstance(doc, dict):
                experiment_name = fpath.stem
                samples = doc
            else:
                logger.warning(
                    "Unrecognized JSON structure in %s, skipping", fpath)
                continue

            # Merge into results, handling collisions
            if experiment_name in results:
                existing = results[experiment_name]
                overlap = set(existing).intersection(samples)
                if overlap:
                    # avoid silently overwriting sample ids
                    for k, v in samples.items():
                        if k in overlap:
                            new_k = f"{fpath.stem}_{k}"
                            logger.debug(
                                "Renaming colliding sample id %s -> %s", k, new_k)
                            existing[new_k] = v
                        else:
                            existing[k] = v
                else:
                    existing.update(samples)
            else:
                results[experiment_name] = dict(samples)

    return results


def process_folder_samples(folder: Path, results: Mapping[str, Mapping[str, Any]], output_file: Path | None = None) -> None:
    """
    Aggregate statistics from `results` and write processed JSON to disk.
    Supports new structure with per-sample averages inside network power and
    per-node power.samples_mW.
    """

    # Collectors
    net_avg_power: List[float] = []   # "avg_mW" across runs
    net_avg_cpu: List[float] = []    # "avg_cpu" across runs
    net_avg_radio: List[float] = []  # "avg_radio" across runs
    net_avg_energy: List[float] = []   # "avg_mJ" across runs
    net_avg_latency: List[float] = []  # "avg_latency_us" across runs
    net_avg_plr: List[float] = []      # "avg" across runs
    net_avg_pdr: List[float] = []      # "avg" across runs
    net_avg_jitter: List[float] = []   # "avg" across runs
    net_avg_duty_cycle: List[float] = []  # "avg" across runs

    net_per_sample_power: Dict[str, List[float]] = {}
    net_per_sample_cpu: Dict[str, List[float]] = {}
    net_per_sample_radio: Dict[str, List[float]] = {}
    net_per_sample_energy: Dict[str, List[float]] = {}
    net_per_sample_latency: Dict[str, List[float]] = {}
    net_per_sample_plr: Dict[str, List[float]] = {}
    net_per_sample_pdr: Dict[str, List[float]] = {}
    net_per_sample_jitter: Dict[str, List[float]] = {}
    net_per_sample_duty_cycle: Dict[str, List[float]] = {}

    # node_id -> metric_name -> list of values
    node_acc: Dict[str, Dict[str, List[float]]] = {}

    # Iterate runs
    for run_id, run in results.items():
        if not isinstance(run, dict):
            continue

        # --- Network metrics ---
        network = run.get("network", {})
        power = network.get("power", {})
        cpu = network.get("cpu_activity", {})
        radio = network.get("radio_activity", {})
        energy = network.get("energy", {})
        latency = network.get("latency", {})
        packet_loss = network.get("packet_loss", {})
        packet_delivery = network.get("packet_delivery_ratio", {})
        jitter = network.get("jitter", {})
        duty_cycle = network.get("rdc", {})

        if "avg_mW" in power:
            net_avg_power.append(float(power["avg_mW"]))

        if "avg" in cpu:
            net_avg_cpu.append(float(cpu["avg"]))

        if "avg" in radio:
            net_avg_radio.append(float(radio["avg"]))

        if "avg_mJ" in energy:
            net_avg_energy.append(float(energy["avg_mJ"]))

        if "avg_latency_us" in latency:
            net_avg_latency.append(float(latency["avg_latency_us"]))

        if "avg" in packet_loss:
            net_avg_plr.append(float(packet_loss["avg"]))

        if "avg" in packet_delivery:
            net_avg_pdr.append(float(packet_delivery["avg"]))

        if "avg" in jitter:
            net_avg_jitter.append(float(jitter["avg"]))

        if "avg" in duty_cycle:
            net_avg_duty_cycle.append(float(duty_cycle["avg"]))

        if "per_sample_avg_mW" in power and isinstance(power["per_sample_avg_mW"], dict):
            for seq, val in power["per_sample_avg_mW"].items():
                net_per_sample_power.setdefault(seq, []).append(float(val))

        if "per_sample_avg" in cpu and isinstance(cpu["per_sample_avg"], dict):
            for seq, val in cpu["per_sample_avg"].items():
                net_per_sample_cpu.setdefault(seq, []).append(float(val))

        if "per_sample_avg" in radio and isinstance(radio["per_sample_avg"], dict):
            for seq, val in radio["per_sample_avg"].items():
                net_per_sample_radio.setdefault(seq, []).append(float(val))

        if "per_sample_avg_mJ" in energy and isinstance(energy["per_sample_avg_mJ"], dict):
            for seq, val in energy["per_sample_avg_mJ"].items():
                net_per_sample_energy.setdefault(seq, []).append(float(val))

        if "per_sample_avg_us" in latency and isinstance(latency["per_sample_avg_us"], dict):
            for seq, val in latency["per_sample_avg_us"].items():
                net_per_sample_latency.setdefault(seq, []).append(float(val))

        if "samples" in packet_loss and isinstance(packet_loss["samples"], dict):
            for seq, val in packet_loss["samples"].items():
                net_per_sample_plr.setdefault(seq, []).append(float(val))

        if "samples" in packet_delivery and isinstance(packet_delivery["samples"], dict):
            for seq, val in packet_delivery["samples"].items():
                net_per_sample_pdr.setdefault(seq, []).append(float(val))

        if "samples" in jitter and isinstance(jitter["samples"], dict):
            for seq, val in jitter["samples"].items():
                net_per_sample_jitter.setdefault(seq, []).append(float(val))

        if "per_sample_avg" in duty_cycle and isinstance(duty_cycle["per_sample_avg"], dict):
            for seq, val in duty_cycle["per_sample_avg"].items():
                net_per_sample_duty_cycle.setdefault(
                    seq, []).append(float(val))

        # --- Per-node metrics ---
        for node_id, node in run.items():
            if node_id == "network":
                continue
            if not isinstance(node, dict):
                continue

            pwr = node.get("power")
            cpu = node.get("cpu_activity")
            radio = node.get("radio_activity")
            egy = node.get("energy")
            lcy = node.get("latency")
            pkt_loss = node.get("packet_loss")
            pkt_delivery_ratio = node.get("packet_delivery_ratio")
            jter = node.get("jitter")
            dty_cycles = node.get("rdc")

            if isinstance(pwr, dict) and "average_mW" in pwr:
                node_acc.setdefault(node_id, {}).setdefault(
                    "average_mW", []).append(float(pwr["average_mW"]))

            if isinstance(cpu, dict) and "average" in cpu:
                node_acc.setdefault(node_id, {}).setdefault(
                    "average_cpu", []).append(float(cpu["average"]))

            if isinstance(radio, dict) and "average" in radio:
                node_acc.setdefault(node_id, {}).setdefault(
                    "average_radio", []).append(float(radio["average"]))

            if isinstance(egy, dict) and "energy_mJ" in egy:
                node_acc.setdefault(node_id, {}).setdefault(
                    "average_mJ", []).append(float(egy["energy_mJ"]))

            if isinstance(lcy, dict) and "average_us" in lcy:
                node_acc.setdefault(node_id, {}).setdefault(
                    "average_us", []).append(float(lcy["average_us"]))

            if isinstance(pkt_loss, dict) and "percentage" in pkt_loss:
                node_acc.setdefault(node_id, {}).setdefault(
                    "average_packet_loss", []).append(float(pkt_loss["percentage"]))

            if isinstance(pkt_delivery_ratio, dict) and "average" in pkt_delivery_ratio:
                node_acc.setdefault(node_id, {}).setdefault(
                    "average_packet_delivery_ratio", []).append(float(pkt_delivery_ratio["average"]))

            if isinstance(jter, dict) and "average_us" in jter:
                node_acc.setdefault(node_id, {}).setdefault(
                    "average_jitter", []).append(float(jter["average_us"]))

            if isinstance(dty_cycles, dict) and "average_rdc" in dty_cycles:
                node_acc.setdefault(node_id, {}).setdefault(
                    "average_rdc", []).append(float(dty_cycles["average_rdc"]))

            # If you also want to collect per-sample node power values:
            samples = pwr.get("samples_mW") if isinstance(pwr, dict) else None
            if isinstance(samples, dict):
                for seq, s in samples.items():
                    if not isinstance(s, dict):
                        continue
                    if "power" in s:
                        key = f"samples_mW_{seq}"
                        node_acc.setdefault(node_id, {}).setdefault(
                            key, []).append(float(s["power"]))

            samples = cpu.get("samples") if isinstance(cpu, dict) else None
            if isinstance(samples, dict):
                for seq, s in samples.items():
                    if not isinstance(s, dict):
                        continue
                    if "cpu_activity" in s:
                        key = f"samples_cpu_{seq}"
                        node_acc.setdefault(node_id, {}).setdefault(
                            key, []).append(float(s["cpu_activity"]))

            samples = radio.get("samples") if isinstance(radio, dict) else None
            if isinstance(samples, dict):
                for seq, s in samples.items():
                    if not isinstance(s, dict):
                        continue
                    if "radio_activity" in s:
                        key = f"samples_radio_{seq}"
                        node_acc.setdefault(node_id, {}).setdefault(
                            key, []).append(float(s["radio_activity"]))

            samples = egy.get("samples_mJ") if isinstance(
                egy, dict) else None
            if isinstance(samples, dict):
                for seq, s in samples.items():
                    if not isinstance(s, dict):
                        continue
                    if "Energy" in s:
                        key = f"samples_mJ_{seq}"
                        node_acc.setdefault(node_id, {}).setdefault(
                            key, []).append(float(s["Energy"]))

            samples = dty_cycles.get("samples") if isinstance(
                dty_cycles, dict) else None
            if isinstance(samples, dict):
                for seq, s in samples.items():
                    key = f"samples_rdc_{seq}"
                    node_acc.setdefault(node_id, {}).setdefault(
                        key, []).append(float(s["rdc"]))

            samples = lcy.get("samples_us") if isinstance(lcy, dict) else None
            if isinstance(samples, dict):
                for seq, s in samples.items():
                    key = f"samples_us_{seq}"
                    node_acc.setdefault(node_id, {}).setdefault(
                        key, []).append(float(s))

            samples = pkt_loss.get("samples") if isinstance(
                pkt_loss, dict) else None
            if isinstance(samples, dict):
                for seq, s in samples.items():
                    key = f"samples_packet_loss_{seq}"
                    node_acc.setdefault(node_id, {}).setdefault(
                        key, []).append(float(s))

            samples = pkt_delivery_ratio.get("samples") if isinstance(
                pkt_delivery_ratio, dict) else None
            if isinstance(samples, dict):
                for seq, s in samples.items():
                    key = f"samples_packet_delivery_ratio_{seq}"
                    node_acc.setdefault(node_id, {}).setdefault(
                        key, []).append(float(s))

            samples = jter.get("samples") if isinstance(jter, dict) else None
            if isinstance(samples, dict):
                for seq, s in samples.items():
                    key = f"samples_jitter_{seq}"
                    node_acc.setdefault(node_id, {}).setdefault(
                        key, []).append(float(s))

    # --- Build summaries ---
    net_summary = {"power": {}}
    mu, su = safe_mean_std(net_avg_power)
    net_summary["power"]["avg_mW"] = mu
    net_summary["power"]["std_mW"] = su

    mu, su = safe_mean_std(net_avg_cpu)
    net_summary["cpu"] = {}
    net_summary["cpu"]["avg"] = mu
    net_summary["cpu"]["std"] = su

    mu, su = safe_mean_std(net_avg_radio)
    net_summary["radio"] = {}
    net_summary["radio"]["avg"] = mu
    net_summary["radio"]["std"] = su

    net_summary["energy"] = {}
    mu, su = safe_mean_std(net_avg_energy)
    net_summary["energy"]["avg_mJ"] = mu
    net_summary["energy"]["std_mJ"] = su

    net_summary["latency"] = {}
    mu, su = safe_mean_std(net_avg_latency)
    net_summary["latency"]["avg_us"] = mu
    net_summary["latency"]["std_us"] = su

    net_summary["packet_loss"] = {}
    mu, su = safe_mean_std(net_avg_plr)
    net_summary["packet_loss"]["avg"] = mu
    net_summary["packet_loss"]["std"] = su

    net_summary["packet_delivery_ratio"] = {}
    mu, su = safe_mean_std(net_avg_pdr)
    net_summary["packet_delivery_ratio"]["avg"] = mu
    net_summary["packet_delivery_ratio"]["std"] = su

    net_summary["jitter"] = {}
    mu, su = safe_mean_std(net_avg_jitter)
    net_summary["jitter"]["avg_us"] = mu
    net_summary["jitter"]["std_us"] = su

    net_summary["rdc"] = {}
    mu, su = safe_mean_std(net_avg_duty_cycle)
    net_summary["rdc"]["avg"] = mu
    net_summary["rdc"]["std"] = su

    net_summary["power"]["per_sample_avg_mW"] = {}
    for seq, vals in net_per_sample_power.items():
        mu, su = safe_mean_std(vals)
        net_summary["power"]["per_sample_avg_mW"][seq] = {"avg": mu, "std": su}

    net_summary["cpu"]["per_sample_avg"] = {}
    for seq, vals in net_per_sample_cpu.items():
        mu, su = safe_mean_std(vals)
        net_summary["cpu"]["per_sample_avg"][seq] = {"avg": mu, "std": su}

    net_summary["radio"]["per_sample_avg"] = {}
    for seq, vals in net_per_sample_radio.items():
        mu, su = safe_mean_std(vals)
        net_summary["radio"]["per_sample_avg"][seq] = {"avg": mu, "std": su}

    net_summary["energy"]["per_sample_avg_mJ"] = {}
    for seq, vals in net_per_sample_energy.items():
        mu, su = safe_mean_std(vals)
        net_summary["energy"]["per_sample_avg_mJ"][seq] = {
            "avg": mu, "std": su}

    net_summary["latency"]["per_sample_avg_us"] = {}
    for seq, vals in net_per_sample_latency.items():
        mu, su = safe_mean_std(vals)
        net_summary["latency"]["per_sample_avg_us"][seq] = {
            "avg": mu, "std": su}

    net_summary["packet_loss"]["per_sample_avg"] = {}
    for seq, vals in net_per_sample_plr.items():
        mu, su = safe_mean_std(vals)
        net_summary["packet_loss"]["per_sample_avg"][seq] = {
            "avg": mu, "std": su}

    net_summary["packet_delivery_ratio"]["per_sample_avg"] = {}
    for seq, vals in net_per_sample_pdr.items():
        mu, su = safe_mean_std(vals)
        net_summary["packet_delivery_ratio"]["per_sample_avg"][seq] = {
            "avg": mu, "std": su}

    net_summary["jitter"]["per_sample_avg_us"] = {}
    for seq, vals in net_per_sample_jitter.items():
        mu, su = safe_mean_std(vals)
        net_summary["jitter"]["per_sample_avg_us"][seq] = {
            "avg": mu, "std": su}

    net_summary["rdc"]["per_sample_avg"] = {}
    for seq, vals in net_per_sample_duty_cycle.items():
        mu, su = safe_mean_std(vals)
        net_summary["rdc"]["per_sample_avg"][seq] = {
            "avg": mu, "std": su}

    # Per-node
    nodes_summary: Dict[str, Any] = {}
    for node_id, metrics in node_acc.items():
        out: Dict[str, Any] = {}
        if "average_mW" in metrics:
            mu, su = safe_mean_std(metrics["average_mW"])
            out.setdefault("power", {})["avg_mW"] = mu
            out["power"]["std_mW"] = su

        if "average_cpu" in metrics:
            mu, su = safe_mean_std(metrics["average_cpu"])
            out.setdefault("cpu", {})["avg"] = mu
            out["cpu"]["std"] = su

        if "average_radio" in metrics:
            mu, su = safe_mean_std(metrics["average_radio"])
            out.setdefault("radio", {})["avg"] = mu
            out["radio"]["std"] = su

        if "average_uJ" in metrics:
            mu, su = safe_mean_std(metrics["average_uJ"])
            out.setdefault("energy", {})["avg_uJ"] = mu
            out["energy"]["std_uJ"] = su

        if "average_us" in metrics:
            mu, su = safe_mean_std(metrics["average_us"])
            out.setdefault("latency", {})["avg_us"] = mu
            out["latency"]["std_us"] = su

        if "average_packet_loss" in metrics:
            mu, su = safe_mean_std(metrics["average_packet_loss"])
            out.setdefault("packet_loss", {})["avg"] = mu
            out["packet_loss"]["std"] = su

        if "average_packet_delivery_ratio" in metrics:
            mu, su = safe_mean_std(metrics["average_packet_delivery_ratio"])
            out.setdefault("packet_delivery_ratio", {})["avg"] = mu
            out["packet_delivery_ratio"]["std"] = su

        if "average_jitter" in metrics:
            mu, su = safe_mean_std(metrics["average_jitter"])
            out.setdefault("jitter", {})["avg"] = mu
            out["jitter"]["std"] = su

        if "average_rdc" in metrics:
            mu, su = safe_mean_std(metrics["average_rdc"])
            out.setdefault("rdc", {})["avg"] = mu
            out["rdc"]["std"] = su

        # If per-sample node values were collected
        for k, vals in metrics.items():
            if k.startswith("samples_mW_"):
                seq = k.split("_")[-1]
                mu, su = safe_mean_std(vals)
                out.setdefault("power", {}).setdefault(
                    "samples_mW", {})[seq] = {"avg": mu, "std": su}

            if k.startswith("samples_cpu_"):
                seq = k.split("_")[-1]
                mu, su = safe_mean_std(vals)
                out.setdefault("cpu", {}).setdefault(
                    "samples_cpu", {})[seq] = {"avg": mu, "std": su}

            if k.startswith("samples_radio_"):
                seq = k.split("_")[-1]
                mu, su = safe_mean_std(vals)
                out.setdefault("radio", {}).setdefault(
                    "samples_radio", {})[seq] = {"avg": mu, "std": su}

            if k.startswith("samples_mJ_"):
                seq = k.split("_")[-1]
                mu, su = safe_mean_std(vals)
                out.setdefault("energy", {}).setdefault(
                    "samples_mJ", {})[seq] = {"avg": mu, "std": su}

            if k.startswith("samples_packet_loss_"):
                seq = k.split("_")[-1]
                mu, su = safe_mean_std(vals)
                out.setdefault("packet_loss", {}).setdefault(
                    "samples_packet_loss", {})[seq] = {"avg": mu, "std": su}

            if k.startswith("samples_packet_delivery_ratio_"):
                seq = k.split("_")[-1]
                mu, su = safe_mean_std(vals)
                out.setdefault("packet_delivery_ratio", {}).setdefault(
                    "samples_packet_delivery_ratio", {})[seq] = {"avg": mu, "std": su}

            if k.startswith("samples_us_"):
                seq = k.split("_")[-1]
                mu, su = safe_mean_std(vals)
                out.setdefault("latency", {}).setdefault(
                    "samples_us", {})[seq] = {"avg": mu, "std": su}

            if k.startswith("samples_jitter_"):
                seq = k.split("_")[-1]
                mu, su = safe_mean_std(vals)
                out.setdefault("jitter", {}).setdefault(
                    "samples_jitter", {})[seq] = {"avg": mu, "std": su}

            if k.startswith("samples_rdc_"):
                seq = k.split("_")[-1]
                mu, su = safe_mean_std(vals)
                out.setdefault("rdc", {}).setdefault(
                    "samples_rdc", {})[seq] = {"avg": mu, "std": su}

        nodes_summary[node_id] = out

    # --- Final output ---
    output = {
        folder.name: {
            "network": net_summary,
            "nodes": nodes_summary,
        }
    }

    out_path = output_file or (folder / "processed_results.json")
    with out_path.open("w") as fh:
        json.dump(output, fh, indent=4)
    logger.info("Processed results saved to %s", out_path)


def configure_logging(verbose: bool) -> None:
    fmt = "%(asctime)s - %(message)s"
    handler = RichHandler(rich_tracebacks=True)
    formatter = logging.Formatter(fmt)
    handler.setFormatter(formatter)
    logger.setLevel(logging.DEBUG if verbose else logging.INFO)
    handler.setLevel(logging.DEBUG if verbose else logging.INFO)
    logger.addHandler(handler)


def main(argv: List[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Process testlog JSON files and compute aggregated statistics.")
    parser.add_argument("-f", "--folder", type=Path,
                        required=True, help="Path to folder with JSON testlogs")
    parser.add_argument("-o", "--out", type=Path, default=None,
                        help="Output JSON file (defaults to <folder>/processed_results.json)")
    parser.add_argument("--no-remove-old", action="store_true",
                        help="Do not remove existing processed_results.json before running")
    parser.add_argument("-v", "--verbose",
                        action="store_true", help="Verbose logging")

    args = parser.parse_args(argv)
    configure_logging(args.verbose)

    folder = args.folder
    if not folder.exists() or not folder.is_dir():
        logger.error("Folder %s does not exist or is not a directory", folder)
        return 2

    default_out = folder / "processed_results.json"
    if default_out.exists() and args.out is None and not args.no_remove_old:
        try:
            default_out.unlink()
            logger.info("Removed existing %s", default_out)
        except Exception:
            logger.exception("Could not remove existing %s", default_out)

    results = parse_testlogs(folder)
    if not results:
        logger.warning("No JSON testlog files found in %s", folder)
        return 0

    process_folder_samples(folder, results, output_file=args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
