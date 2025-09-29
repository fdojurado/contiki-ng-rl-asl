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
# Function that looks up the node configuration by its iotlab_id


def get_node_config(iotlab_id):
    for node_name, config in topology_a.items():
        if config["iotlab_id"] == iotlab_id:
            return config
    return None
