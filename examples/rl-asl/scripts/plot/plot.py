"""
Refactored plotting script using class-based architecture.
"""

import argparse
from plot_manager import PlotManager


def main():
    """Main function for the refactored plotting system."""
    parser = argparse.ArgumentParser(
        description="Compare COOJA simulation results from multiple JSON files using refactored plotting system."
    )
    parser.add_argument("files", nargs="+", help="List of JSON files to compare")
    parser.add_argument("--labels", nargs="+", help="Optional labels for each file")
    parser.add_argument("--output-folder", type=str, default="plots", 
                       help="Folder to save the plots")
    parser.add_argument("--plot-type", type=str, 
                       help="Create only specific plot type (optional)")
    parser.add_argument("--metric", type=str, 
                       help="Create plots only for specific metric (optional)")
    parser.add_argument("--list-metrics", action="store_true", 
                       help="List available metrics and exit")
    parser.add_argument("--list-plot-types", action="store_true", 
                       help="List available plot types and exit")

    args = parser.parse_args()

    # Initialize plot manager
    plot_manager = PlotManager()
    
    # Handle list commands
    if args.list_metrics:
        print("Available metrics:")
        for metric in plot_manager.get_available_metrics():
            print(f"  - {metric}")
        return
    
    if args.list_plot_types:
        print("Available plot types:")
        for plot_type in plot_manager.get_available_plot_types():
            print(f"  - {plot_type}")
        return

    # Validate arguments
    if args.labels and len(args.labels) != len(args.files):
        parser.error("Number of labels must match number of files.")

    # Load data
    try:
        networks, labels = plot_manager.load_data(args.files, args.labels)
        print(f"Loaded data for {len(networks)} protocols: {labels}")
    except Exception as e:
        print(f"Error loading data: {e}")
        return

    # Create plots
    if args.plot_type:
        # Create specific plot type
        kwargs = {}
        if args.metric:
            kwargs['metric'] = args.metric
        
        print(f"Creating {args.plot_type} plot...")
        plot_manager.create_custom_plot(args.plot_type, networks, labels, 
                                      args.output_folder, **kwargs)
    
    elif args.metric:
        # Create all plot types for specific metric
        print(f"Creating all plot types for metric: {args.metric}")
        
        # Create basic plots for the metric
        plot_manager.create_custom_plot("bar", networks, labels, 
                                      args.output_folder, metric=args.metric)
        plot_manager.create_custom_plot("line", networks, labels, 
                                      args.output_folder, metric=args.metric)
        plot_manager.create_custom_plot("violin", networks, labels, 
                                      args.output_folder, metric=args.metric, kind="violin")
        plot_manager.create_custom_plot("violin", networks, labels, 
                                      args.output_folder, metric=args.metric, kind="box")
        
        # Create CDF and histogram for latency/jitter
        if args.metric in ["latency", "jitter"]:
            plot_manager.create_custom_plot("cdf", networks, labels, 
                                          args.output_folder, metric=args.metric)
            plot_manager.create_custom_plot("histogram", networks, labels, 
                                          args.output_folder, metric=args.metric)
    
    else:
        # Create all plots
        print("Creating all plots...")
        plot_manager.create_all_plots(networks, labels, args.output_folder)


if __name__ == "__main__":
    main()