import os
import json
import argparse
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

sns.set(style="whitegrid")


def save_plot(df, x, y, xlabel, ylabel, filename, node_id, output_folder,
              label=None, color=None, linestyle="-", secondary_y=None):
    """Helper to save line plots."""
    plt.figure(figsize=(10, 5))

    ax = sns.lineplot(x=x, y=y, data=df, label=label or y, color=color, linestyle=linestyle)

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

    out_file = os.path.join(output_folder, f"{node_id}_{filename}.png")
    plt.savefig(out_file)
    plt.close()
    print(f"Saved {out_file}")


def rolling_mean(series, window=20):
    """Return rolling mean (for smoothing)."""
    return series.rolling(window=window, min_periods=1).mean()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot Q-Learning results from JSON files")
    parser.add_argument("file", help="Path to the JSON file to compare")
    parser.add_argument(
        "-o", "--output-folder", type=str, default="Q-Learning-plots",
        help="Folder to save the plots"
    )
    args = parser.parse_args()

    os.makedirs(args.output_folder, exist_ok=True)

    # --- Load JSON ---
    file_path = args.file
    if not os.path.exists(file_path):
        print(f"Error: File not found at {file_path}")
        exit(1)

    if os.path.getsize(file_path) == 0:
        print(f"Error: JSON file '{file_path}' is empty. Skipping processing.")
        exit(1)

    try:
        with open(file_path, "r") as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"Error decoding JSON from '{file_path}': {e}")
        exit(1)

    # Loop through nodes
    for node_id, node_data in data["0"].items():
        if node_id == "network":
            continue
        samples = node_data.get("episode_monitoring", {}).get("samples", {})
        if len(samples) == 0:
            print(f"No episode monitoring samples found for node {node_id}. Skipping.")
            continue

        # --- Convert to DataFrame ---
        df = pd.DataFrame.from_dict(samples, orient="index").astype(float)
        df.index = df.index.astype(int)   # ensure integer episodes
        df = df.sort_index()

        # Add smoothed reward
        # df["smoothed_reward"] = rolling_mean(df["episode_reward"])

        # --- Individual plots ---
        # save_plot(df, df.index, "episode_reward", "Episode", "Reward",
        #           "rewards", node_id, args.output_folder, label="Episode Reward")
        # save_plot(df, df.index, "avg_reward", "Episode", "Reward",
        #           "avg_rewards", node_id, args.output_folder, label="Avg Reward", linestyle="--")
        # save_plot(df, df.index, "epsilon", "Episode", "Epsilon",
        #           "epsilon", node_id, args.output_folder, label="Epsilon", color="red")
        # save_plot(df, df.index, "steps", "Episode", "Steps",
        #           "steps", node_id, args.output_folder, label="Steps per Episode", color="green")

        # --- Combined reward + epsilon plot ---
        plt.figure(figsize=(10, 5))
        ax1 = sns.lineplot(x=df.index, y=df["episode_reward"], label="Episode Reward")
        # sns.lineplot(x=df.index, y=df["smoothed_reward"], label="Smoothed Reward", linestyle="--")
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

        out_file = os.path.join(args.output_folder, f"{node_id}_reward_epsilon.png")
        plt.savefig(out_file)
        plt.close()
        print(f"Saved {out_file}")
