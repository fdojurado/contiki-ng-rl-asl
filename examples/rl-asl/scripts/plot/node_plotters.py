"""
Per-node plotter implementations for analyzing individual node metrics.
"""

from scipy.stats import norm
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from matplotlib.patches import Patch

from metric_plotter import MetricPlotter


class NodeBarPlotter(MetricPlotter):
    """Creates bar plots for per-node metrics."""

    def plot(self, networks, labels, metric, output_folder, node_ids=None):
        """Create bar plot showing metric values for each node across protocols.
        Nodes are shown in ascending ID order. Within each node's grouped bars,
        the bars (protocols) are ordered by their value for that node.
        """
        fig, ax = self.create_figure(metric, "bar")

        # Extract node data for all protocols
        node_data = {}
        all_node_ids = set()

        for net, label in zip(networks, labels):
            node_data[label] = {}
            if 'nodes' in net:
                for node_id, node_info in net['nodes'].items():
                    if node_ids is None or int(node_id) in node_ids:
                        if metric in node_info:
                            node_data[label][int(node_id)] = self._extract_node_metric(
                                node_info, metric)
                            all_node_ids.add(int(node_id))

        if not all_node_ids:
            print(f"No node data found for metric {metric}")
            return

        # Sort nodes by ID
        sorted_node_ids = sorted(all_node_ids)
        n_nodes = len(sorted_node_ids)

        # Determine maximum number of bars that can appear in any single node group
        max_bars_per_node = 0
        for node_id in sorted_node_ids:
            cnt = sum(
                1 for label in labels if node_id in node_data.get(label, {}))
            max_bars_per_node = max(max_bars_per_node, cnt)
        if max_bars_per_node == 0:
            print(f"No node data found for metric {metric}")
            return

        # width per bar (keep groups within 0.8 width)
        width = 0.8 / max_bars_per_node
        x = np.arange(n_nodes)

        # Keep track of which protocol labels have been added to the legend
        legend_added = set()

        # If metric is latency, use logarithmic y-scale and ensure non-zero positive values
        use_log_y = (metric == "latency")
        # small positive epsilon to replace non-positive values for log scale
        eps = 1e-6

        # For each node, sort available protocol bars by value and draw them centered around x[node_index]
        for node_idx, node_id in enumerate(sorted_node_ids):
            # collect (label, value) for protocols that have data for this node
            per_node = []
            for label in labels:
                value = node_data.get(label, {}).get(node_id, None)
                if value is not None:
                    per_node.append((label, value))

            if not per_node:
                # nothing to plot for this node
                continue

            # sort bars by value (ascending). Use key=lambda p: p[1]
            per_node.sort(key=lambda p: p[1])

            k = len(per_node)
            # offsets to center the group around the node x position
            offsets = (np.arange(k) - (k - 1) / 2.0) * width

            for i, (label, value) in enumerate(per_node):
                color = self.config.get_label_color(label)
                alpha = self.config.get_label_alpha(label)
                scale = self.config.METRIC_INFO[metric].get("scale", 1.0)
                value *= scale

                # if log scale requested, replace non-positive values with a small positive epsilon
                if use_log_y and value <= 0:
                    value = eps

                pretty_label = self.config.get_label_name(label)

                lbl = pretty_label if pretty_label not in legend_added else None
                if lbl is not None:
                    legend_added.add(pretty_label)

                ax.bar(x[node_idx] + offsets[i], value, width,
                       label=lbl, color=color, alpha=alpha,
                       edgecolor="black", linewidth=1)

        # If metric is latency, set logarithmic y-scale AFTER plotting
        if use_log_y:
            ax.set_yscale("log")
            # set the y-limits to [0, 20]
            ax.set_ylim(bottom=eps, top=20.0)

        # Style the plot
        ax.set_xlabel("Node ID")
        ax.set_xticks(x)
        ax.set_xticklabels([f"Node {nid}" for nid in sorted_node_ids])

        # Use styler to add labels/legend/styles (legend will reflect first appearance of each protocol)
        self.styler.set_axis_labels(ax, metric, "bar", x_label="Node ID")
        self.styler.add_legend(ax, metric, "bar")
        self.styler.style_axes(ax, metric, "bar")

        self.save_plot(fig, f"{metric}_per_node_bar.pdf", output_folder)

    def _extract_node_metric(self, node_info, metric):
        """Extract metric value from node data structure."""
        if metric == "power":
            return node_info.get("power", {}).get("avg_mW", 0)
        elif metric == "latency":
            return node_info.get("latency", {}).get("avg_ms", 0)
        elif metric == "jitter":
            return node_info.get("jitter", {}).get("avg", 0)
        elif metric == "packet_delivery_ratio":
            return node_info.get("packet_delivery_ratio", {}).get("avg", 0)
        return node_info.get(metric, 0)


class NodeStackedBarPlotter(MetricPlotter):
    """Creates stacked bar plots for per-node power breakdown across protocols."""

    def plot(self, networks, labels, metric, output_folder, node_ids=None):
        print(f"Creating stacked bar plot for metric: {metric}")
        if metric != "power":
            print(
                f"Stacked bar plot is only implemented for 'power' metric, skipping {metric}.")
            return

        # ---- Power components to plot ----
        components = [
            "avg_cpu_mW", "avg_tx_mW", "avg_rx_non_uc_total_mW",
            "avg_rx_uc_mW", "avg_rx_uc_idle_mW"
        ]

        # ---- Colors and hatches ----
        base_rx_color = "#C44E52"
        colors = {
            "avg_cpu_mW": "#4C72B0",
            "avg_tx_mW": "#55A868",
            "avg_rx_non_uc_total_mW": base_rx_color,
            "avg_rx_uc_mW": base_rx_color,
            "avg_rx_uc_idle_mW": base_rx_color,
        }
        hatches = {
            "avg_cpu_mW": "",
            "avg_tx_mW": "//",
            "avg_rx_non_uc_total_mW": "\\\\\\\\",
            "avg_rx_uc_mW": "xx",
            "avg_rx_uc_idle_mW": "oo",
        }

        # ---- Collect per-node, per-protocol power data ----
        node_data = {}
        all_node_ids = set()

        for net, label in zip(networks, labels):
            pretty_label = self.config.get_label_name(label)
            if "nodes" not in net:
                continue
            for node_id, node_info in net["nodes"].items():
                node_id = int(node_id)
                if node_ids is not None and node_id not in node_ids:
                    continue

                power_info = node_info.get("power", {})
                if not power_info:
                    continue

                if node_id not in node_data:
                    node_data[node_id] = {}
                # store original label as well so we can retrieve colors/alpha later
                entry = {comp: power_info.get(comp, 0.0) for comp in components}
                entry["avg_rx_mW"] = power_info.get("avg_rx_mW", 0.0)
                entry["_orig_label"] = label
                node_data[node_id][pretty_label] = entry
                all_node_ids.add(node_id)

        if not all_node_ids:
            print("No node power data found for stacked bar plot.")
            return

        # ---- Plot one figure per node ----
        for node_id in sorted(all_node_ids):
            fig, ax = plt.subplots(figsize=(5, 3))

            # Determine protocols available for this node and sort them by chosen metric value (total power)
            available = node_data.get(node_id, {})
            if not available:
                continue

            # compute total power per protocol (CPU + TX + RX total) and sort descending
            def total_power(proto_entry):
                return (
                    proto_entry.get("avg_cpu_mW", 0.0)
                    + proto_entry.get("avg_tx_mW", 0.0)
                    + proto_entry.get("avg_rx_mW", 0.0)
                )

            # available is mapping pretty_label -> entry
            sorted_protocols = sorted(
                available.items(), key=lambda kv: total_power(kv[1]), reverse=False
            )
            pretty_labels = [kv[0] for kv in sorted_protocols]
            n_protocols = len(pretty_labels)
            x = np.arange(n_protocols)
            width = 0.6

            for p_idx, pretty_label in enumerate(pretty_labels):
                node_entry = available.get(pretty_label, None)
                if not node_entry:
                    continue

                orig_label = node_entry.get("_orig_label")
                color = self.config.get_label_color(orig_label)
                alpha = self.config.get_label_alpha(orig_label)

                # Prepare stacked data
                bottoms = 0.0
                for comp in components:
                    value = node_entry.get(comp, 0.0)
                    ax.bar(
                        p_idx,
                        value,
                        width,
                        bottom=bottoms,
                        color=colors[comp],
                        edgecolor="black",
                        linewidth=1,
                        hatch=hatches[comp],
                        alpha=0.85,
                    )
                    bottoms += value

                # RX total outline (fully opaque)
                cpu_tx = node_entry.get("avg_cpu_mW", 0) + node_entry.get("avg_tx_mW", 0)
                rx_total = node_entry.get("avg_rx_mW", 0)
                ax.bar(
                    p_idx,
                    rx_total,
                    width,
                    bottom=cpu_tx,
                    color="none",
                    edgecolor="red",
                    linewidth=2.8,
                    linestyle="--",
                    alpha=1.0,
                    zorder=5,
                    label=f"{pretty_label} RX total",
                )

            # ---- Style axes ----
            ax.set_xticks(x)
            ax.set_xticklabels(pretty_labels, rotation=0, ha="center")
            # ax.set_title(f"Node {node_id} - Power Breakdown", fontsize=18)
            self.styler.set_axis_labels(
                ax, metric, "stacked_bar", x_label="Protocol", y_label="Power [mW]"
            )
            ax.spines["top"].set_visible(False)
            ax.spines["right"].set_visible(False)
            ax.spines["left"].set_linewidth(3)
            ax.spines["left"].set_edgecolor("black")
            ax.spines["bottom"].set_linewidth(3)
            ax.spines["bottom"].set_edgecolor("black")
            ax.tick_params(axis="y", labelsize=18)
            ax.tick_params(axis="x", labelsize=13)
            # ax.grid(axis="y", linestyle="--", alpha=0.7)

            # ---- Save and close ----
            filename = f"{metric}_node_{node_id}_stacked_bar.pdf"
            self.save_plot(fig, filename, output_folder)
            plt.close(fig)
            print(f"  → Saved {filename}")


class NodeHistogramPlotter(MetricPlotter):
    """Creates per-node histograms showing latency distributions per protocol."""

    def plot(self, networks, labels, metric, output_folder, node_ids=None):
        """
        Create one subplot per node, where each subplot shows the latency
        distribution for that node across all protocols.
        """
        node_data = {}
        all_node_ids = set()

        # ---- Collect per-node samples ----
        for net, label in zip(networks, labels):
            if "nodes" not in net:
                continue

            for node_id, node_info in net["nodes"].items():
                node_id = int(node_id)
                if node_ids is not None and node_id not in node_ids:
                    continue

                # Extract per-node samples
                per_sample, scale = self.get_per_sample_data(
                    net, metric, node_id=node_id)
                if not per_sample:
                    continue

                values = np.array(
                    [v["avg"] * scale for v in per_sample.values()])
                if values.size == 0:
                    continue

                if node_id not in node_data:
                    node_data[node_id] = {}
                node_data[node_id][label] = values
                all_node_ids.add(node_id)

        # ---- Check if data exists ----
        if not all_node_ids:
            return

        sorted_node_ids = sorted(all_node_ids)
        n_nodes = len(sorted_node_ids)

        # ---- Create one subplot per node ----
        fig, axes = plt.subplots(
            n_nodes, 1, figsize=(7, 3 * n_nodes), sharex=True
        )
        if n_nodes == 1:
            axes = [axes]

        # ---- Plot per-node histograms ----
        for ax, node_id in zip(axes, sorted_node_ids):
            for label, values in node_data[node_id].items():
                color = self.config.get_label_color(label)
                alpha = self.config.get_label_alpha(label)
                pretty_label = self.config.get_label_name(label)

                sns.histplot(
                    values, bins=30, kde=False, stat="density",
                    label=pretty_label, color=color, alpha=alpha, ax=ax
                )

                # Fit and plot Gaussian curve
                if len(values) > 1:
                    mu, std = norm.fit(values)
                    xmin, xmax = ax.get_xlim()
                    x = np.linspace(xmin, xmax, 200)
                    p = norm.pdf(x, mu, std)
                    ax.plot(x, p, color=color, linewidth=2, alpha=alpha)

            ax.set_title(f"Node {node_id}")
            ax.set_ylabel("Density")
            ax.legend()

        # ---- Common x-label ----
        xlabel = self.config.METRIC_INFO.get(metric, {}).get(
            "label_with_units", metric.replace("_", " ").title()
        )
        axes[-1].set_xlabel(xlabel)

        # ---- Figure title and layout ----
        fig.suptitle(
            f"{metric.replace('_', ' ').title()} Distribution per Node",
            fontsize=14
        )
        fig.tight_layout(rect=[0, 0, 1, 0.96])

        # ---- Save plot ----
        self.save_plot(fig, f"{metric}_per_node_histograms.pdf", output_folder)
        plt.close(fig)


class NodeHeatmapPlotter(MetricPlotter):
    """Creates heatmap showing nodes vs protocols for a specific metric."""

    def plot(self, networks, labels, metric, output_folder, node_ids=None):
        """Create heatmap showing metric values across nodes and protocols."""
        # Extract node data
        data = []
        node_labels = []
        protocol_labels = []

        for net, label in zip(networks, labels):
            if 'nodes' in net:
                for node_id, node_info in net['nodes'].items():
                    if node_ids is None or int(node_id) in node_ids:
                        if metric in node_info or self._has_metric_data(node_info, metric):
                            value = self._extract_node_metric(
                                node_info, metric)
                            data.append(value)
                            node_labels.append(f"Node {node_id}")
                            protocol_labels.append(
                                self.config.get_label_name(label))

        if not data:
            print(f"No node data found for metric {metric}")
            return

        # Create DataFrame
        df = pd.DataFrame({
            'Protocol': protocol_labels,
            'Node': node_labels,
            metric: data
        })

        # Pivot for heatmap
        pivot_df = df.pivot(index='Node', columns='Protocol', values=metric)

        # Create heatmap
        fig, ax = self.create_figure(metric, "heatmap")

        sns.heatmap(pivot_df, annot=True, fmt='.2f', cmap='YlOrRd',
                    cbar_kws={
                        'label': self.config.METRIC_INFO[metric]["label_with_units"]},
                    ax=ax)

        ax.set_title(f"{metric.replace('_', ' ').title()} per Node")

        self.save_plot(fig, f"{metric}_node_heatmap.pdf", output_folder)

    def _has_metric_data(self, node_info, metric):
        """Check if node has data for the given metric."""
        if metric == "power":
            return "power" in node_info
        elif metric == "latency":
            return "delay" in node_info
        elif metric == "jitter":
            return "jitter" in node_info
        elif metric == "packet_delivery_ratio":
            return "packet_delivery_ratio" in node_info
        return metric in node_info


class NodeScatterPlotter(MetricPlotter):
    """Creates scatter plots comparing metrics across nodes."""

    def plot(self, networks, labels, metric, output_folder):
        """Create scatter plot showing metric values vs node IDs (default implementation)."""
        self.plot_metric_vs_node(networks, labels, metric, output_folder)

    def plot_metric_vs_node(self, networks, labels, metric, output_folder):
        """Create scatter plot showing metric values vs node IDs."""
        fig, ax = self.create_figure(metric, "scatter")

        for net, label in zip(networks, labels):
            if 'nodes' in net:
                node_ids = []
                values = []

                for node_id, node_info in net['nodes'].items():
                    if self._has_metric_data(node_info, metric):
                        node_ids.append(int(node_id))
                        values.append(
                            self._extract_node_metric(node_info, metric))

                if node_ids:
                    color = self.config.get_label_color(label)
                    alpha = self.config.get_label_alpha(label)
                    pretty_label = self.config.get_label_name(label)

                    ax.scatter(node_ids, values,
                               label=pretty_label, color=color, alpha=alpha,
                               s=100, edgecolor="black", linewidth=1)

        ax.set_xlabel("Node ID")
        self.styler.set_axis_labels(ax, metric, "scatter", x_label="Node ID")
        self.styler.add_legend(ax, metric, "scatter")
        self.styler.style_axes(ax, metric, "scatter")

        self.save_plot(fig, f"{metric}_vs_node_scatter.pdf", output_folder)

    def plot_two_metrics_per_node(self, networks, labels, metric_x, metric_y, output_folder):
        """Create scatter plot comparing two metrics for each node."""
        fig, ax = self.create_figure(metric_x, "scatter")

        for net, label in zip(networks, labels):
            if 'nodes' in net:
                x_values = []
                y_values = []

                for node_id, node_info in net['nodes'].items():
                    if (self._has_metric_data(node_info, metric_x) and
                            self._has_metric_data(node_info, metric_y)):
                        x_values.append(
                            self._extract_node_metric(node_info, metric_x))
                        y_values.append(
                            self._extract_node_metric(node_info, metric_y))

                if x_values:
                    color = self.config.get_label_color(label)
                    alpha = self.config.get_label_alpha(label)
                    pretty_label = self.config.get_label_name(label)

                    ax.scatter(x_values, y_values,
                               label=pretty_label, color=color, alpha=alpha,
                               s=100, edgecolor="black", linewidth=1)

        ax.set_xlabel(self.config.METRIC_INFO[metric_x]["label_with_units"])
        ax.set_ylabel(self.config.METRIC_INFO[metric_y]["label_with_units"])
        self.styler.add_legend(ax, metric_x, "scatter")
        self.styler.style_axes(ax, metric_x, "scatter")

        self.save_plot(
            fig, f"{metric_x}_vs_{metric_y}_nodes_scatter.pdf", output_folder)

    def _extract_node_metric(self, node_info, metric):
        """Extract metric value from node data structure."""
        if metric == "power":
            return node_info.get("power", {}).get("average_mW", 0)
        elif metric == "latency":
            return node_info.get("delay", {}).get("avg", 0)
        elif metric == "jitter":
            return node_info.get("jitter", {}).get("avg", 0)
        elif metric == "packet_delivery_ratio":
            return node_info.get("packet_delivery_ratio", 0)
        return node_info.get(metric, 0)

    def _has_metric_data(self, node_info, metric):
        """Check if node has data for the given metric."""
        if metric == "power":
            return "power" in node_info
        elif metric == "latency":
            return "delay" in node_info
        elif metric == "jitter":
            return "jitter" in node_info
        elif metric == "packet_delivery_ratio":
            return "packet_delivery_ratio" in node_info
        return metric in node_info


class NodeLinePlotter(MetricPlotter):
    """Creates line plots showing node metrics over time."""

    def plot(self, networks, labels, metric, output_folder, node_ids=None):
        """Create line plot showing metric evolution for selected nodes."""
        fig, ax = self.create_figure(metric, "line")

        for net, label in zip(networks, labels):
            if 'nodes' in net:
                color = self.config.get_label_color(label)
                pretty_label = self.config.get_label_name(label)

                for node_id, node_info in net['nodes'].items():
                    if node_ids is None or int(node_id) in node_ids:
                        if self._has_time_series_data(node_info, metric):
                            time_data, values = self._extract_time_series(
                                node_info, metric)

                            ax.plot(time_data, values,
                                    color=color, alpha=0.7, linewidth=1,
                                    label=f"{pretty_label} - Node {node_id}")

        ax.set_xlabel("Time/Sample")
        self.styler.set_axis_labels(ax, metric, "line", x_label="Time/Sample")
        self.styler.add_legend(ax, metric, "line")
        self.styler.style_axes(ax, metric, "line")

        self.save_plot(fig, f"{metric}_per_node_timeline.pdf", output_folder)

    def _has_time_series_data(self, node_info, metric):
        """Check if node has time series data for the metric."""
        if metric == "power":
            return "power" in node_info and "samples_mW" in node_info["power"]
        return False

    def _extract_time_series(self, node_info, metric):
        """Extract time series data from node info."""
        if metric == "power":
            samples = node_info["power"]["samples_mW"]
            seqs = sorted(map(int, samples.keys()))
            values = [samples[str(seq)]["avg"] for seq in seqs]
            return seqs, values
        return [], []


class NodeBoxPlotter(MetricPlotter):
    """Creates box plots showing metric distributions across nodes."""

    def plot(self, networks, labels, metric, output_folder, node_ids=None):
        """Create box plot showing metric distribution for each protocol."""
        data = []
        protocol_names = []

        for net, label in zip(networks, labels):
            if 'nodes' in net:
                values = []
                for node_id, node_info in net['nodes'].items():
                    if node_ids is None or int(node_id) in node_ids:
                        if self._has_metric_data(node_info, metric):
                            values.append(
                                self._extract_node_metric(node_info, metric))

                data.extend(values)
                protocol_names.extend(
                    [self.config.get_label_name(label)] * len(values))

        if not data:
            print(f"No node data found for metric {metric}")
            return

        # Create DataFrame
        df = pd.DataFrame({"Protocol": protocol_names, metric: data})

        # Create box plot
        fig, ax = self.create_figure(metric, "box")

        sns.boxplot(x="Protocol", y=metric, data=df,
                    palette=self.config.get_palette(labels), ax=ax)

        ax.set_xlabel("")
        ax.set_xticklabels(ax.get_xticklabels(), rotation=45, ha='right')

        self.styler.set_axis_labels(ax, metric, "box")
        self.styler.style_axes(ax, metric, "box")

        self.save_plot(fig, f"{metric}_per_node_box.pdf", output_folder)

    def _extract_node_metric(self, node_info, metric):
        """Extract metric value from node data structure."""
        if metric == "power":
            return node_info.get("power", {}).get("avg_mW", 0)
        elif metric == "latency":
            return node_info.get("latency", {}).get("avg_ms", 0)
        elif metric == "jitter":
            return node_info.get("jitter", {}).get("avg", 0)
        elif metric == "packet_delivery_ratio":
            return node_info.get("packet_delivery_ratio", {}).get("avg", 0)
        return node_info.get(metric, 0)

    def _has_metric_data(self, node_info, metric):
        """Check if node has data for the given metric."""
        if metric == "power":
            return "power" in node_info
        elif metric == "latency":
            return "latency" in node_info
        elif metric == "jitter":
            return "jitter" in node_info
        elif metric == "packet_delivery_ratio":
            return "packet_delivery_ratio" in node_info
        return metric in node_info
