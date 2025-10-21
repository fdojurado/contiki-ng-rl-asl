"""
Configuration module for plotting system.
Contains all constants, styling information, and metric definitions.
"""

import numpy as np
from matplotlib.colors import to_rgba


class PlotConfig:
    """Centralized configuration for all plotting operations."""
    
    # Font sizes
    Y_LABEL_FONT_SIZE = 27
    X_LABEL_FONT_SIZE = 23
    X_TICK_FONT_SIZE = 17
    Y_TICK_FONT_SIZE = 20
    X_TICK_HEATMAP_FONT_SIZE = 10
    Y_TICK_HEATMAP_FONT_SIZE = 10
    LEGEND_FONT_SIZE = 18
    STANDARD_FIG_SIZE = (6, 3)
    
    # Protocol information
    DATA_INFO = {
        # Your approaches (SAGE) – strong, consistent blue tones, full opacity
        "RL-ASL": {"color": "#1f77b4", "label": "RL-ASL", "alpha": 1.0, "hatch": ""},
        # Baselines – distinct colors, but with transparency to look less strong
        "Orchestra": {"color": "#2ca02c", "label": "Orch.-RB", "alpha": 0.6},
        "Orchestra-link-based": {"color": "#ff7f0e", "label": "Orch.-LB", "alpha": 0.6},
        "RL-ASL-link-based": {"color": "#9467bd", "label": "RL-ASL-LB", "alpha": 0.6},
        "PRIL-M": {"color": "#d62728", "label": "PRIL-M", "alpha": 0.6},
    }
    
    # Metric configuration
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
    
    @classmethod
    def get_label_color(cls, label):
        """Get color for a protocol label."""
        return cls.DATA_INFO.get(label, {}).get("color", "#000000")
    
    @classmethod
    def get_label_alpha(cls, label):
        """Get alpha value for a protocol label."""
        return cls.DATA_INFO.get(label, {}).get("alpha", 1.0)
    
    @classmethod
    def get_label_name(cls, label):
        """Get display name for a protocol label."""
        return cls.DATA_INFO.get(label, {}).get("label", label)
    
    @classmethod
    def get_label_hatch(cls, label):
        """Get hatch pattern for a protocol label."""
        return cls.DATA_INFO.get(label, {}).get("hatch", None)
    
    @classmethod
    def get_palette(cls, labels):
        """Get color palette for a list of labels."""
        return {
            cls.get_label_name(lbl): to_rgba(
                cls.DATA_INFO[lbl]["color"],
                cls.DATA_INFO[lbl].get("alpha", 1.0)
            )
            for lbl in labels
        }
    
    @classmethod
    def get_fontsize(cls, metric, key, default=None, plot_type=None):
        """Fetch font size for a given metric and plot type."""
        info = cls.METRIC_INFO.get(metric, {})
        plot_styles = info.get("plot_styles", {})
        if plot_type and plot_type in plot_styles:
            fonts = plot_styles[plot_type].get("fonts", {})
            return fonts.get(key, default)
        fonts = info.get("fonts", {})
        return fonts.get(key, default)
    
    @classmethod
    def get_figsize(cls, metric, default=None, plot_type=None):
        """Get figure size for a metric and plot type."""
        if default is None:
            default = cls.STANDARD_FIG_SIZE
        info = cls.METRIC_INFO.get(metric, {})
        plot_styles = info.get("plot_styles", {})
        if plot_type and plot_type in plot_styles:
            return plot_styles[plot_type].get("figsize", default)
        return info.get("figsize", default)
    
    @classmethod
    def use_legend(cls, metric, plot_type=None, default=True):
        """Check if legend should be used for a metric and plot type."""
        info = cls.METRIC_INFO.get(metric, {})
        info_plot_styles = info.get("plot_styles", {})
        if plot_type and plot_type in info_plot_styles:
            return info_plot_styles[plot_type].get("legend", default)
        return info.get("legend", default)
    
    @classmethod
    def show_axis_label(cls, metric, axis, plot_type=None, default=True):
        """Decide whether to show x/y label for a given metric + plot type."""
        info = cls.METRIC_INFO.get(metric, {})
        styles = info.get("plot_styles", {})
        if plot_type and plot_type in styles:
            show_cfg = styles[plot_type].get("show_labels", {})
            return show_cfg.get(axis, default)
        return default