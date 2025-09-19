import os
import json
import logging
from pathlib import Path
import numpy as np
import fit_iot_lab_conf

logger = logging.getLogger('process_common')


def get_time(line, start_ts_unix=None, is_testbed=False):
    if is_testbed:
        ts_unix = float(line[0])
        if start_ts_unix is None:
            start_ts_unix = ts_unix
        ts_unix -= start_ts_unix
        return int(ts_unix * 1e6), start_ts_unix
    else:
        return int(line[0]), None


def get_node_id(line, is_testbed=False):
    if is_testbed:
        platform = line[1].split('-')[1]
        node_info = fit_iot_lab_conf.get_node_config(platform)
        return node_info["node_id"]
    return int(line[1])


def unique_node_ids(testlog):
    node_ids = []
    try:
        with open(testlog, "r") as f:
            contents = f.read()
            contents = contents.split('\n')
            # Skip the first two lines
            contents = contents[2:]
            for line in contents:
                if "Script timed out." in line:
                    break
                # if the line is empty, skip it
                if not line:
                    continue
                line = line.split()
                node_id = get_node_id(line)
                node_ids.append(int(node_id))
    except FileNotFoundError:
        pass
    except PermissionError as ex:
        print(f"Cannot read testlog: {ex}")
        return False
    # Set to have only unique node ids
    node_ids = set(node_ids)
    return node_ids


def save_results(results, output_folder, testlog_name):
    if results is not None:
        # Create the folder if it does not exist
        if not os.path.exists(output_folder):
            os.makedirs(output_folder)
        # Save the results in a csv file
        with open(os.path.join(output_folder, testlog_name+".json"), "w") as f:
            f.write(json.dumps(results))


def process_throughput(node, msg, time):
    # Get the packet size which is the number after the "len " keyword
    packet_size = msg.split("len ")[1]
    # Split by ',' and pick the first element
    packet_size = int(packet_size.split(",")[0])
    # Add time at Tx to the node, for throughput calculation
    node.throughput_add(packet_size=packet_size, time=time)


def calc_network_avg_power(network):
    power = {}
    # Lets calculate the average power consumption for the node in this period
    for node in network.nodes.values():
        if node.id > 1:
            node.power_print()
            node_avg_power = node.power_get_average()
            if node_avg_power is not None:
                power.update({node.id: node_avg_power})
    # Lets calculate the average power consumption for the network
    if (len(power) == 0):
        logger.warning("No power values found")
        return None, None
    network_avg_power = sum(power.values()) / len(power.values())
    # Lets calculate the standard deviation of the power consumption for the network
    network_std_power = np.std(list(power.values()))
    return network_avg_power, network_std_power


def calc_power(network):
    power = {}
    # Lets calculate the power consumption for the node in this period
    for node in network.nodes.values():
        if node.id > 1:
            # node.power_print()
            node_power = node.power_get_average()
            if node_power is not None:
                power.update({node.id: node_power})
    return power


def calc_packet_loss(network):
    packet_loss = {}
    # Lets calculate the packet loss for the node in this period
    for node in network.nodes.values():
        if node.id > 1:
            node_packet_loss = node.packet_loss_get_percentage()
            if node_packet_loss is not None:
                packet_loss.update({node.id: node_packet_loss})
    return packet_loss


def calc_throughput(network):
    throughput = {}
    # Lets calculate the throughput for the node in this period
    for node in network.nodes.values():
        if node.id > 1:
            node_throughput = node.throughput_calculate()
            if node_throughput is not None:
                throughput.update({node.id: node_throughput})
    return throughput


def calc_jitter(network):
    jitter = {}
    # Lets calculate the jitter for the node in this period
    for node in network.nodes.values():
        if node.id > 1:
            # print(f"Node {node.id} jitter:")
            # node.delay_print()
            node_jitter = node.jitter_get_average()
            if node_jitter is not None:
                jitter.update({node.id: node_jitter})
    return jitter


def calculate_results(network):
    # The results are stored in a dictionary like this:
    # { node_id: { metric_name: { value: <value>, unit: <unit> } } }
    results = {}
    # Calculate the average power consumption for the network using power trace
    network.calc_avg_power_trace_consumption(results)
    # Calculate the power consumption for each node using power trace
    network.calc_power_trace(results)
    # Calculate the power consumption due to uc_rx for each node using power trace
    network.calc_uc_power_trace(results)
    # Calculate the average energy consumption for the network using power trace
    network.calc_avg_energy_trace_consumption(results)
    # Calculate the energy consumption for each node using power trace
    network.calc_energy_trace(results)
    # Calculate the average latency for the network
    network.calc_avg_latency(results)
    # Calculate the latency for each node
    network.calc_latency(results)
    # Latency for each node ordered by timeslots
    network.calc_latency_ordered_by_timeslots(results)
    # Calculate the packet loss for the network
    network.calc_avg_packet_loss(results)
    # Calculate the packet loss for each node
    network.calc_packet_loss(results)
    # Calculate the PDR for the network
    network.calc_avg_pdr(results)
    # Calculate the PDR for each node
    network.calc_pdr(results)
    # Calculate the average jitter for the network
    network.calc_avg_jitter(results)
    # Calculate the jitter for each node
    network.calc_jitter(results)
    # Calculate the average duty cycle for the network
    network.calc_avg_duty_cycle(results)
    # Calculate the duty cycle for each node
    network.calc_duty_cycle(results)
    # Clear all performance metrics
    network.nodes_performance_metrics_clear()

    return results


def preprocess_testlog(testlog, output_folder, process_testlog, args):
    results = {}
    # Is this a directory?
    directory = False
    if os.path.isdir(testlog):
        directory = True
    # If it is a directory, then process all the testlog files in the directory
    if directory:
        logger.info(f"Processing all testlog files in the directory {testlog}")
        for file in os.listdir(testlog):
            if file.endswith(".testlog"):
                results = {}
                logger.info(f"Processing testlog file {file}")
                file_path = os.path.join(testlog, file)
                testlog_name = os.path.splitext(os.path.basename(file_path))[0]
                testlog_name = testlog_name
                results[testlog_name] = process_testlog(Path(file_path), args)

                # Save the results
                save_results(results,
                             output_folder, testlog_name)
    else:
        logger.info(f"Processing testlog file {testlog}")
        testlog_name = os.path.splitext(os.path.basename(testlog))[0]
        results[testlog_name] = process_testlog(Path(testlog), args)
        logger.info(f"Results: {results}")

        # Save the results in a csv file in the output folder if it is not none
        save_results(results, output_folder, testlog_name)


def process_root_folder(root_folder, process_testlog, args):
    # The root folder might be MSF/
    # Here I want to loop through all the child folders and child of child folders until I get to the testlog files
    for root, dirs, files in os.walk(root_folder):
        # are there any testlog files in this folder?
        testlog_files = [f for f in files if f.endswith(".testlog")]
        # If there are testlog files in this folder, then process the directory
        if testlog_files:
            preprocess_testlog(root, root, process_testlog, args)
