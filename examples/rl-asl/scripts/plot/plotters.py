"""
Specific plotter implementations for different types of plots.
"""

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from scipy.stats import norm
from matplotlib.patches import Patch
from matplotlib.ticker import MaxNLocator, FormatStrFormatter
from matplotlib.lines import Line2D


from metric_plotter import MetricPlotter


class BarPlotter(MetricPlotter):
    """Creates bar plots with error bars."""

    def plot(self, networks, labels, metric, output_folder):
        """Create bar plot for a metric."""
        # Sort protocols by metric value
        sorted_labels, sorted_networks = self.sort_protocols_by_metric(
            labels, networks, metric)

        # Extract values
        avg_vals, std_vals = [], []
        for net in sorted_networks:
            avg, std = self.get_metric_values(net, metric)
            avg_vals.append(avg)
            std_vals.append(std)

        # Get styling
        styles = self.styler.get_plot_colors_and_styles(sorted_labels)
        n = len(avg_vals)
        positions = np.arange(n)

        # Create plot
        fig, ax = self.create_figure(metric, "bar")

        bars = ax.bar(
            positions, avg_vals, yerr=std_vals, capsize=4,
            color=styles['colors'], edgecolor="black", linewidth=2
        )

        # Apply individual styling
        for bar, hatch, alpha in zip(bars, styles['hatches'], styles['alphas']):
            if hatch is not None:
                bar.set_hatch(hatch)
            bar.set_alpha(alpha)

        # Style the plot
        ax.set_xlim(-0.5, n - 0.5)
        ax.set_xticks(positions)
        ax.set_xticklabels(
            styles['pretty_labels'],   # or sorted_labels
            ha="center",
            rotation=30,
            fontsize=self.config.get_fontsize(
                metric, "xtick", self.config.X_TICK_FONT_SIZE, plot_type="bar")
        )

        self.styler.set_axis_labels(
            ax, metric, "bar", x_label=None, y_label=None)
        self.styler.style_axes(ax, metric, "bar")
        self.styler.apply_axis_limits(ax, metric)
        ax.xaxis.grid(False)

        if not self.config.use_legend(metric, plot_type="bar"):
            self.save_plot(fig, f"{metric}_bar.pdf", output_folder)
            plt.close(fig)
            return

        # Define preferred legend order (match your dataset naming)
        preferred_order = ["RL-ASL", "RL-ASL-LB",
                           "Orch.", "Orch.-LB", "PRIL-M",
                           r"$R_{skip}=0.25$", r"$R_{skip}=0.50$", r"$R_{skip}=0.75$"]

        # Use the bar patches as legend handles
        handles = list(bars)
        labels = styles['pretty_labels']

        # Build a map for label->handle
        handle_map = {label: handle for handle, label in zip(handles, labels)}

        # Reorder according to preferred_order, skipping missing labels
        reordered_labels = [l for l in preferred_order if l in labels]
        print(f"Reordered labels for legend: {reordered_labels}")

        # Add any remaining labels (unexpected protocols)
        for l in labels:
            if l not in reordered_labels:
                reordered_labels.append(l)

        # Rebuild ordered handle/label lists
        ordered_handles = [handle_map[l] for l in reordered_labels]

        # Create the legend
        legend = ax.legend(
            handles=ordered_handles,
            labels=reordered_labels,
            loc="best",
            frameon=True,
            fontsize=self.config.get_fontsize(metric, "legend",
                                              self.config.LEGEND_FONT_SIZE, plot_type="bar")
        )
        legend.get_frame().set_linewidth(1.0)
        legend.get_frame().set_edgecolor("black")

        # Save plot WITH legend
        self.save_plot(fig, f"{metric}_bar.pdf", output_folder)
        plt.close(fig)


class LinePlotter(MetricPlotter):
    """Creates line plots for per-sample data."""

    def plot(self, networks, labels, metric, output_folder):
        """Create line plot for per-sample data."""
        fig, ax = self.create_figure(metric, "line")

        # Increase this value to make the plotted lines thicker
        line_width = 2
        marker_size = 3

        for net, label in zip(networks, labels):
            per_sample, scale = self.get_per_sample_data(net, metric)

            seqs = sorted(map(int, per_sample.keys()))
            values = [per_sample[str(s)]["avg"] * scale for s in seqs]
            stds = [per_sample[str(s)]["std"] * scale for s in seqs]

            color = self.config.get_label_color(label)
            alpha = self.config.get_label_alpha(label)
            pretty_label = self.config.get_label_name(label)

            ax.plot(seqs, values, marker="o", label=pretty_label,
                    color=color, alpha=alpha, linewidth=line_width, markersize=marker_size)

            # Add error band
            ax.fill_between(seqs,
                            np.array(values) - np.array(stds),
                            np.array(values) + np.array(stds),
                            alpha=0.2, color=color)

        self.styler.set_axis_labels(ax, metric, "line", x_label="Sample")
        self.styler.add_legend(ax, metric, "line")
        self.styler.style_axes(ax, metric, "line")
        self.styler.apply_axis_limits(ax, metric)

        self.save_plot(fig, f"{metric}_per_sample.pdf", output_folder)


class ScatterPlotter(MetricPlotter):
    """Creates scatter plots comparing two metrics."""

    def plot_rdc_vs_latency(self, networks, labels, output_folder):
        """Create scatter plot of RDC vs Latency."""
        metric_x, metric_y = "power", "latency"

        fig, ax = self.create_figure(metric_x, "scatter")

        for net, label in zip(networks, labels):
            rdc, _ = self.get_metric_values(net, metric_x)
            latency, _ = self.get_metric_values(net, metric_y)

            ax.scatter(rdc, latency,
                       label=self.config.get_label_name(label),
                       color=self.config.get_label_color(label),
                       alpha=self.config.get_label_alpha(label),
                       linewidths=2, edgecolor="black", s=200)

        # Set labels from metric info
        ax.set_xlabel(self.config.METRIC_INFO[metric_x]["label_with_units"],
                      fontsize=self.config.get_fontsize(metric_x, "xlabel",
                                                        self.config.X_LABEL_FONT_SIZE, plot_type="scatter"))
        ax.set_ylabel(self.config.METRIC_INFO[metric_y]["label_with_units"],
                      fontsize=self.config.get_fontsize(metric_y, "ylabel",
                                                        self.config.Y_LABEL_FONT_SIZE, plot_type="scatter"))

        # Use logarithmic scale for the y-axis
        ax.set_yscale("log")
        # set the y-limit to 20
        ax.set_ylim(0, 20)

        self.styler.add_legend(ax, metric_x, "scatter")
        self.styler.style_axes(ax, metric_x, "scatter")

        self.save_plot(fig, "power_vs_latency.pdf", output_folder)

    def plot_power_latency_pdr(self, networks, labels, output_folder):
        """Scatter plot: Power vs Latency, PDR encoded as colormap."""

        metric_x = "power"
        metric_y = "latency"
        metric_c = "packet_delivery_ratio"

        fig, ax = self.create_figure(metric_x, "scatter")

        # Collect PDR values first (for consistent color normalization)
        pdr_vals = []
        for net in networks:
            pdr, _ = self.get_metric_values(net, metric_c)
            pdr_vals.append(pdr)

        # Normalize PDR for colormap
        norm = plt.Normalize(vmin=80, vmax=100)
        cmap = plt.cm.viridis  # or viridis / plasma

        # Scatter points
        for net, label in zip(networks, labels):
            power, _ = self.get_metric_values(net, metric_x)
            latency, _ = self.get_metric_values(net, metric_y)
            pdr, _ = self.get_metric_values(net, metric_c)

            ax.scatter(
                power,
                latency,
                s=300,
                facecolor=cmap(norm(pdr)),
                edgecolor=self.config.get_label_color(label),
                linewidths=4,
                alpha=1.0,
                label=self.config.get_label_name(label)
            )

        # Axis labels
        ax.set_xlabel(
            self.config.METRIC_INFO[metric_x]["label_with_units"],
            fontsize=self.config.get_fontsize(metric_x, "xlabel",
                                              self.config.X_LABEL_FONT_SIZE,
                                              plot_type="scatter")
        )
        ax.set_ylabel(
            self.config.METRIC_INFO[metric_y]["label_with_units"],
            fontsize=self.config.get_fontsize(metric_y, "ylabel",
                                              self.config.Y_LABEL_FONT_SIZE,
                                              plot_type="scatter")
        )

        # Latency is usually log-scale
        ax.set_yscale("log")
        ax.set_ylim(0, 20)

        # Styling
        self.styler.style_axes(ax, metric_x, "scatter")

        # --- PDR Colorbar ---
        sm = plt.cm.ScalarMappable(cmap=cmap, norm=norm)
        sm.set_array([])
        cbar = fig.colorbar(sm, ax=ax, pad=0.02)
        # cbar.ax.yaxis.set_major_locator(
        #     MaxNLocator(integer=True)
        # )
        cbar.ax.yaxis.set_major_formatter(
            FormatStrFormatter('%d')
        )
        cbar.set_label(
            self.config.METRIC_INFO[metric_c]["label_with_units"],
            fontsize=self.config.get_fontsize(metric_c, "color_bar",
                                              self.config.Y_LABEL_FONT_SIZE, plot_type="scatter")
        )
        # cbar ticks fontsize
        cbar.ax.tick_params(labelsize=self.config.get_fontsize(metric_c, "color_bar",
                                                               self.config.Y_LABEL_FONT_SIZE, plot_type="scatter"))

        # only put the legend if "scenario-1" or "scenario-4" is in the output folder
        if "scenario-1" in output_folder or "scenario-4" in output_folder:
            preferred_order = [
                "RL-ASL", "RL-ASL-LB",
                "Orch.", "Orch.-LB", "PRIL-M",
                r"$R_{skip}=0.25$", r"$R_{skip}=0.50$", r"$R_{skip}=0.75$"
            ]

            legend_handles = []

            for label in labels:
                legend_handles.append(
                    Line2D(
                        [0], [0],
                        marker='o',
                        linestyle='None',
                        markersize=12,
                        markerfacecolor='none',
                        markeredgecolor=self.config.get_label_color(label),
                        markeredgewidth=3,
                        label=self.config.get_label_name(label)
                    )
                )

            # Build label -> handle map
            handle_map = {h.get_label(): h for h in legend_handles}

            # Reorder according to preferred_order
            ordered_labels = [l for l in preferred_order if l in handle_map]

            # Append any remaining (unexpected) labels
            for l in handle_map:
                if l not in ordered_labels:
                    ordered_labels.append(l)

            ordered_handles = [handle_map[l] for l in ordered_labels]

            legend = ax.legend(
                handles=ordered_handles,
                labels=ordered_labels,
                loc="upper right",
                # ncol=2,
                frameon=True,
                fontsize=self.config.get_fontsize(
                    metric_x, "legend",
                    self.config.LEGEND_FONT_SIZE,
                    plot_type="scatter"
                ),
                columnspacing=1.2,           # horizontal space between columns
                handletextpad=0.2,           # space between marker and text
                labelspacing=0.2             # vertical spacing
            )

            legend.get_frame().set_linewidth(1.0)
            legend.get_frame().set_edgecolor("black")

        self.save_plot(fig, "power_vs_latency_pdr.pdf", output_folder)

    def plot(self, networks, labels, metric, output_folder):
        """Default scatter plot implementation."""
        # This could be extended for other scatter plot types
        if metric == "rdc":  # Special case for RDC vs Latency
            self.plot_rdc_vs_latency(networks, labels, output_folder)
        if metric == "power_latency_pdr":
            self.plot_power_latency_pdr(networks, labels, output_folder)


class ViolinPlotter(MetricPlotter):
    """Creates violin and box plots for data distributions."""

    def plot(self, networks, labels, metric, output_folder, kind="violin", use_log=True):
        """Create violin or box plot. Set use_log=True to plot y-axis on a logarithmic scale."""
        # Collect per-sample data
        data = []
        protocol_names = []
        avg_vals = []

        for net, label in zip(networks, labels):
            per_sample, scale = self.get_per_sample_data(net, metric)
            values = [v["avg"] * scale for v in per_sample.values()]

            data.extend(values)
            protocol_names.extend(
                [self.config.get_label_name(label)] * len(values))
            avg_vals.append(np.mean(values) if len(values) > 0 else np.nan)

        # Sort protocols
        sorted_labels, _ = self.sort_protocols_by_metric(
            labels, networks, metric)
        order = [self.config.get_label_name(lbl) for lbl in sorted_labels]

        # Create DataFrame
        df = pd.DataFrame({"Protocol": protocol_names, metric: data})

        # If log scale requested, ensure all values are positive by shifting if necessary
        if use_log and not df.empty:
            min_val = df[metric].min(skipna=True)
            if min_val is None or np.isnan(min_val):
                pass
            else:
                if min_val <= 0:
                    # shift all values to be strictly positive
                    shift = abs(min_val) + 1e-9
                    df[metric] = df[metric] + shift

        # Create plot
        fig, ax = self.create_figure(metric, kind)

        if kind == "box":
            sns.boxplot(x="Protocol", y=metric, hue="Protocol", data=df,
                        dodge=False, legend=False, order=order,
                        palette=self.config.get_palette(labels), ax=ax)
        else:
            sns.violinplot(x="Protocol", y=metric, hue="Protocol", data=df,
                           dodge=False, legend=False, inner="quartile", order=order,
                           palette=self.config.get_palette(labels), ax=ax)

        # Apply logarithmic scale if requested
        if use_log:
            ax.set_yscale("log")

        # Get styling
        styles = self.styler.get_plot_colors_and_styles(sorted_labels)
        n = len(avg_vals)
        positions = np.arange(n)

        ax.set_xlabel("")
        ax.set_xticks(positions)
        ax.set_xticklabels(
            styles['pretty_labels'],   # or sorted_labels
            ha="center",
            rotation=30,
            fontsize=self.config.get_fontsize(
                metric, "xtick", self.config.X_TICK_FONT_SIZE, plot_type=kind)
        )

        self.styler.set_axis_labels(ax, metric, kind)
        self.styler.style_axes(ax, metric, kind)
        self.styler.apply_axis_limits(ax, metric)

        self.save_plot(
            fig, f"{metric}_{kind}{'_log' if use_log else ''}.pdf", output_folder)


class CDFPlotter(MetricPlotter):
    """Creates cumulative distribution function plots."""

    def plot(self, networks, labels, metric, output_folder):
        """Create CDF plot for a metric."""
        fig, ax = self.create_figure(metric, "cdf")

        for net, label in zip(networks, labels):
            per_sample, scale = self.get_per_sample_data(net, metric)
            values = np.array([v["avg"] * scale for v in per_sample.values()])
            values = np.sort(values)

            # Compute CDF
            yvals = np.arange(1, len(values) + 1) / float(len(values))

            ax.step(values, yvals,
                    label=self.config.get_label_name(label),
                    color=self.config.get_label_color(label),
                    alpha=self.config.get_label_alpha(label),
                    linewidth=2)

        self.styler.set_axis_labels(ax, metric, "cdf", y_label="CDF")
        self.styler.add_legend(ax, metric, "cdf")
        self.styler.style_axes(ax, metric, "line")

        self.save_plot(fig, f"{metric}_cdf.pdf", output_folder)


class HistogramPlotter(MetricPlotter):
    """Creates histogram plots with normal distribution fits."""

    def plot(self, networks, labels, metric, output_folder):
        """Create histogram with fitted normal distribution."""
        fig, ax = self.create_figure(metric, "histogram")

        for net, label in zip(networks, labels):
            per_sample, scale = self.get_per_sample_data(net, metric)
            values = np.array([v["avg"] * scale for v in per_sample.values()])

            color = self.config.get_label_color(label)
            alpha = self.config.get_label_alpha(label)
            pretty_label = self.config.get_label_name(label)

            # Histogram
            sns.histplot(values, bins=30, kde=False, stat="density",
                         color=color, alpha=alpha, ax=ax, label=pretty_label)

            # Fit normal distribution
            if len(values) > 1:
                mu, std = norm.fit(values)
                xmin, xmax = ax.get_xlim()
                x = np.linspace(xmin, xmax, 200)
                p = norm.pdf(x, mu, std)
                ax.plot(x, p, color=color, linewidth=2, alpha=alpha)

        self.styler.set_axis_labels(ax, metric, "histogram", y_label="Density")
        self.styler.add_legend(ax, metric, "histogram")
        self.styler.style_axes(ax, metric, "histogram")

        self.save_plot(fig, f"{metric}_hist_normal.pdf", output_folder)


class RadarPlotter(MetricPlotter):
    """Creates radar plots comparing multiple metrics."""

    def plot(self, networks, labels, output_folder):
        """Create radar plot comparing multiple metrics."""
        metrics = ["latency", "power", "packet_delivery_ratio"]

        # Collect data
        data = {}
        for net, label in zip(networks, labels):
            values = []
            for metric in metrics:
                avg, _ = self.get_metric_values(net, metric)
                values.append(avg)
            data[self.config.get_label_name(label)] = values

        # print the data
        for label, values in data.items():
            print(f"{label}: {values}")

            # Normalize metrics
        values_matrix = np.array(list(data.values()))
        norm_matrix = []
        for i, metric in enumerate(metrics):
            col = values_matrix[:, i]
            min_val, max_val = np.min(col), np.max(col)
            diff = max_val - min_val

            if diff < 1e-12:
                # All protocols have the same value → all should get perfect score 1.0
                norm = np.ones_like(col)
            else:
                if metric == "packet_delivery_ratio" or self.config.METRIC_INFO[metric]["sort"] == "desc":
                    # Higher is better
                    norm = (col - min_val) / (diff + 1e-9)
                elif self.config.METRIC_INFO[metric]["sort"] == "asc":
                    # Lower is better
                    norm = 1 - (col - min_val) / (diff + 1e-9)
                else:
                    # Default: assume higher is better
                    norm = (col - min_val) / (diff + 1e-9)

            norm_matrix.append(norm)

        norm_matrix = np.array(norm_matrix).T

        # print the normalized matrix
        for i, (protocol, values) in enumerate(zip(data.keys(), norm_matrix)):
            print(f"{protocol} (norm): {values.tolist()}")

        # Setup radar plot
        num_vars = len(metrics)
        angles = np.linspace(0, 2 * np.pi, num_vars, endpoint=False).tolist()
        angles += angles[:1]

        fig, ax = plt.subplots(figsize=self.config.get_figsize("radar", plot_type="radar"),
                               subplot_kw=dict(polar=True))

        for (protocol, values), label in zip(zip(data.keys(), norm_matrix), labels):
            vals = values.tolist()
            vals += vals[:1]
            ax.plot(angles, vals,
                    color=self.config.get_label_color(label),
                    alpha=self.config.get_label_alpha(label),
                    linewidth=2, linestyle="--", label=protocol)  # dashed outline
            ax.fill(angles, vals, color=self.config.get_label_color(
                label), alpha=0.25)

        # Make the outer spine dashed as well
        try:
            ax.spines['polar'].set_linestyle('--')
        except Exception:
            # some matplotlib backends may not expose 'polar' spine in the same way; ignore if unavailable
            pass

        # Set labels
        ax.set_xticks(angles[:-1])
        ax.set_xticklabels([self.config.METRIC_INFO[m]["label_no_units"] for m in metrics],
                           fontsize=self.config.get_fontsize("radar", "xtick",
                                                             self.config.X_TICK_FONT_SIZE, plot_type="radar"))

        ax.set_yticks([0.25, 0.5, 0.75, 1.0])
        ax.set_yticklabels(["0.25", "0.5", "0.75", "1.0"],
                           fontsize=self.config.get_fontsize("radar", "ytick",
                                                             self.config.Y_TICK_FONT_SIZE, plot_type="radar"))
        ax.set_ylim(0, 1)

        if self.config.use_legend("radar", plot_type="radar"):
            # Dont create a legend if the output folder does not contain 'scenario-1'
            if "scenario-1" in output_folder or "scenario-4" in output_folder:
                # Define preferred legend order (same idea as bar plot)
                preferred_order = [
                    "RL-ASL", "RL-ASL-LB",
                    "Orch.", "Orch.-LB", "PRIL-M",
                    r"$R_{skip}=0.25$", r"$R_{skip}=0.50$", r"$R_{skip}=0.75$"
                ]

                # Get current legend handles and labels from the axes
                handles, legend_labels = ax.get_legend_handles_labels()

                # Build label -> handle map
                handle_map = {lbl: h for h, lbl in zip(handles, legend_labels)}

                # Reorder according to preferred_order
                reordered_labels = [
                    l for l in preferred_order if l in legend_labels]

                # Append any remaining labels (unexpected protocols)
                for l in legend_labels:
                    if l not in reordered_labels:
                        reordered_labels.append(l)

                ordered_handles = [handle_map[l] for l in reordered_labels]

                legend = ax.legend(
                    handles=ordered_handles,
                    labels=reordered_labels,
                    loc="center left",
                    bbox_to_anchor=(1.1, 0.5),
                    frameon=True,
                    fontsize=self.config.get_fontsize(
                        "radar", "legend",
                        self.config.LEGEND_FONT_SIZE,
                        plot_type="radar"
                    )
                )

                legend.get_frame().set_linewidth(1.0)
                legend.get_frame().set_edgecolor("black")

        self.save_plot(fig, "radar_metrics.pdf", output_folder)


class HeatmapPlotter(MetricPlotter):
    """Creates heatmap plots for protocol-metric comparison."""

    def plot(self, networks, labels, output_folder):
        """Create heatmap of protocols vs metrics."""
        metrics = [m for m in self.config.METRIC_INFO.keys()
                   if m not in ["heatmap", "radar", "power", "energy", "jitter"]]

        # Collect data
        data = {}
        for net, label in zip(networks, labels):
            values = []
            for metric in metrics:
                avg, _ = self.get_metric_values(net, metric)
                values.append(avg)
            data[self.config.get_label_name(label)] = values

        df = pd.DataFrame(data, index=metrics).T

        # Normalize each column
        norm_df = pd.DataFrame(index=df.index, columns=df.columns)
        for metric in metrics:
            col = df[metric].values.astype(float)
            col_min, col_max = np.min(col), np.max(col)

            if col_max == col_min:
                norm = np.ones_like(col) * 0.5
            else:
                norm = (col - col_min) / (col_max - col_min)

            if self.config.METRIC_INFO[metric].get("sort", "asc") == "asc":
                norm = 1 - norm

            norm_df[metric] = norm

        # Create heatmap
        fig, ax = self.create_figure("heatmap", "heatmap")
        heatmap = sns.heatmap(norm_df.astype(float),
                              annot=False, fmt=".2f", cmap="YlGnBu",
                              cbar_kws={
                                  "label": "Norm. Score [0=worst, 1=best]"},
                              linewidths=0.5, linecolor="gray", ax=ax)

        # Style colorbar
        heatmap.figure.axes[-1].yaxis.label.set_size(
            self.config.get_fontsize("heatmap", "color_bar",
                                     self.config.Y_LABEL_FONT_SIZE, plot_type="heatmap"))
        heatmap.figure.axes[-1].tick_params(
            labelsize=self.config.get_fontsize("heatmap", "ytick",
                                               self.config.Y_TICK_HEATMAP_FONT_SIZE, plot_type="heatmap"))

        ax.set_xticklabels(
            labels=[self.config.METRIC_INFO[m]["label_no_units"]
                    for m in metrics],
            rotation=30, ha="right",
            fontsize=self.config.get_fontsize("heatmap", "xtick",
                                              self.config.X_TICK_HEATMAP_FONT_SIZE, plot_type="heatmap")
        )
        plt.yticks(
            fontsize=self.config.get_fontsize("heatmap", "ytick",
                                              self.config.Y_TICK_HEATMAP_FONT_SIZE, plot_type="heatmap"),
            rotation=0, ha="right")

        self.styler.set_axis_labels(ax, "heatmap", "heatmap",
                                    x_label="Metrics", y_label="Protocols")

        self.save_plot(fig, "protocol_metric_heatmap.pdf", output_folder)


class StackedBarPlotter(MetricPlotter):
    """Creates stacked bar plots for power consumption breakdown."""

    def plot(self, networks, labels, output_folder):
        """Create stacked bar chart for power consumption breakdown."""
        metric = "power"

        # Sort protocols by the chosen metric value
        sorted_labels, sorted_networks = self.sort_protocols_by_metric(
            labels, networks, metric)
        pretty_labels = [self.config.get_label_name(l) for l in sorted_labels]

        # Define components
        components = [
            "avg_cpu_mW", "avg_tx_mW", "avg_rx_non_uc_total_mW",
            "avg_rx_uc_mW", "avg_rx_uc_idle_mW"
        ]

        # Colors and hatches
        base_rx_color = "#9E9E9E"
        colors = {
            "avg_cpu_mW": "#7B6FD0",
            "avg_tx_mW": "#8C6D31",
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

        # Collect data (in sorted order)
        data = {comp: [] for comp in components}
        rx_total = []
        for net in sorted_networks:
            power = net["network"]["power"]
            for comp in components:
                data[comp].append(power.get(comp, 0.0))
            rx_total.append(power.get("avg_rx_mW", 0.0))

        n = len(sorted_labels)
        ind = np.arange(n)
        width = 0.6

        fig, ax = self.create_figure(metric, "stacked_bar")

        # Create stacked bars
        bottom = np.zeros(n)
        for comp in components:
            ax.bar(ind, data[comp], width, bottom=bottom,
                   color=colors[comp], edgecolor="black", linewidth=1.2,
                   hatch=hatches[comp])
            bottom += np.array(data[comp])

        # Add total value on top of each stacked bar
        for i, total in enumerate(bottom):
            ax.text(
                ind[i],                    # x position
                total,                     # y position (top of stack)
                f"{total:.1f}",            # format as needed
                ha="center",
                va="bottom",
                fontsize=self.config.get_fontsize(
                    metric, "above_bar",
                    self.config.X_TICK_FONT_SIZE,
                    plot_type="stacked_bar"
                ),
                fontweight="bold"
            )

        # Add RX total outline
        # cpu_tx = np.array(data["avg_cpu_mW"]) + np.array(data["avg_tx_mW"])
        # ax.bar(ind, rx_total, width, bottom=cpu_tx,
        #        color="none", edgecolor="red", linewidth=2.8,
        #        linestyle="--", label="RX (group)")

        # Styling
        self.styler.set_axis_labels(
            ax, metric, "stacked_bar", x_label="Protocols")
        ax.set_xticks(ind)
        ax.set_xticklabels(
            pretty_labels,
            ha="center",
            rotation=30,
            fontsize=self.config.get_fontsize(
                metric, "xtick", self.config.X_TICK_FONT_SIZE, plot_type="stacked_bar")
        )
        self.styler.style_axes(ax, metric, "stacked_bar")
        # ax.tick_params(axis="x", labelsize=13)
        # ax.tick_params(axis="y", labelsize=14)
        ax.grid(axis="y", linestyle="--", alpha=0.7)

        # Dont create a legen if the output folder idoes not contain 'scenario-1'
        if "scenario-1" not in output_folder:
            self.save_plot(fig, f"{metric}_stacked_bar.pdf", output_folder)
            plt.close(fig)
            return

        # Custom legend elements (do NOT add to the main axes)
        legend_elements = [
            Patch(facecolor="#7B6FD0", edgecolor="black", label="CPU"),
            Patch(facecolor="#8C6D31", edgecolor="black",
                  hatch="//", label="TX"),
            Patch(facecolor=base_rx_color, edgecolor="black",
                  hatch="\\\\\\\\", label="RX non-UC"),
            Patch(facecolor=base_rx_color, edgecolor="black",
                  hatch="xx", label="RX UC active"),
            Patch(facecolor=base_rx_color, edgecolor="black",
                  hatch="oo", label="RX UC idle"),
            # Patch(facecolor="none", edgecolor="red",
            #       linestyle="--", label="RX total (boundary)")
        ]

        # Add legend to the SAME axes
        legend = ax.legend(
            handles=legend_elements,
            loc="upper center",
            bbox_to_anchor=(0.5, 1.15),
            ncol=3,                     # ← controls horizontal filling
            frameon=True,
            fontsize=self.config.get_fontsize(
                "power", "legend",
                self.config.LEGEND_FONT_SIZE,
                plot_type="stacked_bar"
            ),
            columnspacing=0.5,          # space between columns
            handlelength=1.0,           # width of legend symbols
            handletextpad=0.5           # space between symbol and text
        )

        legend.get_frame().set_linewidth(1.0)
        legend.get_frame().set_edgecolor("black")

        # Save plot WITH legend
        self.save_plot(fig, f"{metric}_stacked_bar.pdf", output_folder)
        plt.close(fig)
