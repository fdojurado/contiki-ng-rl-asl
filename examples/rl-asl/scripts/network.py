#!/usr/bin/python3
#
# Copyright (C) 2022  Fernando Jurado-Lasso <ffjla@dtu.dk>

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
from pathlib import Path
from typing import Dict, Tuple, Optional, List, Any, Callable

import logging
import numpy as np
import json

from node import Node


logger = logging.getLogger(f'main.{__name__}')


class Network:
    def __init__(
        self,
    ) -> None:
        self.nodes: Dict[int, Node] = {}

    def nodes_clear(self) -> None:
        self.nodes = {}

    def nodes_size(self) -> int:
        return len(self.nodes)

    def nodes_get(self, id: int) -> Optional[Node]:
        return self.nodes.get(id)

    # Get the number of nodes excluding node id 0
    def nodes_count(self) -> int:
        return len(self.nodes) - 1

    def nodes_add(
        self,
        id: int,
        sid: Optional[int] = None,
    ) -> Node:
        node = self.nodes_get(id)
        if node is not None:
            # logger.debug(f"Node ID {id} already exists.")
            return node
        node = Node(id, sid=sid)
        self.nodes[id] = node
        return node

    def nodes_print(self) -> None:
        for node in self.nodes.values():
            node.neighbor_print()
            node.tsch_print()
            node.route_print()
            node.energy_print()
            node.delay_print()
            node.pdr_print()

    def nodes_performance_metrics_clear(self) -> None:
        for node in self.nodes.values():
            node.performance_metrics_clear()

    def calc_avg_power_trace_consumption(self, results: Dict[int, Dict[str, Any]]) -> None:
        power = {}
        # Lets calculate the power consumption for the node in this period
        for node in self.nodes.values():
            if node.id > 1:
                power_samples = node.power_trace.get_samples()
                node_avg_power = node.power_trace_get_average()
                if node_avg_power is not None:
                    power[node.id] = {
                        'average_mW': node_avg_power['average_mW'],
                        'samples_mW': {seq: sample.power for seq, sample in power_samples.items() if sample.power is not None},
                    }

        if not power:
            logger.warning("No power values found")
            return {}

        # -- Per-node averages ---
        node_avgs = [power[node_id]['average_mW'] for node_id in power]

        # --- Per-sample averages across nodes ---
        per_sample_avgs = {}
        # get union of all sample keys
        all_seqs = set().union(
            *[power[node_id]['samples_mW'].keys() for node_id in power])
        for seq in sorted(all_seqs):
            values = [power[node_id]['samples_mW'][seq]
                      for node_id in power if seq in power[node_id]['samples_mW']]
            if values:
                per_sample_avgs[seq] = float(np.mean(values))

        # --- Network-wide stats ---
        network_avg_power = float(np.mean(node_avgs))
        network_std_power = float(np.std(node_avgs))

        # Store results
        if 'network' not in results:
            results['network'] = {}
        if 'power' not in results['network']:
            results['network']['power'] = {}

        results['network']['power']['avg_mW'] = network_avg_power
        results['network']['power']['std_mW'] = network_std_power
        results['network']['power']['per_sample_avg_mW'] = per_sample_avgs

    def calc_power_trace(self, results: Dict[int, Dict[str, Any]]) -> None:
        nodes_sorted = sorted(self.nodes.values(), key=lambda x: x.id)
        for node in nodes_sorted:
            if node.id > 1:
                power_trace = node.power_trace.get_samples()
                node_avg_power = node.power_trace_get_average()
                if node_avg_power is not None:
                    # check if the node id is already in results
                    if node.id not in results:
                        results[node.id] = {}
                    results[node.id]['power'] = {
                        'joined_time': node.joined_time,
                        'average_mW': node_avg_power['average_mW'],
                    }
                    for seq, sample in power_trace.items():
                        if sample.power is not None:
                            results[node.id]['power'].setdefault('samples_mW', {})[seq] = {
                                'power': sample.power,
                                'time': sample.time
                            }

    def calc_uc_power_trace(self, results: Dict[int, Dict[str, Any]]) -> None:
        nodes_sorted = sorted(self.nodes.values(), key=lambda x: x.id)
        for node in nodes_sorted:
            if node.id > 1:
                power_trace = node.power_trace.get_samples()
                node_avg_power = node.power_trace_get_average()
                if node_avg_power is not None:
                    # check if the node id is already in results
                    if node.id not in results:
                        results[node.id] = {}
                    results[node.id]['uc_power'] = {
                        'joined_time': node.joined_time,
                        'average_mW': node_avg_power['average_uc_mW'],
                    }
                    for seq, sample in power_trace.items():
                        if sample.uc_power is not None:
                            results[node.id]['uc_power'].setdefault('samples_mW', {})[seq] = {
                                'uc_power': sample.uc_power,
                                'time': sample.time
                            }

    def calc_cpu_radio_activity(self, results: Dict[int, Dict[str, Any]]) -> None:
        cpu_activity = {}
        radio_activity = {}
        for node in self.nodes.values():
            if node.id > 1:
                samples = node.power_trace.get_samples()
                avg_cpu = node.power_trace_get_cpu_activity_average()
                avg_radio = node.power_trace_get_radio_activity_average()
                if avg_cpu is not None:
                    cpu_activity[node.id] = {
                        'average': avg_cpu,
                        'samples': {seq: sample.cpu_activity for seq, sample in samples.items() if sample.cpu_activity is not None}
                    }
                if avg_radio is not None:
                    radio_activity[node.id] = {
                        'average': avg_radio,
                        'samples': {seq: sample.radio_activity for seq, sample in samples.items() if sample.radio_activity is not None}
                    }

        if not cpu_activity and not radio_activity:
            logger.warning("No cpu or radio activity values found")
            return

        if cpu_activity:
            # --- Per-node averages ---
            node_avgs = [cpu_activity[node_id]['average']
                         for node_id in cpu_activity]

            # --- Per-sample averages across nodes ---
            per_sample_avgs = {}
            # get union of all sample keys
            all_seqs = set().union(
                *[set(cpu_activity[node_id]['samples'].keys()) for node_id in cpu_activity])
            for seq in sorted(all_seqs):
                values = [cpu_activity[node_id]['samples'][seq]
                          for node_id in cpu_activity if seq in cpu_activity[node_id]['samples']]
                if values:
                    per_sample_avgs[seq] = float(np.mean(values))

            # --- Network-wide stats ---
            network_avg_cpu = float(np.mean(node_avgs))
            network_std_cpu = float(np.std(node_avgs))

            # Store results
            if 'network' not in results:
                results['network'] = {}
            if 'cpu_activity' not in results['network']:
                results['network']['cpu_activity'] = {}

            results['network']['cpu_activity']['avg'] = network_avg_cpu
            results['network']['cpu_activity']['std'] = network_std_cpu
            results['network']['cpu_activity']['per_sample_avg'] = per_sample_avgs

        if radio_activity:
            # --- Per-node averages ---
            node_avgs = [radio_activity[node_id]['average']
                         for node_id in radio_activity]

            # --- Per-sample averages across nodes ---
            per_sample_avgs = {}
            # get union of all sample keys
            all_seqs = set().union(
                *[set(radio_activity[node_id]['samples'].keys()) for node_id in radio_activity])
            for seq in sorted(all_seqs):
                values = [radio_activity[node_id]['samples'][seq]
                          for node_id in radio_activity if seq in radio_activity[node_id]['samples']]
                if values:
                    per_sample_avgs[seq] = float(np.mean(values))

            # --- Network-wide stats ---
            network_avg_radio = float(np.mean(node_avgs))
            network_std_radio = float(np.std(node_avgs))

            # Store results
            if 'network' not in results:
                results['network'] = {}
            if 'radio_activity' not in results['network']:
                results['network']['radio_activity'] = {}

            results['network']['radio_activity']['avg'] = network_avg_radio
            results['network']['radio_activity']['std'] = network_std_radio
            results['network']['radio_activity']['per_sample_avg'] = per_sample_avgs

    def calc_cpu_radio_activity_per_node(self, results: Dict[int, Dict[str, Any]]) -> None:
        nodes_sorted = sorted(self.nodes.values(), key=lambda x: x.id)
        for node in nodes_sorted:
            if node.id > 1:
                samples = node.power_trace.get_samples()
                avg_cpu = node.power_trace_get_cpu_activity_average()
                avg_radio = node.power_trace_get_radio_activity_average()
                if avg_cpu is not None:
                    if node.id not in results:
                        results[node.id] = {}
                    results[node.id]['cpu_activity'] = {
                        'joined_time': node.joined_time,
                        'average': avg_cpu,
                    }
                    for seq, sample in samples.items():
                        if sample.cpu_activity is not None:
                            results[node.id]['cpu_activity'].setdefault('samples', {})[seq] = {
                                'cpu_activity': sample.cpu_activity,
                                'time': sample.time
                            }
                if avg_radio is not None:
                    if node.id not in results:
                        results[node.id] = {}
                    results[node.id]['radio_activity'] = {
                        'joined_time': node.joined_time,
                        'average': avg_radio,
                    }
                    for seq, sample in samples.items():
                        if sample.radio_activity is not None:
                            results[node.id]['radio_activity'].setdefault('samples', {})[seq] = {
                                'radio_activity': sample.radio_activity,
                                'time': sample.time
                            }

    def calc_avg_energy_trace_consumption(self, results: Dict[int, Dict[str, Any]]) -> None:
        energy_trace = {}
        for node in self.nodes.values():
            if node.id > 1:
                energy_trace_samples = node.power_trace.get_samples()
                last_energy_trace = node.power_trace.get_sample_last()
                if last_energy_trace is not None:
                    energy_trace[node.id] = {
                        'energy_mJ': last_energy_trace.energy,
                        'samples_mJ': {seq: sample.energy for seq, sample in energy_trace_samples.items() if sample.energy is not None}
                    }
                else:
                    print(f"Node {node.id} has no last energy trace sample")

        if not energy_trace:
            logger.warning("No energy trace values found")
            return

        # -- Per-node averages ---
        node_avgs = [energy_trace[node_id]['energy_mJ']
                     for node_id in energy_trace]

        # --- Per-sample averages across nodes ---
        per_sample_avgs = {}
        # get union of all sample keys
        all_seqs = set().union(
            *[energy_trace[node_id]['samples_mJ'].keys() for node_id in energy_trace]
        )
        for seq in sorted(all_seqs):
            values = [energy_trace[node_id]['samples_mJ'][seq]
                      for node_id in energy_trace if seq in energy_trace[node_id]['samples_mJ']]
            if values:
                per_sample_avgs[seq] = float(np.mean(values))

        # --- Network-wide stats ---
        network_avg_energy = float(np.mean(node_avgs))
        network_std_energy = float(np.std(node_avgs))

        # Store results
        if 'network' not in results:
            results['network'] = {}
        if 'energy' not in results['network']:
            results['network']['energy'] = {}

        results['network']['energy']['avg_mJ'] = network_avg_energy
        results['network']['energy']['std_mJ'] = network_std_energy
        results['network']['energy']['per_sample_avg_mJ'] = per_sample_avgs

    def calc_energy_trace(self, results: Dict[int, Dict[str, Any]]) -> None:
        nodes_sorted = sorted(self.nodes.values(), key=lambda x: x.id)
        for node in nodes_sorted:
            if node.id > 1:
                energy_samples = node.power_trace.get_samples()
                last_energy_sample = node.power_trace.get_sample_last()
                if last_energy_sample is not None:
                    if node.id not in results:
                        results[node.id] = {}
                    results[node.id]['energy'] = {
                        'joined_time': node.joined_time,
                        'energy_mJ': last_energy_sample.energy,
                    }
                    for seq, sample in energy_samples.items():
                        if sample.energy is not None:
                            results[node.id]['energy'].setdefault('samples_mJ', {})[seq] = {
                                'Energy': sample.energy,
                                'time': sample.time
                            }

    def calc_avg_latency(self, results: Dict[int, Dict[str, Any]]) -> None:
        latency = {}
        for node in self.nodes.values():
            if node.id > 1:
                delay_samples = node.delay.get_samples()
                node_avg_latency = node.delay_get_average()
                if node_avg_latency is not None:
                    latency[node.id] = {
                        'average_us': node_avg_latency['microseconds'],
                        'samples_us': {seq: sample.delay for seq, sample in delay_samples.items() if sample.delay is not None},
                    }

        if not latency:
            logger.warning("No latency values found")
            return {}

        # --- Per-node averages ---
        node_avgs = [latency[node_id]['average_us'] for node_id in latency]

        # --- Per-sample averages across nodes ---
        per_sample_avgs = {}
        # get union of all sample keys
        all_seqs = set().union(
            *[set(latency[node_id]['samples_us'].keys()) for node_id in latency])
        for seq in sorted(all_seqs):
            values = [latency[node_id]['samples_us'][seq]
                      for node_id in latency if seq in latency[node_id]['samples_us']]
            if values:
                per_sample_avgs[seq] = float(np.mean(values))

        # --- Network-wide stats ---
        network_avg_latency = float(np.mean(node_avgs))
        network_std_latency = float(np.std(node_avgs))

        # Store results
        if 'network' not in results:
            results['network'] = {}
        if 'latency' not in results['network']:
            results['network']['latency'] = {}

        results['network']['latency']['avg_latency_us'] = network_avg_latency
        results['network']['latency']['std_latency_us'] = network_std_latency
        results['network']['latency']['per_sample_avg_us'] = per_sample_avgs

    def calc_latency(self, results: Dict[int, Dict[str, Any]]) -> None:
        # Lets loop through the unique node id and sort them first
        nodes_sorted = sorted(self.nodes.values(), key=lambda x: x.id)
        for node in nodes_sorted:
            if node.id > 1:
                delay_samples = node.delay.get_samples()
                node_avg_latency = node.delay_get_average()
                if node_avg_latency is not None:
                    # check if the node id is already in results
                    if node.id not in results:
                        results[node.id] = {}
                    results[node.id]['latency'] = {
                        'average_us': node_avg_latency['microseconds'],
                        'samples_us': {seq: sample.delay for seq, sample in delay_samples.items() if sample.delay is not None},
                    }

    def calc_avg_packet_loss(self, results: Dict[int, Dict[str, Any]]) -> None:
        plr = {}
        for node in self.nodes.values():
            if node.id > 1:
                plr_over_sequence = node.delay.get_plr_over_sequence()
                node_avg_plr = node.packet_loss_get_percentage()
                if node_avg_plr is not None:
                    plr[node.id] = {
                        'average': node_avg_plr,
                        'samples': plr_over_sequence
                    }

        if not plr:
            logger.warning("No packet loss values found")
            return

        # --- Per-node averages
        node_avgs = [plr[node_id]['average'] for node_id in plr]

        # -- Per-sample averages across nodes ---
        per_sample_avgs = {}
        # get union of all sample keys
        all_seqs = set().union(
            *[set(plr[node_id]['samples'].keys()) for node_id in plr]
        )
        for seq in sorted(all_seqs):
            values = [plr[node_id]['samples'][seq]
                      for node_id in plr if seq in plr[node_id]['samples']]
            if values:
                per_sample_avgs[seq] = float(np.mean(values))

        # --- Network-wide stats ---
        network_avg_plr = float(np.mean(node_avgs))
        network_std_plr = float(np.std(node_avgs))

        # Store results
        if 'network' not in results:
            results['network'] = {}
        if 'packet_loss' not in results['network']:
            results['network']['packet_loss'] = {}

        results['network']['packet_loss']['avg'] = network_avg_plr
        results['network']['packet_loss']['std'] = network_std_plr
        results['network']['packet_loss']['samples'] = per_sample_avgs

    def calc_packet_loss(self, results: Dict[int, Dict[str, Any]]) -> None:
        # Lets loop through the unique node id and sort them first
        nodes_sorted = sorted(self.nodes.values(), key=lambda x: x.id)
        for node in nodes_sorted:
            if node.id > 1:
                node_packet_loss = node.packet_loss_get_percentage()
                plr_over_sequence = node.delay.get_plr_over_sequence()
                if node_packet_loss is not None:
                    # check if the node id is already in results
                    if node.id not in results:
                        results[node.id] = {}
                    results[node.id]['packet_loss'] = {
                        'percentage': node_packet_loss,
                        'samples': plr_over_sequence
                    }

    def calc_avg_pdr(self, results: Dict[int, Dict[str, Any]]) -> None:
        pdr = {}
        for node in self.nodes.values():
            if node.id > 1:
                pdr_over_sequence = node.delay.get_pdr_over_sequence()
                node_pdr = node.packet_delivery_ratio()
                if node_pdr is not None:
                    pdr[node.id] = {
                        'average': node_pdr,
                        'samples': pdr_over_sequence
                    }

        if not pdr:
            logger.warning("No PDR values found")
            return

        # --- Per-node averages ---
        node_avgs = [pdr[node_id]['average'] for node_id in pdr]

        # --- Per-sample averages across nodes ---
        per_sample_avgs = {}
        # get union of all sample keys
        all_seqs = set().union(
            *[set(pdr[node_id]['samples'].keys()) for node_id in pdr]
        )
        for seq in sorted(all_seqs):
            values = [pdr[node_id]['samples'][seq]
                      for node_id in pdr if seq in pdr[node_id]['samples']]
            if values:
                per_sample_avgs[seq] = float(np.mean(values))

        # --- Network-wide stats ---
        network_avg_pdr = float(np.mean(node_avgs))
        network_std_pdr = float(np.std(node_avgs))

        # Store results
        if 'network' not in results:
            results['network'] = {}
        if 'packet_delivery_ratio' not in results['network']:
            results['network']['packet_delivery_ratio'] = {}

        results['network']['packet_delivery_ratio']['avg'] = network_avg_pdr
        results['network']['packet_delivery_ratio']['std'] = network_std_pdr
        results['network']['packet_delivery_ratio']['samples'] = per_sample_avgs

    def calc_pdr(self, results: Dict[int, Dict[str, Any]]) -> None:
        # Lets loop through the unique node id and sort them first
        nodes_sorted = sorted(self.nodes.values(), key=lambda x: x.id)
        for node in nodes_sorted:
            if node.id > 1:
                pdr_over_sequence = node.delay.get_pdr_over_sequence()
                node_pdr = node.packet_delivery_ratio()
                if node_pdr is not None:
                    # check if the node id is already in results
                    if node.id not in results:
                        results[node.id] = {}
                    results[node.id]['packet_delivery_ratio'] = {
                        'average': node_pdr,
                        'samples': pdr_over_sequence
                    }

    def calc_avg_jitter(self, results: Dict[int, Dict[str, Any]]) -> None:
        jitter = {}
        for node in self.nodes.values():
            if node.id > 1:
                jitter_over_sequence = node.delay.get_jitter_instantaneous_over_sequence()
                node_jitter = node.delay.get_jitter_average()
                if node_jitter is not None:
                    jitter[node.id] = {
                        'average_us': node_jitter['microseconds'],
                        'samples': jitter_over_sequence
                    }

        if not jitter:
            logger.warning("No jitter values found")
            return {}

        # --- Per-node averages ---
        node_avgs = [jitter[node_id]['average_us'] for node_id in jitter]

        # --- Per-sample averages across nodes ---
        per_sample_avgs = {}
        # get union of all sample keys
        all_seqs = set().union(
            *[jitter[node_id]['samples'].keys() for node_id in jitter]
        )
        for seq in sorted(all_seqs):
            values = [jitter[node_id]['samples'][seq]
                      for node_id in jitter if seq in jitter[node_id]['samples']]
            if values:
                per_sample_avgs[seq] = float(np.mean(values))

        # --- Network-wide stats ---
        network_avg_jitter = float(np.mean(node_avgs))
        network_std_jitter = float(np.std(node_avgs))

        # Store results
        if 'network' not in results:
            results['network'] = {}
        if 'jitter' not in results['network']:
            results['network']['jitter'] = {}

        results['network']['jitter']['avg'] = network_avg_jitter
        results['network']['jitter']['std'] = network_std_jitter
        results['network']['jitter']['samples'] = per_sample_avgs

    def calc_jitter(self, results: Dict[int, Dict[str, Any]]) -> None:
        nodes_sorted = sorted(self.nodes.values(), key=lambda x: x.id)
        for node in nodes_sorted:
            if node.id > 1:
                node_jitter = node.delay.get_jitter_average()
                jitter_samples = node.delay.get_jitter_instantaneous_over_sequence()
                if node_jitter is not None:
                    # check if the node id is already in results
                    if node.id not in results:
                        results[node.id] = {}
                    results[node.id]['jitter'] = {
                        'average_us': node_jitter['microseconds'],
                        'samples': jitter_samples
                    }

    def calc_avg_duty_cycle(self, results: Dict[int, Dict[str, Any]]) -> None:
        duty_cycle = {}
        for node in self.nodes.values():
            if node.id > 1:
                duty_cycle_samples = node.power_trace.get_samples()
                node_avg_duty_cycle = node.power_trace.get_average()
                if node_avg_duty_cycle is not None:
                    duty_cycle[node.id] = {
                        'average_rdc': node_avg_duty_cycle['average_rdc'],
                        'samples': {seq: sample.rdc for seq, sample in duty_cycle_samples.items() if sample.rdc is not None}
                    }

        if not duty_cycle:
            logger.warning("No duty cycle values found")
            return {}

        # --- Per-node averages ---
        node_avgs = [duty_cycle[node_id]['average_rdc']
                     for node_id in duty_cycle]

        # --- Per-sample averages across nodes ---
        per_sample_avgs = {}
        # get union of all sample keys
        all_seqs = set().union(
            *[duty_cycle[node_id]['samples'].keys() for node_id in duty_cycle])
        for seq in sorted(all_seqs):
            values = [duty_cycle[node_id]['samples'][seq]
                      for node_id in duty_cycle if seq in duty_cycle[node_id]['samples']]
            if values:  # avoid empty
                per_sample_avgs[seq] = float(np.mean(values))

        # --- Network-wide stats ---
        network_avg_duty_cycle = float(np.mean(node_avgs))
        network_std_duty_cycle = float(np.std(node_avgs))

        # Store results
        if 'network' not in results:
            results['network'] = {}
        if 'rdc' not in results['network']:
            results['network']['rdc'] = {}

        results['network']['rdc']['avg'] = network_avg_duty_cycle
        results['network']['rdc']['std'] = network_std_duty_cycle
        results['network']['rdc']['per_sample_avg'] = per_sample_avgs

    def calc_duty_cycle(self, results: Dict[int, Dict[str, Any]]) -> None:
        nodes_sorted = sorted(self.nodes.values(), key=lambda x: x.id)
        for node in nodes_sorted:
            if node.id > 1:
                rdc = node.power_trace.get_samples()
                node_avg_rdc = node.power_trace.get_average()
                if node_avg_rdc is not None:
                    # check if the node id is already in results
                    if node.id not in results:
                        results[node.id] = {}
                    results[node.id]['rdc'] = {
                        'joined_time': node.joined_time,
                        'average_rdc': node_avg_rdc['average_rdc'],
                    }
                    for seq, sample in rdc.items():
                        if sample.rdc is not None:
                            results[node.id]['rdc'].setdefault('samples', {})[seq] = {
                                'rdc': sample.rdc,
                                'time': sample.time
                            }

    def calc_rl_asl(self, results: Dict[int, Dict[str, Any]]) -> None:
        nodes_sorted = sorted(self.nodes.values(), key=lambda x: x.id)
        for node in nodes_sorted:
            if node.id > 1:
                rl_asl_samples = node.rl_asl_trace.get_samples()
                if rl_asl_samples:
                    # check if the node id is already in results
                    if node.id not in results:
                        results[node.id] = {}
                    results[node.id]['rl_asl'] = {
                        'samples': {asn: {
                            'action': sample.action,
                            'success': sample.success,
                            'time': sample.time
                        } for asn, sample in rl_asl_samples.items()}
                    }

    def calc_episode_monitoring(self, results: Dict[int, Dict[str, Any]]) -> None:
        nodes_sorted = sorted(self.nodes.values(), key=lambda x: x.id)
        for node in nodes_sorted:
            if node.id > 1:
                episode_samples = node.episode_monitoring.get_samples()
                if episode_samples:
                    # check if the node id is already in results
                    if node.id not in results:
                        results[node.id] = {}
                    results[node.id]['episode_monitoring'] = {
                        'samples': {count: {
                            'episode_reward': sample.episode_reward,
                            'epsilon': sample.epsilon,
                            'steps': sample.steps,
                            'avg_reward': sample.avg_reward,
                            'time': sample.time
                        } for count, sample in episode_samples.items()}
                    }

    def calc_rl_asl_q_table(self, results: Dict[int, Dict[str, Any]], out_dir: Path = Path("pretrained_q")) -> None:
        """
            Collect Q-tables per node and also export them to C header files.
            """
        out_dir.mkdir(parents=True, exist_ok=True)
        nodes_sorted = sorted(self.nodes.values(), key=lambda x: x.id)

        for node in nodes_sorted:
            if node.id > 1 and node.rl_asl_q_table.is_initialized() and node.rl_asl_q_table.has_non_zero_q_values():
                if node.id not in results:
                    results[node.id] = {}

                q_table = node.rl_asl_q_table.q_table.tolist()
                num_states = node.rl_asl_q_table.num_states
                num_actions = node.rl_asl_q_table.num_actions
                episode_count = node.rl_asl_q_table_get_episode_count()
                rolling_avg = node.rl_asl_q_table_get_rolling_avg()

                # Lets create a dedicated json for the Q-table
                json_q_table = {
                    'num_states': num_states,
                    'num_actions': num_actions,
                    'episode_count': episode_count,
                    'rolling_avg': rolling_avg,
                    'q_table': q_table
                }

                # --- Write JSON file per node (easy to parse later) ---
                json_path = out_dir / f"rl-asl-pretrained-q-node{node.id}.json"
                json_path.write_text(json.dumps(json_q_table, indent=4))

                # --- Build C header text ---
                lines = []
                for row in q_table:
                    row_str = ", ".join(f"{v:.6f}f" for v in row)
                    lines.append(f"    {{{row_str}}},")
                c_array = (
                    f"#ifndef RL_ASL_PRETRAINED_Q_NODE{node.id}_H\n"
                    f"#define RL_ASL_PRETRAINED_Q_NODE{node.id}_H\n\n"
                    f"static const float rl_asl_pretrained_q[{num_states}][{num_actions}] = {{\n"
                    + "\n".join(lines)
                    + "\n};\n\n"
                    f"#endif /* RL_ASL_PRETRAINED_Q_NODE{node.id}_H */\n"
                )

                # --- Write to file ---
                header_path = out_dir / \
                    f"rl-asl-pretrained-q-node{node.id}.h"
                header_path.write_text(c_array)

    def calc_federated_learning(self, results: Dict[int, Dict[str, Any]], out_dir: Path = Path("federated_q")) -> None:
        """
            Perform Federated Learning aggregation of Q-tables if enabled.
            """
        # Collect all Q-tables from nodes
        q_tables = []
        for node in self.nodes.values():
            if node.id > 1 and node.rl_asl_q_table.is_initialized() and node.rl_asl_q_table.has_non_zero_q_values():
                q_tables.append(node.rl_asl_q_table.q_table)

        if not q_tables:
            logger.warning(
                "No Q-tables found for Federated Learning aggregation")
            return

        # Perform simple averaging of Q-tables
        aggregated_q_table = np.mean(q_tables, axis=0)

        episode_count = max([node.rl_asl_q_table_get_episode_count() for node in self.nodes.values(
        ) if node.id > 1 and node.rl_asl_q_table.is_initialized() and node.rl_asl_q_table.has_non_zero_q_values()] or [0])

        # Lets create a dedicated json for the Q-table FL
        json_q_table = {
            'num_states': aggregated_q_table.shape[0],
            'num_actions': aggregated_q_table.shape[1],
            'episode_count': episode_count,
            'q_table': aggregated_q_table.tolist()
        }
        json_path = out_dir / f"rl-asl-federated-q.json"
        json_path.write_text(json.dumps(json_q_table, indent=4))

        # --- Build C header text ---
        num_states, num_actions = aggregated_q_table.shape
        lines = []
        for row in aggregated_q_table:
            row_str = ", ".join(f"{v:.6f}f" for v in row)
            lines.append(f"    {{{row_str}}},")
        c_array = (
            f"#ifndef RL_ASL_FEDERATED_Q_H\n"
            f"#define RL_ASL_FEDERATED_Q_H\n\n"
            f"static const float rl_asl_federated_q[{num_states}][{num_actions}] = {{\n"
            + "\n".join(lines)
            + "\n};\n\n"
            f"#endif /* RL_ASL_FEDERATED_Q_H */\n"
        )
        # --- Write to file ---
        out_dir.mkdir(parents=True, exist_ok=True)
        header_path = out_dir / "rl-asl-federated-q.h"
        header_path.write_text(c_array)
