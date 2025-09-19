# This scriot is used to automate the results of the experiments.
# It runs the simulation for each of the experiments and saves the results in a csv file in the results folder.
# Each experiment is a tuple of weights of user requirements.
# The user requirements weights are:
# Power consumption, latency, and throughput.
# The weights are a number between 0 and 1.
# Each weight has a step of 0.1.
# The sum of the weights is 1.
# alpha represents the weight of the power consumption.
# beta represents the weight of the latency.
# gamma represents the weight of the throughput.

from __future__ import annotations

import argparse
import json
import logging
import os
import re
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

import numpy as np
import pandas as pd
from rich.logging import RichHandler

from node import Node
from network import Network
import process_common as pc
import fit_iot_lab_conf

logger = logging.getLogger("process_experiments")

START_TIMESTAMP = 60*5*1e6

# --- regex patterns ---
# Period summary #9 (60 seconds)
# Period summary #118 (60 seconds)
_RE_ENERGEST_SEQ = re.compile(r"Period summary #(\d+)\b")
_RE_ENERGEST_TOTAL_TIME_SECS = re.compile(
    r"Period summary #\d+\s*\((\d+)\s*seconds\)")
_RE_ENERGEST_TOTAL_TIME_TICKS = re.compile(
    r"\[INFO: Energest\s*\]\s*Total time\s*:\s*(\d+)")
_RE_ENERGEST_CPU_TIME = re.compile(r"\[INFO: Energest\s*\]\s*CPU\s*:\s*(\d+)")
_RE_ENERGEST_LPM_TIME = re.compile(r"\[INFO: Energest\s*\]\s*LPM\s*:\s*(\d+)")
_RE_ENERGEST_DEEP_LPM_TIME = re.compile(
    r"\[INFO: Energest\s*\]\s*Deep LPM\s*:\s*(\d+)")
_RE_ENERGEST_RADIO_TX_TIME = re.compile(
    r"\[INFO: Energest\s*\]\s*Radio Tx\s*:\s*(\d+)")
_RE_ENERGEST_RADIO_RX_TIME = re.compile(
    r"\[INFO: Energest\s*\]\s*Radio Rx\s*:\s*(\d+)")
_RE_ENERGEST_RADIO_UC_RX_TIME = re.compile(
    r"\[INFO: Energest\s*\]\s*UC Radio Rx\s*:\s*(\d+)")
_RE_ENERGEST_RADIO_UC_IDLE_RX_TIME = re.compile(
    r"\[INFO: Energest\s*\]\s*UC Idle Rx\s*:\s*(\d+)")
_RE_ENERGEST_RADIO_UC_IDLE_RATIO = re.compile(
    r"\[INFO: Energest\s*\]\s*UC Idle ratio\s*\([\d\.]+\s*\)\s*:\s*([\d\.]+)")
_RE_ENERGEST_RADIO_UC_RATIO = re.compile(
    r"\[INFO: Energest\s*\]\s*UC ratio\s*\([\d\.]+\s*\)\s*:\s*([\d\.]+)")
_RE_ENERGEST_RADIO_TOTAL_TIME = re.compile(
    r"\[INFO: Energest\s*\]\s*Radio total\s*:\s*(\d+)")
_RE_ENERGEST_ENERGY = re.compile(
    r"\[INFO: Energest\s*\]\s*Total energy\s*\(uJ\)\s*:\s*(\d+)")
_RE_RADIO_TOTAL = re.compile(r"Radio total\s*:\s*([0-9\.]+)\s*/\s*([0-9\.]+)")
_RE_SEND_SEQ = re.compile(
    r"Sending request\s*(\d+)\s*to\s*([0-9a-fA-F:]+)\s* at asn\s*([0-9]+)(?:\s*\(timeslot\s*([0-9]+)\))?")
_RE_RX_SEQ = re.compile(
    r"Received request 'hello\s*([0-9]+)'\s*from\s*([0-9a-fA-F:]+)")

_RE_PLATFORM = re.compile(r"(?:.+_)?([A-Za-z0-9-]+)_(?:\d+)\.(?:oml|txt)$")


def _safe_int(s: str, default: int = 0) -> int:
    try:
        return int(s)
    except Exception:
        return default


def _safe_float(s: str, default: float = 0.0) -> float:
    try:
        return float(s)
    except Exception:
        return default


def process_line(timestamp: float, node: Node, msg: str, network: Network, args) -> None:
    # Dispatch to the specific processors based on message content
    energest_seq = _RE_ENERGEST_SEQ.search(msg)
    energest_total_secs = _RE_ENERGEST_TOTAL_TIME_SECS.search(msg)
    energest_total_time_ticks = _RE_ENERGEST_TOTAL_TIME_TICKS.search(msg)
    energest_cpu_time = _RE_ENERGEST_CPU_TIME.search(msg)
    energest_lpm_time = _RE_ENERGEST_LPM_TIME.search(msg)
    energest_deep_lpm_time = _RE_ENERGEST_DEEP_LPM_TIME.search(msg)
    energest_radio_tx_time = _RE_ENERGEST_RADIO_TX_TIME.search(msg)
    energest_radio_rx_time = _RE_ENERGEST_RADIO_RX_TIME.search(msg)
    energest_radio_uc_rx_time = _RE_ENERGEST_RADIO_UC_RX_TIME.search(msg)
    energest_radio_uc_idle_rx_time = _RE_ENERGEST_RADIO_UC_IDLE_RX_TIME.search(
        msg)
    energest_radio_uc_idle_ratio = _RE_ENERGEST_RADIO_UC_IDLE_RATIO.search(msg)
    energest_radio_uc_ratio = _RE_ENERGEST_RADIO_UC_RATIO.search(msg)
    energest_radio_total_time = _RE_ENERGEST_RADIO_TOTAL_TIME.search(msg)
    send_seq = _RE_SEND_SEQ.search(msg)
    receive_seq = _RE_RX_SEQ.search(msg)

    if energest_seq:
        node.update_last_power_seq(int(energest_seq.group(1)))
    if energest_total_secs:
        node.power_trace_add(seq=node.get_last_power_seq(), data={
                             "type": "total_time_secs", "value": energest_total_secs.group(1)}, time=timestamp)
    if energest_total_time_ticks:
        node.power_trace_add(seq=node.get_last_power_seq(), data={
                             "type": "total_time_ticks", "value": energest_total_time_ticks.group(1)}, time=timestamp)
    if energest_cpu_time:
        node.power_trace_add(seq=node.get_last_power_seq(), data={
                             "type": "cpu", "value": energest_cpu_time.group(1)}, time=timestamp)
    if energest_lpm_time:
        node.power_trace_add(seq=node.get_last_power_seq(), data={
                             "type": "lpm", "value": energest_lpm_time.group(1)}, time=timestamp)
    if energest_deep_lpm_time:
        node.power_trace_add(seq=node.get_last_power_seq(), data={
                             "type": "deep_lpm", "value": energest_deep_lpm_time.group(1)}, time=timestamp)
    if energest_radio_tx_time:
        node.power_trace_add(seq=node.get_last_power_seq(), data={
                             "type": "tx", "value": energest_radio_tx_time.group(1)}, time=timestamp)
    if energest_radio_rx_time:
        node.power_trace_add(seq=node.get_last_power_seq(), data={
                             "type": "rx", "value": energest_radio_rx_time.group(1)}, time=timestamp)
    if energest_radio_uc_rx_time:
        node.power_trace_add(seq=node.get_last_power_seq(), data={
                             "type": "uc_rx", "value": energest_radio_uc_rx_time.group(1)}, time=timestamp)
    if energest_radio_uc_idle_rx_time:
        node.power_trace_add(seq=node.get_last_power_seq(), data={
                             "type": "uc_idle_rx", "value": energest_radio_uc_idle_rx_time.group(1)}, time=timestamp)
    if energest_radio_uc_idle_ratio:
        node.power_trace_add(seq=node.get_last_power_seq(), data={
                             "type": "uc_idle_ratio", "value": energest_radio_uc_idle_ratio.group(1)}, time=timestamp)
    if energest_radio_uc_ratio:
        node.power_trace_add(seq=node.get_last_power_seq(), data={
                             "type": "uc_ratio", "value": energest_radio_uc_ratio.group(1)}, time=timestamp)
    if energest_radio_total_time:
        node.power_trace_add(seq=node.get_last_power_seq(), data={
                             "type": "radio_total", "value": energest_radio_total_time.group(1)}, time=timestamp)

    if send_seq:
        seq = _safe_int(send_seq.group(1))
        asn = _safe_int(send_seq.group(3))
        timeslot = _safe_int(send_seq.group(
            4), -1) if send_seq.group(4) else -1
        node.delay_add(seq=seq, delay=0, timeslot=timeslot,
                       time_at_tx=timestamp)
    if receive_seq:
        seq = _safe_int(receive_seq.group(1))
        ipaddress = receive_seq.group(2)
        try:
            if ipaddress.startswith("fd00::"):
                ipaddress = ipaddress[6:]
            ip_parts = ipaddress.split(":")
            if len(ip_parts) >= 2:
                node_id_part = ip_parts[-1]  # last part
                node_id = int(node_id_part, 16)
            else:
                node_id = int(ipaddress, 16)
        except Exception:
            node_id = -1
        src_node = network.nodes_get(node_id)
        if src_node is None:
            logger.debug(
                "Source node %s (id=%s) not found in network", ipaddress, node_id)
        else:
            src_node.delay_update_time_at_rx(seq=seq, time_at_rx=timestamp)
    if "SAGE node joining network" in msg:
        node.joined_set(timestamp)


def flushing_packet_error(testlog: Path) -> bool:
    try:
        content = testlog.read_text()
        return "flushing packet" in content
    except Exception:
        return False


def read_oml_consumption(path: Path, power_scale: float = 1.0) -> pd.DataFrame:
    """Read an OML consumption file and return a DataFrame indexed by elapsed seconds.

    Parameters
    - path: path to the .oml file
    - power_scale: multiplier to convert the file's power column into watts.
      Example: if the file stores power in microWatts use power_scale=1e-6.

    Returns a DataFrame with columns ['power_W', 'voltage', 'current'] and a
    float index representing seconds since first sample (as a TimedeltaIndex).
    """
    rows: List[Tuple[float, float, float, float]] = []
    try:
        text = path.read_text()
    except Exception:
        logger.exception("Failed to read OML file %s", path)
        return pd.DataFrame()

    for raw in text.splitlines():
        if not raw.strip():
            continue
        if raw.startswith(("protocol:", "domain:", "start-time:", "sender-id:", "app-name:", "schema:", "content:")):
            continue
        parts = raw.strip().split()
        if len(parts) < 8:
            continue
        # parts layout assumed: <...> ts_s ts_us power voltage current
        ts_s = _safe_int(parts[3])
        ts_us = _safe_int(parts[4])
        power = _safe_float(parts[5]) * power_scale
        voltage = _safe_float(parts[6])
        current = _safe_float(parts[7])
        ts = ts_s + ts_us * 1e-6
        rows.append((ts, power, voltage, current))

    if not rows:
        return pd.DataFrame()

    df = pd.DataFrame(
        rows, columns=["ts_abs", "power_W", "voltage", "current"]).sort_values("ts_abs")
    t0 = df["ts_abs"].iloc[0]
    df["ts_rel_s"] = df["ts_abs"] - t0
    df.index = pd.to_timedelta(df["ts_rel_s"], unit="s")
    df = df.drop(columns=["ts_abs", "ts_rel_s"]).sort_index()
    return df


def energy_joules(series: pd.Series) -> float:
    """Integrate power (W) over time index (TimedeltaIndex) -> Joules.

    The series index must be a TimedeltaIndex or DatetimeIndex; values are power in watts.
    """
    if series.empty:
        return 0.0
    # Convert index to seconds as float
    t = series.index.total_seconds().astype(float)
    p = series.values.astype(float)
    # trapezoidal integration (units: W * s = J)
    return float(np.trapz(p, t))


def process_testlog(testlog: Path, args) -> Dict:
    """Parse a single testlog and return the calculated_results from pc.calculate_results()."""

    # Check for ERR before any processing
    try:
        raw_text = testlog.read_text()
    except Exception:
        logger.exception("Could not read testlog %s", testlog)
        return {}

    # if "ERR" in raw_text:
    #     logger.warning("Skipping file %s due to ERR in contents", testlog)
    #     return {}

    # logger.info("Processing testlog %s", testlog)

    network = Network()
    is_testbed = bool(args.testbed)
    start_ts_unix: Optional[float] = None

    # Split lines and skip header lines if present
    lines = [ln for ln in raw_text.splitlines() if ln.strip()]
    if len(lines) >= 2 and (lines[0].startswith("#") or "start-time" in lines[0].lower()):
        lines = lines[2:]

    for ln in lines:
        if "Script timed out." in ln:
            logger.warning(
                "Script timed out found in %s; stopping parse", testlog)
            break

        tokens = ln.split(";") if is_testbed else ln.split()
        try:
            timestamp, start_ts_unix = pc.get_time(
                tokens, start_ts_unix, is_testbed)
        except Exception:
            logger.debug("Failed to get time for line: %s", ln)
            continue

        if timestamp < START_TIMESTAMP:
            continue

        node_id = pc.get_node_id(tokens, is_testbed)
        node = network.nodes_add(node_id)

        # Message content: join remaining tokens
        try:
            message = " ".join(tokens[2:]).strip()
        except Exception:
            message = ""

        process_line(timestamp, node, message, network, args)

    # If testbed, process OML files co-located with testlog
    if is_testbed:
        oml_dir = testlog.parent
        logger.info("Looking for OML files in %s", oml_dir)
        for file in oml_dir.iterdir():
            if file.suffix.lower() != ".oml":
                continue
            logger.info("Processing OML file %s", file.name)
            # Allow user to pass power_scale; default assume values are in W
            power_scale = float(getattr(args, "oml_power_scale", 1.0))
            df = read_oml_consumption(file, power_scale=power_scale)
            if df.empty:
                logger.debug("Empty OML dataframe for %s", file)
                continue

            # Resample to 1-minute bins (mean power over each minute)
            per_min = df[["power_W"]].resample("1min").mean()
            # energy per minute in Joules
            energy_per_min = df[["power_W"]].resample(
                "1min").apply(lambda s: energy_joules(s["power_W"]))
            # cumulative energy
            energy_cumsum = energy_per_min.cumsum()

            # Convert index to seconds (as integer microseconds to match earlier approach if needed)
            per_min = per_min.reset_index()
            energy_per_min = energy_per_min.reset_index()
            energy_cumsum = energy_cumsum.reset_index()

            # Add seq column
            per_min.insert(0, "seq", range(len(per_min)))
            energy_per_min.insert(0, "seq", range(len(energy_per_min)))
            energy_cumsum.insert(0, "seq", range(len(energy_cumsum)))

            # Extract platform from filename (fallback to first token)
            m = _RE_PLATFORM.match(file.name)
            platform = m.group(1) if m else file.stem.split(
                "_")[-2] if "_" in file.stem else file.stem
            node_info = fit_iot_lab_conf.get_node_config(platform)
            node = network.nodes_add(
                node_info["node_id"]) if node_info else None
            if node is None:
                logger.warning(
                    "Could not resolve node for platform %s (file %s)", platform, file)
                continue

            # Walk rows and add to node
            for _, prow in per_min.iterrows():
                seq = int(prow["seq"])
                power_w = float(prow["power_W"]) if not np.isnan(
                    prow["power_W"]) else 0.0
                # store power in microWatts as int if your Node expects that; otherwise store in W
                # we keep the original method naming (power_testbed_add expects µW in your code)
                node.power_testbed_add(seq, int(round(
                    power_w * 1e6)), int(prow["time"].total_seconds() * 1e6) if "time" in prow else 0)

            for _, erow in energy_per_min.iterrows():
                seq = int(erow["seq"])
                # erow is a 1-column dataframe (power_W integrated -> J); value might be in column 'power_W'
                energy_j = float(erow.get("power_W", 0.0)) if isinstance(
                    erow, pd.Series) else 0.0
                node.energy_testbed_add(seq, energy_j, int(
                    erow["time"].total_seconds() * 1e6) if "time" in erow else 0)

            for _, crow in energy_cumsum.iterrows():
                seq = int(crow["seq"])
                cum_j = float(crow.get("power_W", 0.0)) if isinstance(
                    crow, pd.Series) else 0.0
                node.energy_testbed_add(seq, cum_j, int(
                    crow["time"].total_seconds() * 1e6) if "time" in crow else 0)

    # Final computation
    calculation_results = pc.calculate_results(network)
    return calculation_results


def configure_logging(output_folder: Optional[Path], verbose: bool) -> None:
    fmt = "%(asctime)s - %(levelname)s - %(message)s"
    handler = RichHandler(rich_tracebacks=True)
    formatter = logging.Formatter(fmt)
    handler.setFormatter(formatter)
    logger.setLevel(logging.DEBUG if verbose else logging.INFO)
    logger.addHandler(handler)

    if output_folder is not None:
        output_folder.mkdir(parents=True, exist_ok=True)
        logfile = output_folder / "process_logs.log"
        fh = logging.FileHandler(logfile)
        fh.setFormatter(formatter)
        fh.setLevel(logging.DEBUG)
        logger.addHandler(fh)


def main(argv: Optional[Iterable[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Run experiments parsing and aggregation.")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("-r", "--root-folder", type=Path,
                       help="Root folder to recursively process")
    group.add_argument("-t", "--testlog", type=Path,
                       help="Single testlog file to process")

    parser.add_argument("-o", "--output-folder", type=Path,
                        default=None, help="Folder to store logs and outputs")
    parser.add_argument("--testbed", action="store_true",
                        help="Indicates the logs are from the FIT IoT-LAB testbed")
    parser.add_argument("--oml-power-scale", type=float, default=1.0,
                        help="Multiplier to convert OML power values into Watts (e.g. use 1e-6 if OML reports microWatts)")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args(list(argv) if argv is not None else None)

    configure_logging(args.output_folder, args.verbose)

    if args.testlog:
        pc.preprocess_testlog(
            args.testlog, args.testlog.parent, process_testlog, args)
    elif args.root_folder:
        pc.process_root_folder(args.root_folder, process_testlog, args)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
