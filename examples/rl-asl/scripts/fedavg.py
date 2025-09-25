#!/usr/bin/env python3
import json
import numpy as np
from pathlib import Path
import argparse


def load_federated_qtables(files):
    q_infos = []
    for file in files:
        with open(file, "r") as f:
            q_info = json.load(f)
            q_infos.append(q_info)
    return q_infos


def fedavg(q_infos):
    """Federated averaging of multiple Q-tables with episode weighting."""
    q_tables = []
    weights = []

    for q in q_infos:
        q_tables.append(np.array(q["q_table"], dtype=np.float32))
        # default weight=1 if missing
        weights.append(q.get("episode_count", 1))

    if not q_tables:
        raise ValueError("No Q-tables found for aggregation!")

    weights = np.array(weights, dtype=np.float32)
    weights = weights / weights.sum()  # normalize

    agg_q = np.average(q_tables, axis=0, weights=weights)
    return agg_q


def export_global(agg_q, out_dir: Path, q_infos):
    num_states, num_actions = agg_q.shape

    # --- JSON ---
    agg_json = {
        "num_states": num_states,
        "num_actions": num_actions,
        "q_table": agg_q.tolist(),
        "episode_count": sum(q["episode_count"] for q in q_infos),
        # "sources": [
        #     {"scenario": q["scenario_id"], "topology": q["topology_id"],
        #         "episodes": q["episode_count"]}
        #     for q in q_infos
        # ],
        "node_id": "federated_global"
    }

    out_dir.mkdir(parents=True, exist_ok=True)
    json_path = out_dir / "federated_global.json"
    with open(json_path, "w") as f:
        json.dump(agg_json, f, indent=2)

    # --- C header ---
    lines = []
    for row in agg_q:
        row_str = ", ".join(f"{v:.6f}f" for v in row)
        lines.append(f"    {{{row_str}}},")
    c_array = (
        f"#ifndef RL_ASL_FEDERATED_Q_GLOBAL_H\n"
        f"#define RL_ASL_FEDERATED_Q_GLOBAL_H\n\n"
        f"static const float rl_asl_federated_q_global[{num_states}][{num_actions}] = {{\n"
        + "\n".join(lines)
        + "\n};\n\n"
        f"#endif /* RL_ASL_FEDERATED_Q_GLOBAL_H */\n"
    )

    header_path = out_dir / "rl-asl-federated-q-global.h"
    header_path.write_text(c_array)

    print(
        f"[✓] Exported federated_global.json and rl-asl-federated-q-global.h in {out_dir}")


def main():
    parser = argparse.ArgumentParser(
        description="Federated Averaging of Q-tables")
    parser.add_argument(
        "json_files", nargs="*", type=Path,
        help="Paths to federated_*.json files. If none provided, defaults to federated_q/*.json"
    )
    parser.add_argument(
        "-o", "--out-dir", type=Path, default=Path("federated_global"),
        help="Output directory (default: federated_global)"
    )
    args = parser.parse_args()

    if args.json_files:
        files = args.json_files
    else:
        files = sorted(Path("federated_q").glob("federated_*.json"))

    if not files:
        print("No federated JSON files found.")
        return

    q_infos = load_federated_qtables(files)
    agg_q = fedavg(q_infos)
    export_global(agg_q, args.out_dir, q_infos)


if __name__ == "__main__":
    main()
