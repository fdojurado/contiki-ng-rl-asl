import sys
import json
import argparse
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

sns.set(style="whitegrid")


def rolling_mean(series, window=20):
    """Return rolling mean (for smoothing)."""
    return series.rolling(window=window, min_periods=1).mean()


def plot_rl_asl(node_id, node_data, output_folder, xlim=None):
    """Plot RL-ASL decisions over time with success/failure coloring."""
    from matplotlib.lines import Line2D

    samples = node_data.get("rl_asl", {}).get("samples", {})
    if not samples:
        print(f"No RL-ASL samples for node {node_id}. Skipping.")
        return

    df = pd.DataFrame.from_dict(samples, orient="index").astype(float)
    df.index = df.index.astype(int)
    df = df.sort_index()

    # Convert microseconds → minutes
    df["time"] = df["time"] / 1e6 / 60.0
    df["color"] = df["success"].map({1: "green", 0: "red"})

    plt.figure(figsize=(12, 5))
    plt.scatter(df["time"], df["action"], c=df["color"],
                s=70, alpha=0.7, edgecolor="k", linewidth=0.5)

    plt.xlabel("Time (minutes)", fontsize=16)
    plt.ylabel("Action", fontsize=16)
    plt.title(f"RL-ASL Decisions for Node {node_id}", fontsize=18)
    plt.grid(True, linestyle="--", alpha=0.6)

    legend_elements = [
        Line2D([0], [0], marker="o", color="w", label="Success",
               markerfacecolor="green", markeredgecolor="k", markersize=10),
        Line2D([0], [0], marker="o", color="w", label="Failure",
               markerfacecolor="red", markeredgecolor="k", markersize=10)
    ]
    plt.legend(handles=legend_elements, fontsize=12, loc="best")

    if xlim is not None:
        plt.xlim(xlim)

    out_file = Path(output_folder) / f"{node_id}_rl_asl_timeline.png"
    plt.tight_layout()
    plt.savefig(out_file)
    plt.close()
    print(f"Saved {out_file}")


def load_json_data(file_path):
    """Load JSON safely with checks."""
    file_path = Path(file_path)
    if not file_path.exists():
        print(f"Error: File not found at {file_path}")
        sys.exit(1)

    if file_path.stat().st_size == 0:
        print(f"Error: JSON file '{file_path}' is empty.")
        sys.exit(1)

    try:
        with open(file_path, "r") as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"Error decoding JSON from '{file_path}': {e}")
        sys.exit(1)

    return data


def extract_episode_data(data):
    """Extract per-node episode monitoring data."""
    episode_data = {}
    for node_id, node_data in data.get("0", {}).items():
        if node_id == "network":
            continue
        samples = node_data.get("episode_monitoring", {}).get("samples", {})
        if samples:
            df = pd.DataFrame.from_dict(samples, orient="index").astype(float)
            df.index = df.index.astype(int)
            df = df.sort_index()
            episode_data[node_id] = df
    return episode_data


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Plot Q-Learning results (reward vs episode, epsilon, RL-ASL)"
    )
    parser.add_argument("files", nargs="+",
                        help="Path(s) to one or more JSON files")
    parser.add_argument(
        "--legends", nargs="*", help="Legends for each file (same order as files)"
    )
    parser.add_argument(
        "-o", "--output-folder", type=str, default="Q-Learning-plots",
        help="Folder to save the plots"
    )
    parser.add_argument(
        "--xlim", type=float, nargs=2, metavar=("XMIN", "XMAX"),
        help="Limit x-axis range for RL-ASL plots"
    )
    args = parser.parse_args()

    output_folder = Path(args.output_folder)
    output_folder.mkdir(parents=True, exist_ok=True)

    # Handle legends
    if args.legends:
        legends = args.legends
        if len(legends) < len(args.files):
            # Fill missing with filenames
            legends += [Path(f).stem for f in args.files[len(legends):]]
    else:
        legends = [Path(f).stem for f in args.files]

    # Load all files
    datasets = [load_json_data(f) for f in args.files]
    node_ids = list(datasets[0]["0"].keys())
    if "network" in node_ids:
        node_ids.remove("network")

    # --- Main Plot: Avg Reward vs Episode (all inputs) + Single Epsilon ---
    for node_id in node_ids:
        plt.figure(figsize=(10, 5))
        ax1 = plt.gca()

        for data, legend in zip(datasets, legends):
            episode_data = extract_episode_data(data)
            if node_id not in episode_data:
                print(
                    f"No episode data for node {node_id} in {legend}. Skipping.")
                continue
            df = episode_data[node_id]
            sns.lineplot(x=df.index, y=df["avg_reward"],
                         label=f"{legend} (Avg Reward)", ax=ax1)

        # Only plot epsilon from first file
        first_df = extract_episode_data(datasets[0]).get(node_id)
        if first_df is not None and "epsilon" in first_df:
            ax2 = ax1.twinx()
            sns.lineplot(x=first_df.index, y=first_df["epsilon"], ax=ax2,
                         label="Epsilon", color="red", linestyle=":")
            ax2.set_ylabel("Epsilon", fontsize=16)
            ax2.legend(loc="upper right", fontsize=12)

        ax1.set_xlabel("Episode", fontsize=16)
        ax1.set_ylabel("Reward", fontsize=16)
        ax1.legend(loc="upper left", fontsize=12)
        plt.grid(True, linestyle="--", alpha=0.6)
        plt.tight_layout()

        out_file = output_folder / f"{node_id}_avg_reward_epsilon.png"
        plt.savefig(out_file)
        plt.close()
        print(f"Saved {out_file}")

        # RL-ASL plot (from first dataset)
        first_data = datasets[0]
        first_node_data = first_data.get("0", {}).get(node_id, {})
        plot_rl_asl(node_id, first_node_data, output_folder, xlim=args.xlim)
