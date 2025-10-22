"""
Node plot manager that orchestrates all per-node plotting operations.
"""

import os
from pathlib import Path

from plot_config import PlotConfig
from plot_styler import PlotStyler
from node_plotters import (
    NodeBarPlotter, NodeHeatmapPlotter, NodeScatterPlotter,
    NodeLinePlotter, NodeBoxPlotter, NodeHistogramPlotter, NodeStackedBarPlotter
)


class NodePlotManager:
    """Main class that manages all per-node plotting operations."""
    
    def __init__(self, config: PlotConfig = None):
        """Initialize with configuration."""
        self.config = config or PlotConfig()
        self.styler = PlotStyler(self.config)
        
        # Initialize node-specific plotters
        self.bar_plotter = NodeBarPlotter(self.config, self.styler)
        self.histogram_plotter = NodeHistogramPlotter(self.config, self.styler)
        self.heatmap_plotter = NodeHeatmapPlotter(self.config, self.styler)
        self.scatter_plotter = NodeScatterPlotter(self.config, self.styler)
        self.line_plotter = NodeLinePlotter(self.config, self.styler)
        self.box_plotter = NodeBoxPlotter(self.config, self.styler)
        self.stacked_bar_plotter = NodeStackedBarPlotter(self.config, self.styler)
        
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
    
    def create_all_node_plots(self, networks, labels, output_folder, node_ids=None):
        """Create all types of per-node plots for the given data."""
        # Ensure output folder exists
        Path(output_folder).mkdir(parents=True, exist_ok=True)
        
        # Get available node metrics
        node_metrics = self.get_available_node_metrics(networks)
        
        if not node_metrics:
            print("No node data found in the provided networks.")
            return
        
        print("Creating per-node bar plots...")
        self._create_node_bar_plots(networks, labels, node_metrics, output_folder, node_ids)
        
        # print("Creating per-node heatmaps...")
        # self._create_node_heatmaps(networks, labels, node_metrics, output_folder, node_ids)
        
        # print("Creating per-node scatter plots...")
        # self._create_node_scatter_plots(networks, labels, node_metrics, output_folder)
        
        print("Creating distribution plots...")
        self._create_node_distribution_plots(networks, labels, node_metrics, output_folder, node_ids)

        print("Creating per-node box plots...")
        self._create_node_box_plots(networks, labels, node_metrics, output_folder, node_ids)
        
        print("Creating per-node stacked bar plots...")
        self._create_node_stacked_bar_plots(networks, labels, node_metrics, output_folder, node_ids)
        
        # print("Creating per-node timeline plots...")
        # self._create_node_timeline_plots(networks, labels, output_folder, node_ids)
        
        print(f"All per-node plots saved to: {output_folder}")
    
    def _create_node_bar_plots(self, networks, labels, metrics, output_folder, node_ids):
        """Create bar plots for per-node metrics."""
        for metric in metrics:
            self.bar_plotter.plot(networks, labels, metric, output_folder, node_ids)
    
    def _create_node_heatmaps(self, networks, labels, metrics, output_folder, node_ids):
        """Create heatmaps for per-node metrics."""
        for metric in metrics:
            self.heatmap_plotter.plot(networks, labels, metric, output_folder, node_ids)
    
    def _create_node_scatter_plots(self, networks, labels, metrics, output_folder):
        """Create scatter plots for per-node metrics."""
        # Scatter plot of each metric vs node ID
        for metric in metrics:
            self.scatter_plotter.plot_metric_vs_node(networks, labels, metric, output_folder)
        
        # Cross-metric scatter plots
        if "power" in metrics and "latency" in metrics:
            self.scatter_plotter.plot_two_metrics_per_node(
                networks, labels, "power", "latency", output_folder)
        
        if "latency" in metrics and "packet_delivery_ratio" in metrics:
            self.scatter_plotter.plot_two_metrics_per_node(
                networks, labels, "latency", "packet_delivery_ratio", output_folder)
            
    def _create_node_distribution_plots(self, networks, labels, metrics, output_folder, node_ids):
        """Create distribution-related plots for per-node metrics."""   
        # CDF plots for specific metrics
        # for metric in ["latency", "jitter"]:
        #     if metric in metrics:
        #         self.cdf_plotter.plot(networks, labels, metric, output_folder)
        
        # Histogram plots for specific metrics
        for metric in ["latency", "jitter"]:
            if metric in metrics:
                self.histogram_plotter.plot(networks, labels, metric, output_folder)
            
    def _create_node_box_plots(self, networks, labels, metrics, output_folder, node_ids):
        """Create box plots for per-node metrics."""
        for metric in metrics:
            self.box_plotter.plot(networks, labels, metric, output_folder, node_ids)
            
    def _create_node_stacked_bar_plots(self, networks, labels, metrics, output_folder, node_ids):
        """Create stacked bar plots for per-node metrics."""
        for metric in ["power"]:
            if metric in metrics:
                print(f"Creating stacked bar plot for metric: {metric}")
                self.stacked_bar_plotter.plot(networks, labels, metric, output_folder, node_ids)
    
    def _create_node_timeline_plots(self, networks, labels, output_folder, node_ids):
        """Create timeline plots for per-node time series data."""
        # Only create timeline plots for metrics that have time series data
        self.line_plotter.plot(networks, labels, "power", output_folder, node_ids)
    
    def create_custom_node_plot(self, plot_type, networks, labels, output_folder, **kwargs):
        """Create a specific type of per-node plot."""
        metric = kwargs.get("metric")
        node_ids = kwargs.get("node_ids")
        
        if plot_type == "bar":
            if metric:
                self.bar_plotter.plot(networks, labels, metric, output_folder, node_ids)
        
        elif plot_type == "heatmap":
            if metric:
                self.heatmap_plotter.plot(networks, labels, metric, output_folder, node_ids)
        
        elif plot_type == "scatter_vs_node":
            if metric:
                self.scatter_plotter.plot_metric_vs_node(networks, labels, metric, output_folder)
        
        elif plot_type == "scatter_two_metrics":
            metric_x = kwargs.get("metric_x")
            metric_y = kwargs.get("metric_y")
            if metric_x and metric_y:
                self.scatter_plotter.plot_two_metrics_per_node(
                    networks, labels, metric_x, metric_y, output_folder)
        
        elif plot_type == "box":
            if metric:
                self.box_plotter.plot(networks, labels, metric, output_folder, node_ids)
        
        elif plot_type == "timeline":
            if metric:
                self.line_plotter.plot(networks, labels, metric, output_folder, node_ids)
        
        else:
            print(f"Unknown per-node plot type: {plot_type}")
    
    def get_available_node_metrics(self, networks):
        """Get list of available per-node metrics from the data."""
        metrics = set()
        
        for network in networks:
            if 'nodes' in network:
                for node_info in network['nodes'].values():
                    if 'power' in node_info:
                        metrics.add('power')
                    if 'latency' in node_info:
                        metrics.add('latency')
                    if 'jitter' in node_info:
                        metrics.add('jitter')
                    if 'packet_delivery_ratio' in node_info:
                        metrics.add('packet_delivery_ratio')
        
        return sorted(metrics)
    
    def get_available_node_ids(self, networks):
        """Get list of available node IDs from the data."""
        node_ids = set()
        
        for network in networks:
            if 'nodes' in network:
                for node_id in network['nodes'].keys():
                    node_ids.add(int(node_id))
        
        return sorted(node_ids)
    
    def get_available_plot_types(self):
        """Get list of available per-node plot types."""
        return [
            "bar", "heatmap", "scatter_vs_node", "scatter_two_metrics",
            "box", "timeline"
        ]
    
    def print_data_summary(self, networks, labels):
        """Print summary of available node data."""
        print("=== Per-Node Data Summary ===")
        
        for net, label in zip(networks, labels):
            print(f"\nProtocol: {label}")
            if 'nodes' in net:
                node_count = len(net['nodes'])
                print(f"  Total nodes: {node_count}")
                
                # Check what metrics are available
                available_metrics = set()
                for node_info in net['nodes'].values():
                    if 'power' in node_info:
                        available_metrics.add('power')
                    if 'delay' in node_info:
                        available_metrics.add('latency')
                    if 'jitter' in node_info:
                        available_metrics.add('jitter')
                    if 'packet_delivery_ratio' in node_info:
                        available_metrics.add('packet_delivery_ratio')
                
                print(f"  Available metrics: {', '.join(sorted(available_metrics))}")
                
                # Show sample node IDs
                node_ids = sorted([int(nid) for nid in net['nodes'].keys()])
                if node_ids:
                    if len(node_ids) <= 10:
                        print(f"  Node IDs: {node_ids}")
                    else:
                        print(f"  Node IDs: {node_ids[:5]} ... {node_ids[-5:]} (showing first 5 and last 5)")
            else:
                print("  No node data available")
        
        print("\n" + "="*40)


# Convenience functions for easy usage
def create_node_plots(files, labels=None, output_folder="plots/nodes", node_ids=None):
    """
    Convenience function to create all per-node plots from JSON files.
    
    Args:
        files: List of JSON file paths
        labels: Optional list of protocol labels
        output_folder: Output directory for plots
        node_ids: Optional list of specific node IDs to plot (None = all nodes)
    """
    manager = NodePlotManager()
    networks, final_labels = manager.load_data(files, labels)
    manager.create_all_node_plots(networks, final_labels, output_folder, node_ids)


def create_custom_node_plot(files, plot_type, metric=None, labels=None, 
                           output_folder="plots/nodes", **kwargs):
    """
    Convenience function to create a specific per-node plot type.
    
    Args:
        files: List of JSON file paths
        plot_type: Type of plot ("bar", "heatmap", "scatter_vs_node", etc.)
        metric: Metric to plot
        labels: Optional list of protocol labels
        output_folder: Output directory for plots
        **kwargs: Additional arguments (node_ids, metric_x, metric_y, etc.)
    """
    manager = NodePlotManager()
    networks, final_labels = manager.load_data(files, labels)
    manager.create_custom_node_plot(plot_type, networks, final_labels, 
                                   output_folder, metric=metric, **kwargs)