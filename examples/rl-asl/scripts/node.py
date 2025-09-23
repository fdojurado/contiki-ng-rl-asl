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

from delay import DelaySamples
from power_trace import PowerTraceSamples, PowerTrace
from rl_asl_trace import RLASLTraceSamples, RLASLTrace
from episode_monitoring import EpisodeSamples, EpisodeSample
from rl_asl_q_table import RLASLQTable


from typing import Any, Optional, Dict

logger = logging.getLogger(f'main.{__name__}')


class Node():
    def __init__(
        self,
        id,
        sid: str | None
    ) -> None:
        assert isinstance(id, int), "node ID must be a integer"
        assert id >= 0, "node ID must be positive"
        self.id = id
        self.joined_time = None
        if sid is None:
            self.sid = str(id) + ".0"
        else:
            self.sid = sid
        self.power_trace = PowerTraceSamples(self)
        self.rl_asl_trace = RLASLTraceSamples(self)
        self.delay = DelaySamples(self)
        self.episode_monitoring = EpisodeSamples(self)
        self.last_power_seq = 0
        self.rl_asl_q_table = RLASLQTable()

    # ---------------------------------------------------------------------------
    def update_last_power_seq(self, seq: int):
        self.last_power_seq = seq

    def get_last_power_seq(self) -> int:
        return self.last_power_seq
    # ---------------------------------------------------------------------------
    # setter for joined

    def joined_set(self, time):
        self.joined_time = time

    # ---------------------------------------------------------------------------

    def power_trace_add(self, seq, data: Dict[str, Any], time: Optional[int] = None) -> PowerTrace:
        power = self.power_trace.add_sample(
            seq=seq, data=data, time=time)
        return power

    def power_trace_print(self):
        self.power_trace.print()

    def power_trace_get_average(self):
        return self.power_trace.get_average(self.joined_time)

    def power_trace_get_last(self):
        return self.power_trace.get_sample_last()

    def power_trace_clear(self):
        self.power_trace.clear()

    # ---------------------------------------------------------------------------

    def rl_asl_trace_add(self, data: dict, time=None) -> RLASLTrace:
        trace = self.rl_asl_trace.add_sample(
            data=data, time=time)
        return trace

    # ---------------------------------------------------------------------------
    def episode_monitoring_add(self, data: dict, time=None) -> EpisodeSample:
        sample = self.episode_monitoring.add_sample(
            data=data, time=time)
        return sample
    # ---------------------------------------------------------------------------

    def power_testbed_add(self, seq, power, time=None):
        power = self.power_testbed.add_sample(
            seq=seq, power=power, time=time)
        return power

    def power_testbed_print(self):
        self.power_testbed.print()

    def power_testbed_get_average(self):
        return self.power_testbed.get_average(self.joined_time)

    def power_testbed_get_last(self):
        return self.power_testbed.get_sample_last()

    def power_testbed_clear(self):
        self.power_testbed.clear()

    # ---------------------------------------------------------------------------

    def delay_add(self, seq, delay, time_at_tx: Optional[int] = None):
        delay = self.delay.add_sample(
            seq=seq, delay=delay, time_at_tx=time_at_tx)
        return delay

    def delay_update_time_at_rx(self, seq, time_at_rx: int):
        self.delay.update_time_at_rx(seq=seq, time_at_rx=time_at_rx)

    def delay_print(self):
        self.delay.print()

    def delay_get_average(self):
        return self.delay.get_average()

    def delay_clear(self):
        self.delay.clear()

    # ---------------------------------------------------------------------------
    def delay_get_jitter_average(self):
        return self.delay.get_jitter_average()

    # ---------------------------------------------------------------------------

    def packet_loss_get_percentage(self):
        # These are the samples in the delay samples that have not been received
        # PL = 1 - number of samples received / number of samples
        num_samples = self.delay.size()
        num_samples_received = 0
        for delay in self.delay.samples.values():
            if delay.time_at_rx is not None:
                num_samples_received += 1
        if num_samples == 0:
            return None
        plr = 1 - num_samples_received / num_samples
        return plr

    def packet_delivery_ratio(self):
        num_samples = self.delay.size()
        num_samples_received = 0
        for delay in self.delay.samples.values():
            if delay.time_at_rx is not None:
                num_samples_received += 1
        if num_samples == 0:
            return None
        pdr = num_samples_received / num_samples
        return pdr

    # ---------------------------------------------------------------------------

    def rl_asl_q_table_initialize(self, num_states: int, num_actions: int) -> None:
        self.rl_asl_q_table.initialize(
            num_states=num_states, num_actions=num_actions)

    def rl_asl_q_table_set_q_value(self, state: int, action: int, value: float) -> None:
        self.rl_asl_q_table.set_q_value(
            state=state, action=action, value=value)

    def rl_asl_q_table_get_q_value(self, state: int, action: int) -> float:
        return self.rl_asl_q_table.get_q_value(state=state, action=action)
    
    def rl_asl_q_table_set_episode_count(self, episode: int) -> None:
        self.rl_asl_q_table.episode_count = episode
        
    def rl_asl_q_table_get_episode_count(self) -> int:
        return self.rl_asl_q_table.episode_count
    
    def rl_asl_q_table_set_rolling_avg(self, rolling_avg: float) -> None:
        self.rl_asl_q_table.rolling_avg = rolling_avg
        
    def rl_asl_q_table_get_rolling_avg(self) -> float:
        return self.rl_asl_q_table.rolling_avg

    # ---------------------------------------------------------------------------

    def clear(self):
        self.delay.clear()
        self.power_trace.clear()
        self.rl_asl_trace.clear()

    def performance_metrics_clear(self):
        self.delay_clear()
        self.power_trace_clear()
