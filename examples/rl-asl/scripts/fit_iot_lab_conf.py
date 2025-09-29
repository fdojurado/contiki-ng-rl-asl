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

short_topology_orchestra_scenario_1 = {
    "sink": {
        "iotlab_id": "2",   # IoT-LAB platform ID
        "node_id": 1,         # Logical Contiki Node ID
        "role": "SINK",
        "traffic_pattern": None,
        "rl_asl_enabled": False,
        "train_mode": False,
        "model": None,
        "topology_a": None,
    },
    "relay1": {
        "iotlab_id": "30",
        "node_id": 2,
        "role": "RELAY",
        "traffic_pattern": None,
        "rl_asl_enabled": False,
        "train_mode": False,
        "model": None,
        "topology_a": True,
    },
    "leaf1": {
        "iotlab_id": "54",
        "node_id": 3,
        "role": "LEAF",
        "traffic_pattern": 1,
        "rl_asl_enabled": False,
        "train_mode": False,
        "model": None,
        "topology_a": True,
    },
    "leaf2": {
        "iotlab_id": "56",
        "node_id": 4,
        "role": "LEAF",
        "traffic_pattern": 1,
        "rl_asl_enabled": False,
        "train_mode": False,
        "model": None,
        "topology_a": True,
    },
    "leaf3": {
        "iotlab_id": "58",
        "node_id": 5,
        "role": "LEAF",
        "traffic_pattern": 1,
        "rl_asl_enabled": False,
        "train_mode": False,
        "model": None,
        "topology_a": True,
    }
}
# Function that looks up the node configuration by its iotlab_id


def get_node_config(iotlab_id):
    for node_name, config in short_topology_orchestra_scenario_1.items():
        if config["iotlab_id"] == iotlab_id:
            return config
    return None
