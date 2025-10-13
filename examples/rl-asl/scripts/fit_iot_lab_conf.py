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

topology_a = {
    "sink": {
        "iotlab_id": "2",   # IoT-LAB platform ID
        "node_id": 1,         # Logical Contiki Node ID
    },
    "relay1": {
        "iotlab_id": "30",
        "node_id": 2,

    },
    "leaf1": {
        "iotlab_id": "54",
        "node_id": 3,
    },
    "leaf2": {
        "iotlab_id": "56",
        "node_id": 4,
    },
    "leaf3": {
        "iotlab_id": "58",
        "node_id": 5,
    }
}

topology_b = {
    "sink": {
        "iotlab_id": "2",   # IoT-LAB platform ID
        "node_id": 1,         # Logical Contiki Node ID
    },
    "relay1": {
        "iotlab_id": "20",
        "node_id": 2,

    },
    "relay2": {
        "iotlab_id": "22",
        "node_id": 3,
    },
    "relay3": {
        "iotlab_id": "6",
        "node_id": 4,
    },
    "relay4": {
        "iotlab_id": "30",
        "node_id": 5,
    },
    "relay5": {
        "iotlab_id": "32",
        "node_id": 6,
    },
    "relay6": {
        "iotlab_id": "36",
        "node_id": 7,
    },
    "relay7": {
        "iotlab_id": "38",
        "node_id": 8,
    },
    "relay8": {
        "iotlab_id": "24",
        "node_id": 9,
    },
    "relay9": {
        "iotlab_id": "10",
        "node_id": 10,
    },
    "leaf1": {
        "iotlab_id": "54",
        "node_id": 11,
    },
    "leaf2": {
        "iotlab_id": "56",
        "node_id": 12,
    },
    "leaf3": {
        "iotlab_id": "53",
        "node_id": 13,
    },
    "leaf4": {
        "iotlab_id": "55",
        "node_id": 14,
    },
    "leaf5": {
        "iotlab_id": "29",
        "node_id": 15,
    },
    "leaf6": {
        "iotlab_id": "31",
        "node_id": 16,
    },
    "leaf7": {
        "iotlab_id": "35",
        "node_id": 17,
    },
    "leaf8": {
        "iotlab_id": "37",
        "node_id": 18,
    },
    "leaf9": {
        "iotlab_id": "23",
        "node_id": 19,
    },
    "leaf10": {
        "iotlab_id": "25",
        "node_id": 20,
    },
    "leaf11": {
        "iotlab_id": "13",
        "node_id": 21,
    },
    "leaf12": {
        "iotlab_id": "15",
        "node_id": 22,
    }
}


def get_node_config(iotlab_id):
    for node_name, config in topology_a.items():
        if config["iotlab_id"] == iotlab_id:
            return config
    return None
