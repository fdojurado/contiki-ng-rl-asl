from matplotlib.colors import to_rgba
from scipy.stats import norm
import argparse
import json
import os
import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd
import numpy as np


sns.set(style="whitegrid")

# Fonts
Y_LABEL_FONT_SIZE = 27
X_LABEL_FONT_SIZE = 23
X_TICK_FONT_SIZE = 17
Y_TICK_FONT_SIZE = 20
X_TICK_HEATMAP_FONT_SIZE = 10
Y_TICK_HEATMAP_FONT_SIZE = 10
LEGEND_FONT_SIZE = 18
STANDARD_FIG_SIZE = (6, 3)

# Colors & labels per protocol
DATA_INFO = {
    # Your approaches (SAGE) – strong, consistent blue tones, full opacity
    "RL-ASL": {"color": "#1f77b4", "label": "RL-ASL", "alpha": 1.0, "hatch": ""},
    # Baselines – distinct colors, but with transparency to look less strong
    # green
    "Orchestra": {"color": "#2ca02c", "label": "Orch.-RB", "alpha": 0.6},
    # orange
    "Orchestra-link-based": {"color": "#ff7f0e", "label": "Orch.-LB", "alpha": 0.6},
    # purple
    "RL-ASL-link-based": {"color": "#9467bd", "label": "RL-ASL-LB", "alpha": 0.6}
}


def get_label_color(label): return DATA_INFO.get(
    label, {}).get("color", "#000000")


def get_label_alpha(label): return DATA_INFO.get(label, {}).get("alpha", 1.0)


def get_label_name(label): return DATA_INFO.get(label, {}).get("label", label)
def get_label_hatch(label): return DATA_INFO.get(label, {}).get("hatch", None)


# Map metrics to JSON keys with more descriptive info
METRIC_INFO = {
    "power": {
        "suffix": "mW",
        "label_with_units": "Power [mW]",
        "label_no_units": "Power",
        "sort": "asc",
        "scale": 1.0,      # mW
        "plot_styles": {
            "bar": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": True,
                "show_labels": {"x": False, "y": True}
            },
            "line": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
            },
            "scatter": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
            }
        }
    },
    "energy": {
        "suffix": "mJ",
        "label_with_units": "Energy [J]",
        "label_no_units": "Energy",
        "sort": "asc",
        "scale": 1e-3,
        "plot_styles": {
            "bar": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 9, "xtick": 14, "ytick": 8, "legend": 16},
                "legend": False,
                "show_labels": {"x": False, "y": True}
            },
            "line": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
            }
        }
    },
    "latency": {
        "suffix": "ms",
        "label_with_units": "Latency [s]",
        "label_no_units": "Latency",
        "sort": "asc",
        "scale": 1e-3,
        "plot_styles": {
            "bar": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 9, "xtick": 14, "ytick": 8, "legend": 16},
                "legend": False,
                "show_labels": {"x": False, "y": True}
            },
            "line": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
            },
            "violin": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
                "show_labels": {"x": False, "y": True}
            },
            "histogram": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
            },
            "cdf": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
            },
            "scatter": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
            }
        }
    },
    "jitter": {
        "suffix": "ms",
        "label_with_units": "Jitter [s]",
        "label_no_units": "Jitter",
        "sort": "asc",
        "scale": 1e-3,
        "plot_styles": {
            "bar": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 18, "ylabel": 22, "xtick": 14, "ytick": 14, "legend": 16}
            },
            "line": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
            },
            "violin": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
                "show_labels": {"x": False, "y": True}
            },
            "histogram": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
            },
            "cdf": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
            }
        }
    },
    "packet_delivery_ratio": {
        "suffix": "",
        "label_with_units": "PDR [%]",
        "label_no_units": "PDR",
        "sort": "desc",
        "scale": 100,
        "ylim": (80, 100),
        "plot_styles": {
            "bar": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
                "show_labels": {"x": False, "y": True}
            },
            "line": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
            }
        }
    },
    "rdc": {
        "suffix": "",
        "label_with_units": "RDC [%]",
        "label_no_units": "RDC",
        "sort": "asc",
        "scale": 100.0,
        "plot_styles": {
            "bar": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": True,
                "show_labels": {"x": False, "y": True}
            },
            "line": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
            },
            "violin": {
                "figsize": (2, 1.5),
                "fonts": {"xlabel": 10, "ylabel": 0, "xtick": 7, "ytick": 11, "legend": 18},
                "legend": False,
                "show_labels": {"x": True, "y": False}
            },
            "scatter": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 22, "xtick": 20, "ytick": 20, "legend": 18},
                "legend": False,
            }
        }
    },
    "heatmap": {
        "suffix": "",
        "label_with_units": "Heatmap",
        "label_no_units": "Heatmap",
        "sort": "asc",
        "plot_styles": {
            "heatmap": {
                "figsize": (6, 4),
                "fonts": {"xlabel": 20, "ylabel": 20, "xtick": 18, "ytick": 18, "legend": 18,
                          "color_bar": 17},
                "legend": False,
                "show_labels": {"x": False, "y": True}
            }
        }
    },
    "radar": {
        "suffix": "",
        "label_with_units": "Radar",
        "label_no_units": "Radar",
        "sort": "asc",
        "plot_styles": {
            "radar": {
                "figsize": (9, 5),
                "fonts": {"xlabel": 20, "ylabel": 20, "xtick": 26, "ytick": 22, "legend": 25},
                "legend": True,
                "show_labels": {"x": False, "y": True}
            }
        }
    }
}


def load_json(file):
    with open(file, "r") as f:
        data = json.load(f)
    label = list(data.keys())[0]
    return data[label], label


def show_axis_label(metric, axis, plot_type=None, default=True):
    """
    Decide whether to show x/y label for a given metric + plot type.
    axis = "x" or "y".
    """
    info = METRIC_INFO.get(metric, {})
    styles = info.get("plot_styles", {})
    if plot_type and plot_type in styles:
        show_cfg = styles[plot_type].get("show_labels", {})
        return show_cfg.get(axis, default)
    return default


def get_fontsize(metric, key, default=None, plot_type=None):
    """Fetch font size for a given metric and plot type ('bar','line',etc.)."""
    info = METRIC_INFO.get(metric, {})
    plot_styles = info.get("plot_styles", {})
    if plot_type and plot_type in plot_styles:
        fonts = plot_styles[plot_type].get("fonts", {})
        return fonts.get(key, default)
    fonts = info.get("fonts", {})
    return fonts.get(key, default)


def get_figsize(metric, default=STANDARD_FIG_SIZE, plot_type=None):
    info = METRIC_INFO.get(metric, {})
    plot_styles = info.get("plot_styles", {})
    if plot_type and plot_type in plot_styles:
        return plot_styles[plot_type].get("figsize", default)
    return info.get("figsize", default)


def use_legend(metric, plot_type=None, default=True):
    info = METRIC_INFO.get(metric, {})
    info_plot_styles = info.get("plot_styles", {})
    if plot_type and plot_type in info_plot_styles:
        return info_plot_styles[plot_type].get("legend", default)
    return info.get("legend", default)


def get_metric_values(net, metric):
    info = METRIC_INFO[metric]
    suffix = info.get("suffix", "")
    scale = info.get("scale", 1.0)

    if suffix:
        avg = net["network"][metric][f"avg_{suffix}"] * scale
        std = net["network"][metric][f"std_{suffix}"] * scale
    else:
        avg = net["network"][metric]["avg"] * scale
        std = net["network"][metric]["std"] * scale

    return avg, std


def get_palette(labels):
    return {
        get_label_name(lbl): to_rgba(
            DATA_INFO[lbl]["color"],
            DATA_INFO[lbl].get("alpha", 1.0)
        )
        for lbl in labels
    }


def apply_axis_limits(ax, metric):
    info = METRIC_INFO[metric]
    if "ylim" in info:
        ax.set_ylim(info["ylim"])


def style_axes(ax, metric=None, plot_type=None):
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_linewidth(3)
    ax.spines["left"].set_edgecolor("black")
    ax.spines["bottom"].set_linewidth(3)
    ax.spines["bottom"].set_edgecolor("black")

    if metric:
        ax.tick_params(axis="y", labelsize=get_fontsize(
            metric, "ytick", Y_TICK_FONT_SIZE, plot_type=plot_type))
        ax.tick_params(axis="x", labelsize=get_fontsize(
            metric, "xtick", X_TICK_FONT_SIZE, plot_type=plot_type))
    else:
        ax.tick_params(axis="y", labelsize=Y_TICK_FONT_SIZE)
        ax.tick_params(axis="x", labelsize=X_TICK_FONT_SIZE)


def plot_bar_with_error(networks, labels, metric, output_folder):
    info = METRIC_INFO[metric]

    # --- collect avg/std with scaling ---
    avg_vals, std_vals = [], []
    for net in networks:
        avg, std = get_metric_values(net, metric)
        avg_vals.append(avg)
        std_vals.append(std)

    # --- sort protocols by avg value according to 'sort' field ---
    combined = list(zip(labels, avg_vals, std_vals))
    reverse_sort = True if info.get("sort", "asc") == "desc" else False
    combined.sort(key=lambda x: x[1], reverse=reverse_sort)
    labels, avg_vals, std_vals = zip(*combined)

    # --- map labels through DATA_INFO ---
    pretty_labels = [get_label_name(lbl) for lbl in labels]

    # --- calculate compact figure size ---
    n = len(avg_vals)
    # bar_width = 0.8  # bars nearly touching
    # fig_width = max(1.0, n * bar_width * 0.6)  # scale with #protocols
    # fig_height = 1.0

    fig, ax = plt.subplots(figsize=get_figsize(metric, plot_type="bar"))
    colors = [get_label_color(lbl) for lbl in labels]
    hatches = [get_label_hatch(lbl) for lbl in labels]
    alphas = [get_label_alpha(lbl) for lbl in labels]
    positions = np.arange(n)

    bars = ax.bar(
        positions, avg_vals, yerr=std_vals, capsize=4,
        color=colors, edgecolor="black", linewidth=2,
    )
    # Set hatch and alpha for each bar individually
    for bar, hatch, alpha in zip(bars, hatches, alphas):
        if hatch is not None:
            bar.set_hatch(hatch)
        bar.set_alpha(alpha)

    # compact x-limits (remove white padding)
    ax.set_xlim(-0.5, n - 0.5)

    # y-axis label
    if show_axis_label(metric, "y", plot_type="bar"):
        ax.set_ylabel(info["label_with_units"],
                      fontsize=get_fontsize(metric, "ylabel", Y_LABEL_FONT_SIZE, plot_type="bar"))
    else:
        ax.set_ylabel("")

    # remove x tick labels (since legend is used)
    ax.set_xticks([])
    ax.set_xticklabels([])

    if use_legend(metric, plot_type="bar"):
        ax.legend(
            bars, pretty_labels,
            fontsize=get_fontsize(metric, "legend", LEGEND_FONT_SIZE),
            loc="best"
        )

    style_axes(ax, metric, plot_type="bar")
    apply_axis_limits(ax, metric)
    ax.xaxis.grid(False)
    fig.tight_layout()

    fig.savefig(os.path.join(output_folder, f"{metric}_bar.pdf"), format="pdf",
                bbox_inches="tight", pad_inches=0.0)
    plt.close(fig)


def plot_per_sample_line(networks, labels, metric, output_folder):
    info = METRIC_INFO[metric]
    suffix = info.get("suffix", "")
    scale = info.get("scale", 1.0)

    fig, ax = plt.subplots(figsize=get_figsize(metric, plot_type="line"))
    for net, label in zip(networks, labels):
        if suffix:
            per_sample = net["network"][metric][f"per_sample_avg_{suffix}"]
        else:
            per_sample = net["network"][metric]["per_sample_avg"]

        seqs = sorted(map(int, per_sample.keys()))
        values = [per_sample[str(s)]["avg"] * scale for s in seqs]
        stds = [per_sample[str(s)]["std"] * scale for s in seqs]

        color = get_label_color(label)
        alpha = get_label_alpha(label)
        ax.plot(seqs, values, marker="o",
                label=get_label_name(label), color=color, alpha=alpha,
                linewidth=1, markersize=2)

        # --- add shadow / error band ---
        ax.fill_between(seqs,
                        np.array(values) - np.array(stds),
                        np.array(values) + np.array(stds),
                        alpha=0.2, color=color)

    if show_axis_label(metric, "x", plot_type="line"):
        ax.set_xlabel("Sample", fontsize=get_fontsize(
            metric, "xlabel", X_LABEL_FONT_SIZE, plot_type="line"))
    else:
        ax.set_xlabel("")

    if show_axis_label(metric, "y", plot_type="line"):
        ax.set_ylabel(info["label_with_units"], fontsize=get_fontsize(
            metric, "ylabel", Y_LABEL_FONT_SIZE, plot_type="line"))
    else:
        ax.set_ylabel("")

    if use_legend(metric, plot_type="line"):
        ax.legend(fontsize=get_fontsize(
            metric, "legend", LEGEND_FONT_SIZE),
            loc="best")
    style_axes(ax, metric, plot_type="line")
    apply_axis_limits(ax, metric)
    fig.tight_layout()
    fig.savefig(os.path.join(output_folder,
                f"{metric}_per_sample.pdf"), format="pdf",
                bbox_inches="tight", pad_inches=0.0)
    plt.close(fig)


def plot_scatter_rdc_vs_latency(networks, labels, output_folder):
    metric_x = "rdc"
    metric_y = "latency"

    fig, ax = plt.subplots(figsize=get_figsize(metric_x, plot_type="scatter"))
    for net, label in zip(networks, labels):
        rdc, _ = get_metric_values(net, metric_x)
        latency, _ = get_metric_values(net, metric_y)

        ax.scatter(rdc, latency,
                   label=get_label_name(label),
                   color=get_label_color(label),
                   alpha=get_label_alpha(label),
                   linewidths=2,
                   edgecolor="black", s=200)

    # axis labels come from METRIC_INFO (with scale applied)
    ax.set_xlabel(METRIC_INFO[metric_x]
                  ["label_with_units"], fontsize=get_fontsize(
                      metric_x, "xlabel", X_LABEL_FONT_SIZE, plot_type="scatter"))
    ax.set_ylabel(METRIC_INFO[metric_y]
                  ["label_with_units"], fontsize=get_fontsize(
                      metric_y, "ylabel", Y_LABEL_FONT_SIZE, plot_type="scatter"))

    if use_legend(metric_x, plot_type="scatter"):
        ax.legend(fontsize=get_fontsize(
            metric_x, "legend", LEGEND_FONT_SIZE, plot_type="scatter"))

    style_axes(ax, metric_x, plot_type="scatter")

    fig.tight_layout()

    fig.savefig(os.path.join(output_folder,
                "rdc_vs_latency.pdf"), format="pdf")
    plt.close(fig)


def plot_box_violin(networks, labels, metric, output_folder, kind="box"):
    """
    Create boxplot or violin plot of per-sample values across protocols.
    kind = "box" or "violin"
    """
    info = METRIC_INFO[metric]
    suffix = info.get("suffix", "")
    scale = info.get("scale", 1.0)

    # --- collect per-sample data across protocols ---
    data = []
    protocol_names = []
    avg_vals = []
    for net, label in zip(networks, labels):
        if suffix:
            per_sample = net["network"][metric][f"per_sample_avg_{suffix}"]
        else:
            per_sample = net["network"][metric]["per_sample_avg"]

        values = [v["avg"] * scale for v in per_sample.values()]
        data.extend(values)
        protocol_names.extend([get_label_name(label)] * len(values))

        # store average for sorting
        avg_vals.append(np.mean(values))

    # --- build dataframe for seaborn ---
    df = pd.DataFrame({"Protocol": protocol_names, metric: data})

    # --- sort protocols according to metric 'sort' key ---
    reverse_sort = True if info.get("sort", "asc") == "desc" else False
    combined = list(zip(labels, avg_vals))
    combined.sort(key=lambda x: x[1], reverse=reverse_sort)
    order = [get_label_name(lbl) for lbl, _ in combined]

    # --- plotting ---
    fig, ax = plt.subplots(figsize=get_figsize(metric, plot_type=kind))
    if kind == "box":
        sns.boxplot(
            x="Protocol", y=metric, hue="Protocol", data=df,
            dodge=False, legend=False,
            order=order,
            palette=get_palette(labels),
            ax=ax
        )
    else:
        sns.violinplot(
            x="Protocol", y=metric, hue="Protocol", data=df,
            dodge=False, legend=False, inner="quartile",
            order=order,
            palette=get_palette(labels),
            ax=ax
        )

    ax.set_xlabel("")

    if show_axis_label(metric, "y", plot_type=kind):
        ax.set_ylabel(info["label_with_units"], fontsize=get_fontsize(
            metric, "ylabel", Y_LABEL_FONT_SIZE, plot_type=kind))
    else:
        ax.set_ylabel("")

    if show_axis_label(metric, "x", plot_type=kind):
        ax.set_xlabel(info["label_with_units"], fontsize=get_fontsize(
            metric, "xlabel", X_LABEL_FONT_SIZE, plot_type=kind))
    else:
        ax.set_xlabel("")

     # remove x tick labels (since legend is used)
    ax.set_xticks([])
    ax.set_xticklabels([])

    style_axes(ax, metric, plot_type=kind)
    apply_axis_limits(ax, metric)

    fig.tight_layout()
    fig.savefig(os.path.join(output_folder,
                f"{metric}_{kind}.pdf"), format="pdf",
                bbox_inches="tight", pad_inches=0.0)
    plt.close(fig)


def plot_cdf(networks, labels, metric, output_folder):
    """
    Plot empirical CDF for a given metric across protocols.
    Useful for latency/jitter (deadline satisfaction view).
    """
    info = METRIC_INFO[metric]
    suffix = info.get("suffix", "")
    scale = info.get("scale", 1.0)

    fig, ax = plt.subplots(figsize=get_figsize(metric, plot_type="cdf"))

    for net, label in zip(networks, labels):
        # --- extract all per-sample values ---
        if suffix:
            per_sample = net["network"][metric][f"per_sample_avg_{suffix}"]
        else:
            per_sample = net["network"][metric]["per_sample_avg"]

        values = np.array([v["avg"] * scale for v in per_sample.values()])
        values = np.sort(values)

        # --- compute CDF ---
        yvals = np.arange(1, len(values) + 1) / float(len(values))

        ax.step(values, yvals,
                label=get_label_name(label),
                color=get_label_color(label),
                alpha=get_label_alpha(label),
                linewidth=2)

    ax.set_xlabel(info["label_with_units"], fontsize=get_fontsize(
        metric, "xlabel", X_LABEL_FONT_SIZE, plot_type="cdf"))
    ax.set_ylabel("CDF", fontsize=get_fontsize(
        metric, "ylabel", Y_LABEL_FONT_SIZE, plot_type="cdf"))

    if use_legend(metric, plot_type="cdf"):
        ax.legend(fontsize=get_fontsize(
            metric, "legend", LEGEND_FONT_SIZE, plot_type="cdf"), loc="best")

    style_axes(ax, metric, plot_type='line')

    fig.tight_layout()
    fig.savefig(os.path.join(output_folder, f"{metric}_cdf.pdf"), format="pdf")
    plt.close(fig)


def plot_radar(networks, labels, output_folder):
    """
    Radar plot comparing latency, PDR, and power consumption across protocols.
    Metrics are normalized [0,1] where 1 = best.
    """

    # --- pick which metrics to include ---
    metrics = ["latency",
               "rdc", "packet_delivery_ratio"]

    # --- collect avg values per protocol ---
    data = {}
    for net, label in zip(networks, labels):
        values = []
        for metric in metrics:
            avg, _ = get_metric_values(net, metric)
            values.append(avg)
        data[get_label_name(label)] = values

    # --- normalize each metric across protocols (higher=better) ---
    values_matrix = np.array(list(data.values()))
    norm_matrix = []
    for i, metric in enumerate(metrics):
        col = values_matrix[:, i]
        if METRIC_INFO[metric]["sort"] == "asc":
            # lower is better → invert
            norm = 1 - (col - np.min(col)) / (np.max(col) - np.min(col) + 1e-9)
        else:
            # higher is better
            norm = (col - np.min(col)) / (np.max(col) - np.min(col) + 1e-9)
        norm_matrix.append(norm)
    norm_matrix = np.array(norm_matrix).T

    # --- radar plot setup ---
    num_vars = len(metrics)
    angles = np.linspace(0, 2 * np.pi, num_vars, endpoint=False).tolist()
    angles += angles[:1]  # close the loop

    fig, ax = plt.subplots(figsize=get_figsize("radar", plot_type="radar"),
                           subplot_kw=dict(polar=True))

    for (protocol, values), label in zip(zip(data.keys(), norm_matrix), labels):
        vals = values.tolist()
        vals += vals[:1]
        ax.plot(angles, vals, color=get_label_color(label), alpha=get_label_alpha(label),
                linewidth=2, label=protocol)
        ax.fill(angles, vals, color=get_label_color(label), alpha=0.25)

    # --- set axis labels ---
    ax.set_xticks(angles[:-1])
    ax.set_xticklabels([METRIC_INFO[m]["label_no_units"] for m in metrics],
                       fontsize=get_fontsize("radar", "xtick", X_TICK_FONT_SIZE, plot_type="radar"))

    ax.set_yticks([0.25, 0.5, 0.75, 1.0])
    ax.set_yticklabels(["0.25", "0.5", "0.75", "1.0"],
                       fontsize=get_fontsize("radar", "ytick", Y_TICK_FONT_SIZE, plot_type="radar"))
    ax.set_ylim(0, 1)

    if use_legend("radar", plot_type="radar"):
        ax.legend(
            loc="center left",
            bbox_to_anchor=(1.1, 0.5),   # (x, y) relative to axes
            fontsize=get_fontsize(
                "radar", "legend", LEGEND_FONT_SIZE, plot_type="radar")
        )

    fig.tight_layout()
    fig.savefig(os.path.join(output_folder, "radar_metrics.pdf"), format="pdf")
    plt.close(fig)


def plot_histogram_with_fit(networks, labels, metric, output_folder):
    """
    Plot histogram + fitted normal PDF for a given metric across protocols.
    Works well for latency, jitter, power, energy, duty_cycle.
    """
    info = METRIC_INFO[metric]
    suffix = info.get("suffix", "")
    scale = info.get("scale", 1.0)

    fig, ax = plt.subplots(figsize=get_figsize(metric, plot_type="histogram"))

    for net, label in zip(networks, labels):
        # --- extract per-sample values ---
        if suffix:
            per_sample = net["network"][metric][f"per_sample_avg_{suffix}"]
        else:
            per_sample = net["network"][metric]["per_sample_avg"]

        values = np.array([v["avg"] * scale for v in per_sample.values()])

        # --- histogram ---
        color = get_label_color(label)
        alpha = get_label_alpha(label)
        sns.histplot(values, bins=30, kde=False, stat="density",
                     color=color, alpha=alpha, ax=ax, label=get_label_name(label))

        # --- fit normal distribution ---
        if len(values) > 1:  # avoid fitting empty/single values
            mu, std = norm.fit(values)
            xmin, xmax = ax.get_xlim()
            x = np.linspace(xmin, xmax, 200)
            p = norm.pdf(x, mu, std)
            ax.plot(x, p, color=color, linewidth=2, alpha=alpha)

    ax.set_xlabel(info["label_with_units"], fontsize=get_fontsize(
        metric, "xlabel", X_LABEL_FONT_SIZE, plot_type="histogram"))
    ax.set_ylabel("Density", fontsize=get_fontsize(
        metric, "ylabel", Y_LABEL_FONT_SIZE, plot_type="histogram"))
    if use_legend(metric, plot_type="histogram"):
        ax.legend(fontsize=LEGEND_FONT_SIZE)
    style_axes(ax, metric, plot_type="histogram")

    fig.tight_layout()
    fig.savefig(os.path.join(output_folder,
                f"{metric}_hist_normal.pdf"), format="pdf")
    plt.close(fig)


def plot_protocol_metric_heatmap(networks, labels, output_folder):
    """
    Heatmap: protocols × metrics
    Rows = protocols, Columns = metrics
    Colors = normalized values (0=best, 1=worst).
    """

    metrics = list(METRIC_INFO.keys())  # all metrics
    # remove heatmap
    metrics.remove("heatmap")
    metrics.remove("radar")
    metrics.remove("power")
    metrics.remove("energy")
    metrics.remove("jitter")
    # metrics.remove("packet_delivery_ratio")

    # --- collect avg values ---
    data = {}
    for net, label in zip(networks, labels):
        values = []
        for metric in metrics:
            avg, _ = get_metric_values(net, metric)
            values.append(avg)
        data[get_label_name(label)] = values

    df = pd.DataFrame(data, index=metrics).T  # protocols × metrics

    # --- normalize each column [0,1] (higher = better) ---
    norm_df = pd.DataFrame(index=df.index, columns=df.columns)
    for metric in metrics:
        col = df[metric].values.astype(float)
        col_min, col_max = np.min(col), np.max(col)

        if col_max == col_min:
            norm = np.ones_like(col) * 0.5  # neutral if all equal
        else:
            norm = (col - col_min) / (col_max - col_min)

        if METRIC_INFO[metric].get("sort", "asc") == "asc":
            # lower is better → invert
            norm = 1 - norm

        norm_df[metric] = norm

    # --- make heatmap ---
    fig, ax = plt.subplots(figsize=get_figsize("heatmap", plot_type="heatmap"))
    heatmap = sns.heatmap(norm_df.astype(float),
                          annot=False, fmt=".2f", cmap="YlGnBu",
                          cbar_kws={"label": "Norm. Score [0=worst, 1=best]"},
                          linewidths=0.5, linecolor="gray", ax=ax)
    # Set colorbar label font size
    heatmap.figure.axes[-1].yaxis.label.set_size(get_fontsize(
        "heatmap", "color_bar", Y_LABEL_FONT_SIZE, plot_type="heatmap"))
    # Optionally, set colorbar tick label font size
    heatmap.figure.axes[-1].tick_params(labelsize=get_fontsize(
        "heatmap", "ytick", Y_TICK_HEATMAP_FONT_SIZE, plot_type="heatmap"))

    ax.set_xticklabels(
        # ticks=np.arange(len(metrics)) + 0.5,
        labels=[METRIC_INFO[m]["label_no_units"] for m in metrics],
        rotation=30, ha="right", fontsize=get_fontsize("heatmap", "xtick", X_TICK_HEATMAP_FONT_SIZE, plot_type="heatmap")
    )
    plt.yticks(
        fontsize=get_fontsize("heatmap", "ytick",
                              Y_TICK_HEATMAP_FONT_SIZE, plot_type="heatmap"),
        rotation=0, ha="right")

    #  ax.set_xticklabels([METRIC_INFO[m]["label_no_units"] for m in metrics],
    #                    rotation=30, ha="right", fontsize=get_fontsize("heatmap", "xtick", X_TICK_HEATMAP_FONT_SIZE, plot_type="heatmap"))
    # ax.set_yticklabels([METRIC_INFO[m]["label_no_units"] for m in metrics],
    #                    rotation=0, ha="right", fontsize=get_fontsize("heatmap", "ytick", Y_TICK_HEATMAP_FONT_SIZE, plot_type="heatmap"))

    if show_axis_label("heatmap", "y", plot_type="heatmap"):
        ax.set_ylabel("Protocols", fontsize=get_fontsize(
            "heatmap", "ylabel", Y_LABEL_FONT_SIZE, plot_type="heatmap"))
    else:
        ax.set_ylabel("")

    if show_axis_label("heatmap", "x", plot_type="heatmap"):
        ax.set_xlabel("Metrics", fontsize=get_fontsize(
            "heatmap", "xlabel", X_LABEL_FONT_SIZE, plot_type="heatmap"))
    else:
        ax.set_xlabel("")

    fig.tight_layout()
    fig.savefig(os.path.join(output_folder,
                "protocol_metric_heatmap.pdf"), format="pdf")
    plt.close()


from matplotlib.patches import Patch

def plot_stacked_bar(networks, labels, metric, output_folder):
    """
    Stacked bar chart for power consumption breakdown across protocols.
    RX is grouped: all subcomponents share the same color (different hatches),
    and a boundary outline shows the total RX block.
    """

    info = METRIC_INFO[metric]

    # --- define components in hierarchical order ---
    components = [
        "avg_cpu_mW",
        "avg_tx_mW",
        "avg_rx_non_uc_total_mW",
        "avg_rx_uc_mW",
        "avg_rx_uc_idle_mW",
    ]
    labels_comp = [
        "CPU",
        "TX",
        "RX non-UC",
        "RX UC active",
        "RX UC idle",
    ]

    # --- colors & hatches ---
    base_rx_color = "#C44E52"  # unified RX base color
    colors = {
        "avg_cpu_mW": "#4C72B0",         # CPU (blue)
        "avg_tx_mW": "#55A868",          # TX (green)
        "avg_rx_non_uc_total_mW": base_rx_color,
        "avg_rx_uc_mW": base_rx_color,
        "avg_rx_uc_idle_mW": base_rx_color,
    }
    hatches = {
        "avg_cpu_mW": "", 
        "avg_tx_mW": "//",
        "avg_rx_non_uc_total_mW": "\\\\",   # dark pattern
        "avg_rx_uc_mW": "xx",              # medium pattern
        "avg_rx_uc_idle_mW": "oo",         # light pattern
    }

    # --- collect data ---
    data = {comp: [] for comp in components}
    rx_total = []
    for net in networks:
        power = net["network"]["power"]
        for comp in components:
            data[comp].append(power.get(comp, 0.0))
        rx_total.append(power.get("avg_rx_mW", 0.0))

    n = len(labels)
    ind = np.arange(n)
    width = 0.6

    fig, ax = plt.subplots(figsize=get_figsize(metric, plot_type="bar"))

    # --- stacked bars ---
    bottom = np.zeros(n)
    for comp in components:
        ax.bar(
            ind,
            data[comp],
            width,
            bottom=bottom,
            label=comp,  # temp label (we'll override legend below)
            color=colors[comp],
            edgecolor="black",
            linewidth=1.2,
            hatch=hatches[comp]
        )
        bottom += np.array(data[comp])

    # --- RX total outline ---
    cpu_tx = np.array(data["avg_cpu_mW"]) + np.array(data["avg_tx_mW"])
    ax.bar(
        ind,
        rx_total,
        width,
        bottom=cpu_tx,
        color="none",
        edgecolor="red",
        linewidth=1.2,
        linestyle="--",
        label="RX (group)"
    )

    # --- axis labels ---
    if show_axis_label(metric, "y", plot_type="bar"):
        ax.set_ylabel(
            info["label_with_units"],
            fontsize=get_fontsize(
                metric, "ylabel", Y_LABEL_FONT_SIZE, plot_type="bar")
        )
    else:
        ax.set_ylabel("")

    if show_axis_label(metric, "x", plot_type="bar"):
        ax.set_xlabel("Protocols",
                      fontsize=get_fontsize(metric, "xlabel", X_LABEL_FONT_SIZE, plot_type="bar"))
    else:
        ax.set_xlabel("")

    # --- ticks & labels ---
    ax.set_xticks(ind)
    ax.set_xticklabels(labels, rotation=20, ha="right", fontsize=10)

    # --- grid ---
    ax.grid(axis="y", linestyle="--", alpha=0.7)

    # --- custom legend ---
    legend_elements = [
        Patch(facecolor="#4C72B0", edgecolor="black", label="CPU"),
        Patch(facecolor="#55A868", edgecolor="black", hatch="//", label="TX"),
        Patch(facecolor=base_rx_color, edgecolor="black", hatch="\\\\", label="RX non-UC"),
        Patch(facecolor=base_rx_color, edgecolor="black", hatch="xx", label="RX UC active"),
        Patch(facecolor=base_rx_color, edgecolor="black", hatch="oo", label="RX UC idle"),
        Patch(facecolor="none", edgecolor="red", linestyle="--", label="RX total (boundary)")
    ]

    ax.legend(
        handles=legend_elements,
        bbox_to_anchor=(1.01, 1),
        loc="upper left",
        frameon=False,
        fontsize=9
    )

    fig.tight_layout()
    output_path = os.path.join(output_folder, f"{metric}_stacked_bar.pdf")
    fig.savefig(output_path, format="pdf", bbox_inches="tight")
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Compare COOJA simulation results from multiple JSON files."
    )
    parser.add_argument("files", nargs="+",
                        help="List of JSON files to compare")
    parser.add_argument("--labels", nargs="+",
                        help="Optional labels for each file")
    parser.add_argument(
        "--output-folder", type=str, default="plots", help="Folder to save the plots"
    )

    args = parser.parse_args()

    if args.labels and len(args.labels) != len(args.files):
        parser.error("Number of labels must match number of files.")

    labels = args.labels if args.labels else []
    networks = []
    default_labels = []

    for idx, file in enumerate(args.files):
        data, label = load_json(file)
        networks.append(data)
        default_labels.append(label)

    if not labels:
        labels = default_labels

    os.makedirs(args.output_folder, exist_ok=True)

    # --- Bar plots with error bars ---
    for metric in METRIC_INFO.keys():
        # omit heatmap
        if metric == "heatmap" or metric == "radar":
            continue
        plot_bar_with_error(networks, labels, metric, args.output_folder)

    # --- Per-sequence line plots ---
    for metric in METRIC_INFO.keys():
        if metric == "heatmap" or metric == "radar":
            continue
        plot_per_sample_line(networks, labels, metric, args.output_folder)

     # --- Box/Violin plots for distributions ---
    for metric in METRIC_INFO.keys():
        if metric == "heatmap" or metric == "radar":
            continue
        plot_box_violin(networks, labels, metric,
                        args.output_folder, kind="box")
        plot_box_violin(networks, labels, metric,
                        args.output_folder, kind="violin")

    # --- CDF plots for deadline satisfaction (latency, jitter) ---
    for metric in ["latency", "jitter"]:
        plot_cdf(networks, labels, metric, args.output_folder)

    # --- Scatter plot: rdc vs latency ---
    plot_scatter_rdc_vs_latency(networks, labels, args.output_folder)

    # --- Radar plot: all metrics in one figure ---
    plot_radar(networks, labels, args.output_folder)

    for metric in ["latency", "jitter"]:
        plot_histogram_with_fit(networks, labels, metric, args.output_folder)

    # --- Protocol × Metric Heatmap ---
    plot_protocol_metric_heatmap(networks, labels, args.output_folder)

    # Lets plot stacked bar chart for power consumption breakdown
    metric = "power"
    info = METRIC_INFO[metric]
    plot_stacked_bar(networks, labels, metric, args.output_folder)
