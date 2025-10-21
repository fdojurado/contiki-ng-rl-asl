#!/usr/bin/env python3
"""
Per-node plotting script for analyzing individual node metrics.

This script analyzes and visualizes per-node metrics from IoT network simulations,
complementing the network-wide analysis provided by the main plotting system.
"""

import argparse
import sys
from pathlib import Path

# Add the plot directory to the path
plot_dir = Path(__file__).parent / "plot"
sys.path.append(str(plot_dir))

from node_plot_manager import NodePlotManager


def main():
    """Main function for the per-node plotting system."""
    parser = argparse.ArgumentParser(
        description="Analyze and plot per-node metrics from COOJA simulation results."
    )
    parser.add_argument("files", nargs="+", help="List of JSON files to compare")
    parser.add_argument("--labels", nargs="+", help="Optional labels for each file")
    parser.add_argument("--output-folder", type=str, default="plots/nodes", 
                       help="Folder to save the plots (default: plots/nodes)")
    parser.add_argument("--plot-type", type=str, 
                       help="Create only specific plot type (optional)")
    parser.add_argument("--metric", type=str, 
                       help="Create plots only for specific metric (optional)")
    parser.add_argument("--metric-x", type=str, 
                       help="X-axis metric for scatter plots (use with --plot-type scatter_two_metrics)")
    parser.add_argument("--metric-y", type=str, 
                       help="Y-axis metric for scatter plots (use with --plot-type scatter_two_metrics)")
    parser.add_argument("--nodes", nargs="+", type=int, 
                       help="Specific node IDs to analyze (default: all nodes)")
    parser.add_argument("--summary", action="store_true", 
                       help="Print summary of available node data and exit")
    parser.add_argument("--list-metrics", action="store_true", 
                       help="List available node metrics and exit")
    parser.add_argument("--list-plot-types", action="store_true", 
                       help="List available plot types and exit")
    parser.add_argument("--list-nodes", action="store_true", 
                       help="List available node IDs and exit")

    args = parser.parse_args()

    # Initialize node plot manager
    node_manager = NodePlotManager()
    
    # Handle list commands that don't require data loading
    if args.list_plot_types:
        print("Available per-node plot types:")
        for plot_type in node_manager.get_available_plot_types():
            print(f"  - {plot_type}")
        return

    # Load data for other operations
    try:
        networks, labels = node_manager.load_data(args.files, args.labels)
        print(f"Loaded data for {len(networks)} protocols: {labels}")
    except Exception as e:
        print(f"Error loading data: {e}")
        return

    # Handle commands that require loaded data
    if args.list_metrics:
        print("Available node metrics in loaded data:")
        metrics = node_manager.get_available_node_metrics(networks)
        if metrics:
            for metric in metrics:
                print(f"  - {metric}")
        else:
            print("  No node metrics found in the provided data.")
        return
    
    if args.list_nodes:
        print("Available node IDs in loaded data:")
        node_ids = node_manager.get_available_node_ids(networks)
        if node_ids:
            print(f"  Node IDs: {node_ids}")
        else:
            print("  No node data found.")
        return
    
    if args.summary:
        node_manager.print_data_summary(networks, labels)
        return

    # Validate arguments
    if args.labels and len(args.labels) != len(args.files):
        parser.error("Number of labels must match number of files.")
    
    if args.plot_type == "scatter_two_metrics" and (not args.metric_x or not args.metric_y):
        parser.error("--metric-x and --metric-y are required for scatter_two_metrics plot type.")

    # Create plots
    node_ids = args.nodes if args.nodes else None
    
    if args.plot_type:
        # Create specific plot type
        kwargs = {"node_ids": node_ids}
        
        if args.metric:
            kwargs['metric'] = args.metric
        if args.metric_x:
            kwargs['metric_x'] = args.metric_x
        if args.metric_y:
            kwargs['metric_y'] = args.metric_y
        
        print(f"Creating {args.plot_type} plot...")
        if node_ids:
            print(f"Analyzing nodes: {node_ids}")
        
        node_manager.create_custom_node_plot(args.plot_type, networks, labels, 
                                            args.output_folder, **kwargs)
    
    elif args.metric:
        # Create all plot types for specific metric
        print(f"Creating all plot types for node metric: {args.metric}")
        if node_ids:
            print(f"Analyzing nodes: {node_ids}")
        
        # Create basic plots for the metric
        node_manager.create_custom_node_plot("bar", networks, labels, 
                                            args.output_folder, 
                                            metric=args.metric, node_ids=node_ids)
        node_manager.create_custom_node_plot("heatmap", networks, labels, 
                                            args.output_folder, 
                                            metric=args.metric, node_ids=node_ids)
        node_manager.create_custom_node_plot("scatter_vs_node", networks, labels, 
                                            args.output_folder, metric=args.metric)
        node_manager.create_custom_node_plot("box", networks, labels, 
                                            args.output_folder, 
                                            metric=args.metric, node_ids=node_ids)
        
        # Create timeline for power metric
        if args.metric == "power":
            node_manager.create_custom_node_plot("timeline", networks, labels, 
                                                args.output_folder, 
                                                metric=args.metric, node_ids=node_ids)
    
    else:
        # Create all plots
        print("Creating all per-node plots...")
        if node_ids:
            print(f"Analyzing nodes: {node_ids}")
        
        node_manager.create_all_node_plots(networks, labels, args.output_folder, node_ids)


def example_usage():
    """
    Print example usage commands.
    """
    print("\n=== Example Usage ===")
    print("# Create all per-node plots:")
    print("python plot_nodes.py protocol1.json protocol2.json")
    print()
    print("# Analyze specific nodes only:")
    print("python plot_nodes.py protocol1.json protocol2.json --nodes 2 3 4 5")
    print()
    print("# Create bar plots for power consumption:")
    print("python plot_nodes.py protocol1.json protocol2.json --plot-type bar --metric power")
    print()
    print("# Create heatmap for latency:")
    print("python plot_nodes.py protocol1.json protocol2.json --plot-type heatmap --metric latency")
    print()
    print("# Compare power vs latency per node:")
    print("python plot_nodes.py protocol1.json protocol2.json --plot-type scatter_two_metrics --metric-x power --metric-y latency")
    print()
    print("# Get summary of available data:")
    print("python plot_nodes.py protocol1.json protocol2.json --summary")
    print()
    print("# List available plot types:")
    print("python plot_nodes.py --list-plot-types")
    print()


if __name__ == "__main__":
    # Check if no arguments provided, show help
    if len(sys.argv) == 1:
        print("Per-Node Network Analysis Tool")
        print("=" * 40)
        print("This tool creates per-node visualizations from simulation results.")
        print("Use --help for detailed options.")
        example_usage()
    else:
        main()