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

import logging

import sys
from typing import Dict, Any
from rich.table import Table

# from sdwsn_controller.common import common


logger = logging.getLogger(f'main.{__name__}')

VOLTAGE = 3  # V
CURRENT = {
    "CPU": 1.8,  # mA
    "LPM": 0.545,  # mA
    "DEEP_LPM": 0.545,  # mA
    "TX": 17.4,  # mA
    "RX": 20,  # mA
}
# VOLTAGE = 3.3  # V
# CURRENT = {
#     "CPU": 14.0,       # mA  (STM32F103 running; IoT-LAB M3 doc)
#     "LPM": 0.014,      # mA  (≈14 µA, Stop mode typical @ 3.3 V)
#     "DEEP_LPM": 0.002,  # mA  (≈2 µA, Standby mode typical @ 3.3 V)
#     "TX": 11.6,        # mA  (AT86RF231 @ +3 dBm)
#     "RX": 12.3,        # mA  (AT86RF231 RX_ON)
# }


class PowerTrace():
    def __init__(
        self,
        seq,
        cpu=None,
        lpm=None,
        deep_lpm=None,
        tx=None,
        rx=None,
        uc_rx=None,
        uc_idle_rx=None,
        uc_idle_ratio=None,
        uc_ratio=None,
        radio_total=None,
        total_time_secs=None,
        total_time_ticks=None,
        power=None,
        rx_power=None,
        rx_uc_power=None,
        energy=None,
        rdc=None,
        cpu_activity=None,
        radio_activity=None,
        time=None
    ) -> None:
        assert isinstance(seq, int)
        if cpu is not None:
            assert isinstance(cpu, int)
        if lpm is not None:
            assert isinstance(lpm, int)
        if deep_lpm is not None:
            assert isinstance(deep_lpm, int)
        if tx is not None:
            assert isinstance(tx, int)
        if rx is not None:
            assert isinstance(rx, int)
        if uc_rx is not None:
            assert isinstance(uc_rx, int)
        if uc_idle_rx is not None:
            assert isinstance(uc_idle_rx, int)
        if uc_idle_ratio is not None:
            assert isinstance(uc_idle_ratio, float)
        if uc_ratio is not None:
            assert isinstance(uc_ratio, float)
        if radio_total is not None:
            assert isinstance(radio_total, int)
        if cpu_activity is not None:
            assert isinstance(cpu_activity, float)
        if radio_activity is not None:
            assert isinstance(radio_activity, float)
        self.seq = seq
        self.cpu = cpu
        self.lpm = lpm
        self.deep_lpm = deep_lpm
        self.tx = tx
        self.rx = rx
        self.uc_rx = uc_rx
        self.uc_idle_rx = uc_idle_rx
        self.uc_idle_ratio = uc_idle_ratio
        self.uc_ratio = uc_ratio
        self.radio_total = radio_total
        self.total_time_secs = total_time_secs
        self.total_time_ticks = total_time_ticks
        self.power = power
        self.rx_power = rx_power
        self.rx_uc_power = rx_uc_power
        self.energy = energy
        self.rdc = rdc
        self.cpu_activity = cpu_activity
        self.radio_activity = radio_activity
        self.time = time

    # __str__
    def __str__(self):
        return (f"PowerTrace(seq={self.seq}, cpu={self.cpu}, lpm={self.lpm}, "
                f"deep_lpm={self.deep_lpm}, tx={self.tx}, rx={self.rx}, "
                f"uc_rx={self.uc_rx}, uc_idle_rx={self.uc_idle_rx}, uc_idle_ratio={self.uc_idle_ratio}, uc_ratio={self.uc_ratio}, "
                f"radio_total={self.radio_total}, total_time_secs={self.total_time_secs}, "
                f"total_time_ticks={self.total_time_ticks}, power={self.power}, energy={self.energy}, RDC={self.rdc}, cpu_activity={self.cpu_activity}, radio_activity={self.radio_activity}, time={self.time})")


class PowerTraceSamples():
    def __init__(
        self,
        node
    ) -> None:
        self.node = node
        self.callback = None
        self.clear()

    def clear(self):
        self.samples = {}
        self.last_seq = 0

    def size(self):
        return len(self.samples)

    def register_callback(self, callback):
        self.callback = callback

    def get_sample(self, seq):
        return self.samples.get(seq)

    def get_samples(self) -> dict[int, PowerTrace]:
        return self.samples

    def get_average(self, joined_time=None) -> Dict[str, Any]:
        if not self.samples:
            return None
        # Calculate the average power consumption of samples with time greater than joined_time
        total_power = 0
        total_rx_power = 0
        total_rx_uc_power = 0
        count = 0
        rx_power_count = 0
        rx_uc_power_count = 0
        for sample in self.samples.values():
            if sample.power is not None:
                if joined_time is None or sample.time >= joined_time:
                    total_power += sample.power
                    count += 1
            if sample.rx_power is not None:
                if joined_time is None or sample.time >= joined_time:
                    total_rx_power += sample.rx_power
                    rx_power_count += 1
            if sample.rx_uc_power is not None:
                if joined_time is None or sample.time >= joined_time:
                    total_rx_uc_power += sample.rx_uc_power
                    rx_uc_power_count += 1
        if count == 0:
            return None
        if rx_uc_power_count == 0:
            return None
        if rx_power_count == 0:
            return None
        average_power = total_power / count
        average_rx_power = total_rx_power / rx_power_count
        average_rx_uc_power = total_rx_uc_power / rx_uc_power_count
        # Calculate the average RDC of samples with time greater than joined_time
        total_rdc = 0
        rdc_count = 0
        for sample in self.samples.values():
            if sample.rdc is not None:
                if joined_time is None or sample.time >= joined_time:
                    total_rdc += sample.rdc
                    rdc_count += 1
        average_rdc = total_rdc / rdc_count if rdc_count > 0 else 0

        return {
            'average_mW': average_power,
            'average_rx_mW': average_rx_power,
            'average_rx_uc_mW': average_rx_uc_power,
            'samples_mW': {seq: sample.power for seq, sample in self.samples.items() if sample.power is not None},
            'average_rdc': average_rdc,
        }

    def get_cpu_activity_average(self, joined_time=None) -> float:
        if not self.samples:
            return None
        # Calculate the average cpu activity of samples with time greater than joined_time
        total_cpu_activity = 0
        count = 0
        for sample in self.samples.values():
            if sample.cpu_activity is not None:
                if joined_time is None or sample.time >= joined_time:
                    total_cpu_activity += sample.cpu_activity
                    count += 1
        if count == 0:
            return None
        average_cpu_activity = total_cpu_activity / count
        return average_cpu_activity

    def get_radio_activity_average(self, joined_time=None) -> float:
        if not self.samples:
            return None
        # Calculate the average radio activity of samples with time greater than joined_time
        total_radio_activity = 0
        count = 0
        for sample in self.samples.values():
            if sample.radio_activity is not None:
                if joined_time is None or sample.time >= joined_time:
                    total_radio_activity += sample.radio_activity
                    count += 1
        if count == 0:
            return None
        average_radio_activity = total_radio_activity / count
        return average_radio_activity

    def get_sample_last(self):
        if self.samples:
            return self.get_sample(self.last_seq)
        return None

    # Lets create a method that add sample of any type eg. CPU, LPM, etc.
    def add_sample(self, seq: int, data: Dict[str, int], time=None) -> PowerTrace:
        power_trace = self.get_sample(seq)
        data_type = data.get("type")
        if data_type not in ["cpu", "lpm", "deep_lpm", "tx", "rx", "uc_rx", "uc_idle_rx", "uc_idle_ratio", "uc_ratio", "radio_total", "power", "total_time_secs", "total_time_ticks"]:
            logger.warning(
                f'Node {self.node.id}: unknown power data type {data_type}')
            return
        # Get existing sample or create a new one
        if not power_trace:
            power_trace = PowerTrace(seq=seq, time=time)
        # Update the sample with the new data
        setattr(power_trace, data_type, data.get("value"))
        # If we have all the data, we can calculate the total power
        if all([getattr(power_trace, attr) is not None for attr in ["cpu", "lpm", "deep_lpm", "tx", "rx", "uc_rx", "radio_total", "total_time_secs", "total_time_ticks"]]):
            # Calculate energy
            cpu_time = int(getattr(power_trace, "cpu", 0))
            lpm_time = int(getattr(power_trace, "lpm", 0))
            deep_lpm_time = int(getattr(power_trace, "deep_lpm", 0))
            tx_time = int(getattr(power_trace, "tx", 0))
            rx_time = int(getattr(power_trace, "rx", 0))
            uc_rx_time = int(getattr(power_trace, "uc_rx", 0))
            radio_total_time = int(getattr(power_trace, "radio_total", 0))
            total_time_ticks = int(getattr(power_trace, "total_time_ticks", 1))
            total_time_secs = int(getattr(power_trace, "total_time_secs", 1))
            total_energy = (cpu_time * CURRENT["CPU"] +
                            lpm_time * CURRENT["LPM"] +
                            deep_lpm_time * CURRENT["DEEP_LPM"] +
                            tx_time * CURRENT["TX"] +
                            rx_time * CURRENT["RX"])  # tick × mA
            rx_energy = rx_time * CURRENT["RX"]
            rx_uc_energy = uc_rx_time * CURRENT["RX"]
            power = total_energy * VOLTAGE / total_time_ticks  # mW
            rx_power = rx_energy * VOLTAGE / total_time_ticks  # mW
            rx_uc_power = rx_uc_energy * VOLTAGE / total_time_ticks  # mW
            energy_mj = power * total_time_secs

            cpu_activity = cpu_time / total_time_ticks
            power_trace.cpu_activity = cpu_activity

            radio_activity = (tx_time + rx_time) / total_time_ticks
            power_trace.radio_activity = radio_activity

            power_trace.power = power
            power_trace.rx_power = rx_power
            power_trace.rx_uc_power = rx_uc_power

            power_trace.rdc = radio_total_time / \
                total_time_ticks if total_time_ticks > 0 else 0
            # we accumulate the energy
            # Lets check if the seq-1 exist in the samples
            if seq-1 in self.samples:
                power_trace.energy = self.samples[seq - 1].energy + energy_mj
            else:
                power_trace.energy = energy_mj

        self.samples[seq] = power_trace
        if seq > self.last_seq:
            self.last_seq = seq
        return power_trace

    def print(self):
        table = Table(
            title=f"Power samples (node {self.node.id})")

        table.add_column("Node", justify="center",
                         style="cyan", no_wrap=True)
        # table.add_column("Cycle sequence", justify="center", style="magenta")
        table.add_column("Power", justify="center", style="green")
        for key in self.samples:
            # if cycle_seq is not None:
            #     if key[0] == cycle_seq:
            #         power = self.samples.get(key)
            #         table.add_row(str(self.node.id), str(power.cycle_seq),
            #                       str(power.seq), str(power.power))
            # else:
            power = self.samples.get(key)
            table.add_row(self.node.sid,
                          str(power.power))

        # logger.info(f"Power samples\n{common.log_table(table)}")
