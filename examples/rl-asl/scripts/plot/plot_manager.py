"""
Main plot manager that orchestrates all plotting operations.
"""

import os
from pathlib import Path

from plot_config import PlotConfig
from plot_styler import PlotStyler
from plotters import (
    BarPlotter, LinePlotter, ScatterPlotter, ViolinPlotter,
    CDFPlotter, HistogramPlotter, RadarPlotter, HeatmapPlotter, StackedBarPlotter
)


class PlotManager:
    """Main class that manages all plotting operations."""
    
    def __init__(self, config: PlotConfig = None):
        """Initialize with configuration."""
        self.config = config or PlotConfig
        self.styler = PlotStyler(self.config)
        
        # Initialize plotters
        self.bar_plotter = BarPlotter(self.config, self.styler)
        self.line_plotter = LinePlotter(self.config, self.styler)
        self.scatter_plotter = ScatterPlotter(self.config, self.styler)
        self.violin_plotter = ViolinPlotter(self.config, self.styler)
        self.cdf_plotter = CDFPlotter(self.config, self.styler)
        self.histogram_plotter = HistogramPlotter(self.config, self.styler)
        self.radar_plotter = RadarPlotter(self.config, self.styler)
        self.heatmap_plotter = HeatmapPlotter(self.config, self.styler)
        self.stacked_bar_plotter = StackedBarPlotter(self.config, self.styler)
        
        # Apply global styling
        self.styler.apply_seaborn_style()
    
    def load_data(self, files, labels=None):
        """Load data from JSON files."""
        networks = []
        default_labels = []
        
        for file_path in files:
            data, label = self.bar_plotter.load_json(file_path)
            networks.append(data)
            default_labels.append(label)
        
        # Use provided labels or default to extracted labels
        final_labels = labels if labels else default_labels
        
        return networks, final_labels
    
    def create_all_plots(self, networks, labels, output_folder):
        """Create all types of plots for the given data."""
        # Ensure output folder exists
        Path(output_folder).mkdir(parents=True, exist_ok=True)
        
        # Get metrics to plot (exclude special plot types)
        metrics_to_plot = [m for m in self.config.METRIC_INFO.keys() 
                          if m not in ["heatmap", "radar"]]
        
        print("Creating bar plots...")
        self._create_bar_plots(networks, labels, metrics_to_plot, output_folder)
        
        print("Creating line plots...")
        self._create_line_plots(networks, labels, metrics_to_plot, output_folder)
        
        print("Creating distribution plots...")
        self._create_distribution_plots(networks, labels, metrics_to_plot, output_folder)
        
        print("Creating specialized plots...")
        self._create_specialized_plots(networks, labels, output_folder)
        
        print(f"All plots saved to: {output_folder}")
    
    def _create_bar_plots(self, networks, labels, metrics, output_folder):
        """Create bar plots for all metrics."""
        for metric in metrics:
            self.bar_plotter.plot(networks, labels, metric, output_folder)
    
    def _create_line_plots(self, networks, labels, metrics, output_folder):
        """Create line plots for per-sample data."""
        for metric in metrics:
            self.line_plotter.plot(networks, labels, metric, output_folder)
    
    def _create_distribution_plots(self, networks, labels, metrics, output_folder):
        """Create distribution-related plots."""
        for metric in metrics:
            # Box and violin plots
            self.violin_plotter.plot(networks, labels, metric, output_folder, kind="box")
            self.violin_plotter.plot(networks, labels, metric, output_folder, kind="violin")
        
        # CDF plots for specific metrics
        for metric in ["latency", "jitter"]:
            if metric in metrics:
                self.cdf_plotter.plot(networks, labels, metric, output_folder)
        
        # Histogram plots for specific metrics
        for metric in ["latency", "jitter"]:
            if metric in metrics:
                self.histogram_plotter.plot(networks, labels, metric, output_folder)
    
    def _create_specialized_plots(self, networks, labels, output_folder):
        """Create specialized plots like radar, heatmap, etc."""
        # Scatter plot: RDC vs Latency
        self.scatter_plotter.plot_rdc_vs_latency(networks, labels, output_folder)
        
        # Radar plot
        self.radar_plotter.plot(networks, labels, output_folder)
        
        # Heatmap
        self.heatmap_plotter.plot(networks, labels, output_folder)
        
        # Stacked bar for power breakdown
        self.stacked_bar_plotter.plot(networks, labels, output_folder)
    
    def create_custom_plot(self, plot_type, networks, labels, output_folder, **kwargs):
        """Create a specific type of plot."""
        if plot_type == "bar":
            metric = kwargs.get("metric")
            if metric:
                self.bar_plotter.plot(networks, labels, metric, output_folder)
        
        elif plot_type == "line":
            metric = kwargs.get("metric")
            if metric:
                self.line_plotter.plot(networks, labels, metric, output_folder)
        
        elif plot_type == "violin":
            metric = kwargs.get("metric")
            kind = kwargs.get("kind", "violin")
            if metric:
                self.violin_plotter.plot(networks, labels, metric, output_folder, kind=kind)
        
        elif plot_type == "cdf":
            metric = kwargs.get("metric")
            if metric:
                self.cdf_plotter.plot(networks, labels, metric, output_folder)
        
        elif plot_type == "histogram":
            metric = kwargs.get("metric")
            if metric:
                self.histogram_plotter.plot(networks, labels, metric, output_folder)
        
        elif plot_type == "scatter_rdc_latency":
            self.scatter_plotter.plot_rdc_vs_latency(networks, labels, output_folder)
        
        elif plot_type == "radar":
            self.radar_plotter.plot(networks, labels, output_folder)
        
        elif plot_type == "heatmap":
            self.heatmap_plotter.plot(networks, labels, output_folder)
        
        elif plot_type == "stacked_bar_power":
            self.stacked_bar_plotter.plot(networks, labels, output_folder)
        
        else:
            print(f"Unknown plot type: {plot_type}")
    
    def get_available_metrics(self):
        """Get list of available metrics for plotting."""
        return [m for m in self.config.METRIC_INFO.keys() if m not in ["heatmap", "radar"]]
    
    def get_available_plot_types(self):
        """Get list of available plot types."""
        return [
            "bar", "line", "violin", "box", "cdf", "histogram", 
            "scatter_rdc_latency", "radar", "heatmap", "stacked_bar_power"
        ]