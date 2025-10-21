"""
Abstract base class for metric plotters with common functionality.
"""

from abc import ABC, abstractmethod
import os
import json
import numpy as np
import matplotlib.pyplot as plt
from plot_config import PlotConfig
from plot_styler import PlotStyler


class MetricPlotter(ABC):
    """Base class for all metric plotting operations."""
    
    def __init__(self, config: PlotConfig = None, styler: PlotStyler = None):
        """Initialize with configuration and styling."""
        self.config = config or PlotConfig
        self.styler = styler or PlotStyler(self.config)
    
    @staticmethod
    def load_json(file):
        """Load JSON data and extract label."""
        with open(file, "r") as f:
            data = json.load(f)
        label = list(data.keys())[0]
        return data[label], label
    
    def get_metric_values(self, net, metric):
        """Extract metric values with proper scaling."""
        info = self.config.METRIC_INFO[metric]
        suffix = info.get("suffix", "")
        scale = info.get("scale", 1.0)

        if suffix:
            avg = net["network"][metric][f"avg_{suffix}"] * scale
            std = net["network"][metric][f"std_{suffix}"] * scale
        else:
            avg = net["network"][metric]["avg"] * scale
            std = net["network"][metric]["std"] * scale

        return avg, std
    
    def sort_protocols_by_metric(self, labels, networks, metric):
        """Sort protocols by metric values according to configuration."""
        info = self.config.METRIC_INFO[metric]
        avg_vals = []
        
        for net in networks:
            avg, _ = self.get_metric_values(net, metric)
            avg_vals.append(avg)
        
        combined = list(zip(labels, networks, avg_vals))
        reverse_sort = True if info.get("sort", "asc") == "desc" else False
        combined.sort(key=lambda x: x[2], reverse=reverse_sort)
        
        return [item[0] for item in combined], [item[1] for item in combined]
    
    def get_per_sample_data(self, net, metric):
        """Extract per-sample data for a metric."""
        info = self.config.METRIC_INFO[metric]
        suffix = info.get("suffix", "")
        scale = info.get("scale", 1.0)
        
        if suffix:
            per_sample = net["network"][metric][f"per_sample_avg_{suffix}"]
        else:
            per_sample = net["network"][metric]["per_sample_avg"]
        
        return per_sample, scale
    
    def create_figure(self, metric, plot_type):
        """Create a figure with proper sizing."""
        figsize = self.config.get_figsize(metric, plot_type=plot_type)
        return plt.subplots(figsize=figsize)
    
    def save_plot(self, fig, filename, output_folder):
        """Save plot with consistent formatting."""
        output_path = os.path.join(output_folder, filename)
        self.styler.finalize_plot(fig, output_path)
    
    @abstractmethod
    def plot(self, networks, labels, metric, output_folder):
        """Abstract method that must be implemented by subclasses."""
        pass