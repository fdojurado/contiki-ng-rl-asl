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

nodes = {
    "sink": {
        "iotlab_id": "2",   # IoT-LAB platform ID
        "node_id": 1,         # Logical Contiki Node ID
        "role": "SINK"
    },
    "relay1": {
        "iotlab_id": "48",
        "node_id": 2,
        "role": "RELAY"
    },
    "relay2": {
        "iotlab_id": "40",
        "node_id": 3,
        "role": "RELAY"
    },
    "relay3": {
        "iotlab_id": "8",
        "node_id": 4,
        "role": "RELAY"
    },
    "leaf1": {
        "iotlab_id": "54",
        "node_id": 5,
        "role": "LEAF"
    },
    "leaf2": {
        "iotlab_id": "56",
        "node_id": 6,
        "role": "LEAF"
    },
    "leaf3": {
        "iotlab_id": "58",
        "node_id": 7,
        "role": "LEAF"
    },
    "leaf4": {
        "iotlab_id": "35",
        "node_id": 8,
        "role": "LEAF"
    },
    "leaf5": {
        "iotlab_id": "37",
        "node_id": 9,
        "role": "LEAF"
    },
    "leaf6": {
        "iotlab_id": "39",
        "node_id": 10,
        "role": "LEAF"
    },
    "leaf7": {
        "iotlab_id": "15",
        "node_id": 11,
        "role": "LEAF"
    },
    "leaf8": {
        "iotlab_id": "17",
        "node_id": 12,
        "role": "LEAF"
    },
    "leaf13": {
        "iotlab_id": "27",
        "node_id": 13,
        "role": "LEAF"
    },
    # you can add more nodes (relay, leaf2, etc.)
}

# Function that looks up the node configuration by its iotlab_id


def get_node_config(iotlab_id):
    for node_name, config in nodes.items():
        if config["iotlab_id"] == iotlab_id:
            return config
    return None
