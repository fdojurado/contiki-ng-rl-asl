"""
Plot styling utilities for consistent appearance across all plots.
"""

import matplotlib.pyplot as plt
from plot_config import PlotConfig


class PlotStyler:
    """Handles all styling operations for plots."""
    
    def __init__(self, config: PlotConfig = None):
        """Initialize with optional custom configuration."""
        self.config = config or PlotConfig
    
    def apply_seaborn_style(self):
        """Apply global seaborn styling."""
        import seaborn as sns
        sns.set(style="whitegrid")
    
    def style_axes(self, ax, metric=None, plot_type=None):
        """Apply consistent axis styling."""
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.spines["left"].set_linewidth(3)
        ax.spines["left"].set_edgecolor("black")
        ax.spines["bottom"].set_linewidth(3)
        ax.spines["bottom"].set_edgecolor("black")

        if metric:
            ax.tick_params(axis="y", labelsize=self.config.get_fontsize(
                metric, "ytick", self.config.Y_TICK_FONT_SIZE, plot_type=plot_type))
            ax.tick_params(axis="x", labelsize=self.config.get_fontsize(
                metric, "xtick", self.config.X_TICK_FONT_SIZE, plot_type=plot_type))
        else:
            ax.tick_params(axis="y", labelsize=self.config.Y_TICK_FONT_SIZE)
            ax.tick_params(axis="x", labelsize=self.config.X_TICK_FONT_SIZE)
    
    def apply_axis_limits(self, ax, metric):
        """Apply axis limits if specified in metric config."""
        info = self.config.METRIC_INFO[metric]
        if "ylim" in info:
            ax.set_ylim(info["ylim"])
    
    def set_axis_labels(self, ax, metric, plot_type, x_label=None, y_label=None):
        """Set axis labels with proper styling."""
        info = self.config.METRIC_INFO[metric]
        
        # X-axis label
        if self.config.show_axis_label(metric, "x", plot_type=plot_type):
            xlabel = x_label or "Sample" if plot_type == "line" else ""
            if xlabel:
                ax.set_xlabel(xlabel, fontsize=self.config.get_fontsize(
                    metric, "xlabel", self.config.X_LABEL_FONT_SIZE, plot_type=plot_type))
        else:
            ax.set_xlabel("")
        
        # Y-axis label
        if self.config.show_axis_label(metric, "y", plot_type=plot_type):
            ylabel = y_label or info["label_with_units"]
            ax.set_ylabel(ylabel, fontsize=self.config.get_fontsize(
                metric, "ylabel", self.config.Y_LABEL_FONT_SIZE, plot_type=plot_type))
        else:
            ax.set_ylabel("")
    
    def add_legend(self, ax, metric, plot_type, handles=None, labels=None, **kwargs):
        """Add legend if specified in configuration."""
        if self.config.use_legend(metric, plot_type=plot_type):
            legend_args = {
                "fontsize": self.config.get_fontsize(
                    metric, "legend", self.config.LEGEND_FONT_SIZE, plot_type=plot_type),
                "loc": "best"
            }
            legend_args.update(kwargs)
            
            if handles and labels:
                ax.legend(handles, labels, **legend_args)
            else:
                ax.legend(**legend_args)
    
    def get_plot_colors_and_styles(self, labels):
        """Get colors, alphas, and hatches for a list of labels."""
        colors = [self.config.get_label_color(lbl) for lbl in labels]
        alphas = [self.config.get_label_alpha(lbl) for lbl in labels]
        hatches = [self.config.get_label_hatch(lbl) for lbl in labels]
        pretty_labels = [self.config.get_label_name(lbl) for lbl in labels]
        
        return {
            'colors': colors,
            'alphas': alphas,
            'hatches': hatches,
            'pretty_labels': pretty_labels
        }
    
    def finalize_plot(self, fig, output_path, bbox_inches="tight", pad_inches=0.0):
        """Apply final styling and save the plot."""
        fig.tight_layout()
        fig.savefig(output_path, format="pdf", bbox_inches=bbox_inches, pad_inches=pad_inches)
        plt.close(fig)