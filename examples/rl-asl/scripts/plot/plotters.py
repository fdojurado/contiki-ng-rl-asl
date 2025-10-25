"""
Specific plotter implementations for different types of plots.
"""

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from scipy.stats import norm
from matplotlib.patches import Patch

from metric_plotter import MetricPlotter


class BarPlotter(MetricPlotter):
    """Creates bar plots with error bars."""
    
    def plot(self, networks, labels, metric, output_folder):
        """Create bar plot for a metric."""
        # Sort protocols by metric value
        sorted_labels, sorted_networks = self.sort_protocols_by_metric(labels, networks, metric)
        
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
        ax.set_xticks([])
        ax.set_xticklabels([])
        
        self.styler.set_axis_labels(ax, metric, "bar")
        self.styler.add_legend(ax, metric, "bar", handles=bars, labels=styles['pretty_labels'])
        self.styler.style_axes(ax, metric, "bar")
        self.styler.apply_axis_limits(ax, metric)
        ax.xaxis.grid(False)
        
        self.save_plot(fig, f"{metric}_bar.pdf", output_folder)


class LinePlotter(MetricPlotter):
    """Creates line plots for per-sample data."""
    
    def plot(self, networks, labels, metric, output_folder):
        """Create line plot for per-sample data."""
        fig, ax = self.create_figure(metric, "line")
        
        for net, label in zip(networks, labels):
            per_sample, scale = self.get_per_sample_data(net, metric)
            
            seqs = sorted(map(int, per_sample.keys()))
            values = [per_sample[str(s)]["avg"] * scale for s in seqs]
            stds = [per_sample[str(s)]["std"] * scale for s in seqs]
            
            color = self.config.get_label_color(label)
            alpha = self.config.get_label_alpha(label)
            pretty_label = self.config.get_label_name(label)
            
            ax.plot(seqs, values, marker="o", label=pretty_label, 
                   color=color, alpha=alpha, linewidth=1, markersize=2)
            
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
        
        self.styler.add_legend(ax, metric_x, "scatter")
        self.styler.style_axes(ax, metric_x, "scatter")
        
        self.save_plot(fig, "power_vs_latency.pdf", output_folder)
    
    def plot(self, networks, labels, metric, output_folder):
        """Default scatter plot implementation."""
        # This could be extended for other scatter plot types
        if metric == "rdc":  # Special case for RDC vs Latency
            self.plot_rdc_vs_latency(networks, labels, output_folder)


class ViolinPlotter(MetricPlotter):
    """Creates violin and box plots for data distributions."""
    
    def plot(self, networks, labels, metric, output_folder, kind="violin"):
        """Create violin or box plot."""
        # Collect per-sample data
        data = []
        protocol_names = []
        avg_vals = []
        
        for net, label in zip(networks, labels):
            per_sample, scale = self.get_per_sample_data(net, metric)
            values = [v["avg"] * scale for v in per_sample.values()]
            
            data.extend(values)
            protocol_names.extend([self.config.get_label_name(label)] * len(values))
            avg_vals.append(np.mean(values))
        
        # Sort protocols
        sorted_labels, _ = self.sort_protocols_by_metric(labels, networks, metric)
        order = [self.config.get_label_name(lbl) for lbl in sorted_labels]
        
        # Create DataFrame
        df = pd.DataFrame({"Protocol": protocol_names, metric: data})
        
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
        
        ax.set_xlabel("")
        ax.set_xticks([])
        ax.set_xticklabels([])
        
        self.styler.set_axis_labels(ax, metric, kind)
        self.styler.style_axes(ax, metric, kind)
        self.styler.apply_axis_limits(ax, metric)
        
        self.save_plot(fig, f"{metric}_{kind}.pdf", output_folder)


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
        
        # Normalize metrics
        values_matrix = np.array(list(data.values()))
        norm_matrix = []
        for i, metric in enumerate(metrics):
            col = values_matrix[:, i]
            if self.config.METRIC_INFO[metric]["sort"] == "asc":
                norm = 1 - (col - np.min(col)) / (np.max(col) - np.min(col) + 1e-9)
            else:
                norm = (col - np.min(col)) / (np.max(col) - np.min(col) + 1e-9)
            norm_matrix.append(norm)
        norm_matrix = np.array(norm_matrix).T
        
        # Setup radar plot
        num_vars = len(metrics)
        angles = np.linspace(0, 2 * np.pi, num_vars, endpoint=False).tolist()
        angles += angles[:1]
        
        fig, ax = plt.subplots(figsize=self.config.get_figsize("radar", plot_type="radar"),
                              subplot_kw=dict(polar=True))
        
        for (protocol, values), label in zip(zip(data.keys(), norm_matrix), labels):
            vals = values.tolist()
            vals += vals[:1]
            ax.plot(angles, vals, color=self.config.get_label_color(label), 
                   alpha=self.config.get_label_alpha(label),
                   linewidth=2, label=protocol)
            ax.fill(angles, vals, color=self.config.get_label_color(label), alpha=0.25)
        
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
            ax.legend(loc="center left", bbox_to_anchor=(1.1, 0.5),
                     fontsize=self.config.get_fontsize("radar", "legend", 
                                                      self.config.LEGEND_FONT_SIZE, plot_type="radar"))
        
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
                             cbar_kws={"label": "Norm. Score [0=worst, 1=best]"},
                             linewidths=0.5, linecolor="gray", ax=ax)
        
        # Style colorbar
        heatmap.figure.axes[-1].yaxis.label.set_size(
            self.config.get_fontsize("heatmap", "color_bar", 
                                   self.config.Y_LABEL_FONT_SIZE, plot_type="heatmap"))
        heatmap.figure.axes[-1].tick_params(
            labelsize=self.config.get_fontsize("heatmap", "ytick", 
                                             self.config.Y_TICK_HEATMAP_FONT_SIZE, plot_type="heatmap"))
        
        ax.set_xticklabels(
            labels=[self.config.METRIC_INFO[m]["label_no_units"] for m in metrics],
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
        sorted_labels, sorted_networks = self.sort_protocols_by_metric(labels, networks, metric)
        pretty_labels = [self.config.get_label_name(l) for l in sorted_labels]
        
        # Define components
        components = [
            "avg_cpu_mW", "avg_tx_mW", "avg_rx_non_uc_total_mW",
            "avg_rx_uc_mW", "avg_rx_uc_idle_mW"
        ]
        
        # Colors and hatches
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
        
        # Add RX total outline
        cpu_tx = np.array(data["avg_cpu_mW"]) + np.array(data["avg_tx_mW"])
        ax.bar(ind, rx_total, width, bottom=cpu_tx,
              color="none", edgecolor="red", linewidth=2.8,
              linestyle="--", label="RX (group)")
        
        # Styling
        self.styler.set_axis_labels(ax, metric, "stacked_bar", x_label="Protocols")
        ax.set_xticks(ind)
        ax.set_xticklabels(pretty_labels, rotation=0, ha="center", fontsize=10)
        self.styler.style_axes(ax, metric, "stacked_bar")
        ax.grid(axis="y", linestyle="--", alpha=0.7)
        
        # Custom legend
        legend_elements = [
            Patch(facecolor="#4C72B0", edgecolor="black", label="CPU"),
            Patch(facecolor="#55A868", edgecolor="black", hatch="//", label="TX"),
            Patch(facecolor=base_rx_color, edgecolor="black", hatch="\\\\\\\\", label="RX non-UC"),
            Patch(facecolor=base_rx_color, edgecolor="black", hatch="xx", label="RX UC active"),
            Patch(facecolor=base_rx_color, edgecolor="black", hatch="oo", label="RX UC idle"),
            Patch(facecolor="none", edgecolor="red", linestyle="--", label="RX total (boundary)")
        ]
        
        legend = ax.legend(handles=legend_elements,
                   loc="upper left", frameon=True, fontsize=16)
        # Increase legend box outline width
        legend.get_frame().set_linewidth(1.0)
        legend.get_frame().set_edgecolor("black")
        
        self.save_plot(fig, f"{metric}_stacked_bar.pdf", output_folder)