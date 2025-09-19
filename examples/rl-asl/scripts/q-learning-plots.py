import sys
import json
import argparse
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from matplotlib.lines import Line2D

sns.set(style="whitegrid")


def save_plot(df, x, y, xlabel, ylabel, filename, node_id, output_folder,
              label=None, color=None, linestyle="-", secondary_y=None):
    """Helper to save line plots."""
    plt.figure(figsize=(10, 5))

    ax = sns.lineplot(x=x, y=y, data=df, label=label or y,
                      color=color, linestyle=linestyle)

    if secondary_y is not None:
        ax2 = ax.twinx()
        sns.lineplot(x=df.index, y=df[secondary_y], ax=ax2, label=secondary_y,
                     color="red", linestyle="--")
        ax2.set_ylabel(secondary_y, fontsize=16)
        ax2.legend(loc="upper right", fontsize=12)

    plt.xlabel(xlabel, fontsize=16)
    plt.ylabel(ylabel, fontsize=16)
    plt.legend(loc="best", fontsize=12)
    plt.grid(True, linestyle="--", alpha=0.6)
    plt.tight_layout()

    out_file = Path(output_folder) / f"{node_id}_{filename}.png"
    plt.savefig(out_file)
    plt.close()
    print(f"Saved {out_file}")


def rolling_mean(series, window=20):
    """Return rolling mean (for smoothing)."""
    return series.rolling(window=window, min_periods=1).mean()


def plot_rl_asl(node_id, node_data, output_folder, xlim=None):
    """Plot RL-ASL decisions over time with success/failure coloring.

    Args:
        node_id (str): Node identifier
        node_data (dict): Node data with rl_asl info
        output_folder (Path): Where to save plots
        xlim (tuple or None): (xmin, xmax) to zoom into the timeline (in seconds)
    """
    samples = node_data.get("rl_asl", {}).get("samples", {})
    if not samples:
        print(f"No RL-ASL samples for node {node_id}. Skipping.")
        return

    # --- Convert to DataFrame ---
    df = pd.DataFrame.from_dict(samples, orient="index").astype(float)
    df.index = df.index.astype(int)
    df = df.sort_index()

    # Convert microseconds → seconds
    df["time"] = df["time"] / 1e6
    
    # convert to minutes
    df["time"] = df["time"] / 60.0

    # Map success to color
    df["color"] = df["success"].map({1: "green", 0: "red"})

    # --- Plot actions over time ---
    plt.figure(figsize=(12, 5))
    plt.scatter(df["time"], df["action"], c=df["color"],
                s=70, alpha=0.7, edgecolor="k", linewidth=0.5)

    plt.xlabel("Time (m)", fontsize=16)
    plt.ylabel("Action", fontsize=16)
    plt.title(f"RL-ASL Decisions for Node {node_id}", fontsize=18)
    plt.grid(True, linestyle="--", alpha=0.6)

    # Legend
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


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Plot Q-Learning results from JSON files")
    parser.add_argument("file", help="Path to the JSON file to compare")
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

    # --- Load JSON ---
    file_path = Path(args.file)
    if not file_path.exists():
        print(f"Error: File not found at {file_path}")
        sys.exit(1)

    if file_path.stat().st_size == 0:
        print(f"Error: JSON file '{file_path}' is empty. Skipping processing.")
        sys.exit(1)

    try:
        with open(file_path, "r") as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"Error decoding JSON from '{file_path}': {e}")
        sys.exit(1)

    # Loop through nodes
    for node_id, node_data in data["0"].items():
        if node_id == "network":
            continue
        samples = node_data.get("episode_monitoring", {}).get("samples", {})
        if not samples:
            print(
                f"No episode monitoring samples for node {node_id}. Skipping.")
            continue

        df = pd.DataFrame.from_dict(samples, orient="index").astype(float)
        df.index = df.index.astype(int)
        df = df.sort_index()

        # --- Combined reward + epsilon plot ---
        plt.figure(figsize=(10, 5))
        ax1 = sns.lineplot(
            x=df.index, y=df["episode_reward"], label="Episode Reward")
        sns.lineplot(x=df.index, y=df["avg_reward"], label="Avg Reward")

        ax2 = ax1.twinx()
        sns.lineplot(x=df.index, y=df["epsilon"], ax=ax2, label="Epsilon",
                     color="red", linestyle=":")
        ax2.set_ylabel("Epsilon", fontsize=16)

        ax1.set_xlabel("Episode", fontsize=16)
        ax1.set_ylabel("Reward", fontsize=16)

        ax1.legend(loc="upper left", fontsize=12)
        ax2.legend(loc="upper right", fontsize=12)
        plt.grid(True, linestyle="--", alpha=0.6)
        plt.tight_layout()

        out_file = output_folder / f"{node_id}_reward_epsilon.png"
        plt.savefig(out_file)
        plt.close()
        print(f"Saved {out_file}")

        # RL-ASL plot
        plot_rl_asl(node_id, node_data, output_folder, xlim=args.xlim)
