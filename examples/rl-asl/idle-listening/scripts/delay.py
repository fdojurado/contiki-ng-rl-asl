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
import numpy as np
from typing import Dict, Optional

from rich.table import Table

# from sdwsn_controller.common import common

from typing import Optional


logger = logging.getLogger(f'main.{__name__}')


class Delay():
    def __init__(
        self,
        # cycle_seq,
        seq,
        delay,
        timeslot,
        time_at_tx: Optional[int] = None,
        time_at_rx: Optional[int] = None
    ) -> None:
        # assert isinstance(cycle_seq, int)
        assert isinstance(seq, int)
        assert isinstance(delay, int)
        # self.cycle_seq = cycle_seq
        self.seq = seq
        self.delay = delay
        self.timeslot = timeslot
        self.time_at_tx = time_at_tx
        self.time_at_rx = time_at_rx


class DelaySamples():
    def __init__(
        self,
        node
    ) -> None:
        self.node = node
        self.callback = None
        self.samples = {}
        self.clear()

    def clear(self):
        # Only delete samples where the time at rx is not None
        self.samples = {k: v for k, v in self.samples.items()
                        if v.time_at_rx is None}

    def size(self):
        return len(self.samples)

    def register_callback(self, callback):
        self.callback = callback

    def get_sample(self, seq):
        return self.samples.get(seq)

    def get_samples(self) -> Dict[int, Delay]:
        # Samples are raw values in microseconds
        return self.samples

    def get_average(self) -> Dict[str, float]:
        # Dict keys are milliseconds and microseconds
        if len(self.samples) == 0:
            return None
        # We want to omit samples where time_at_rx is None
        valid_samples = [
            sample for sample in self.samples.values() if sample.time_at_rx is not None]
        if not valid_samples:
            return None
        # Calculate the average delay in microseconds
        avg_delay = sum(
            sample.delay for sample in valid_samples) / len(valid_samples)
        # Calculate the average delay in milliseconds
        avg_delay_ms = avg_delay / 1000
        return {
            'milliseconds': avg_delay_ms,
            'microseconds': avg_delay
        }

    def get_jitter_average(self) -> Optional[Dict[str, float]]:
        if len(self.samples) < 2:
            return None

        # We want to omit samples where time_at_rx is None
        valid_samples = [
            sample for sample in self.samples.values() if sample.time_at_rx is not None]
        if len(valid_samples) < 2:
            return None

        # Sort samples by sequence number to preserve arrival order
        valid_samples.sort(key=lambda x: x.seq)

        # Calculate absolute differences between successive delays
        differences = [
            abs(valid_samples[i].delay - valid_samples[i - 1].delay)
            for i in range(1, len(valid_samples))
        ]

        if not differences:
            return None

        avg_jitter = sum(differences) / len(differences)  # in microseconds
        return {
            'milliseconds': avg_jitter / 1000,
            'microseconds': avg_jitter
        }

    def get_jitter_instantaneous_over_sequence(self) -> Optional[Dict[int, float]]:
        if not self.samples:
            return None

        # Sort samples by seq to ensure correct order
        sorted_seqs = sorted(self.samples.keys())
        jitter_per_seq = {}

        for i in range(1, len(sorted_seqs)):
            seq = sorted_seqs[i]
            prev_seq = sorted_seqs[i - 1]
            curr_sample = self.samples[seq]
            prev_sample = self.samples[prev_seq]

            if curr_sample.time_at_rx is not None and prev_sample.time_at_rx is not None:
                jitter = abs(curr_sample.delay - prev_sample.delay)
                jitter_per_seq[seq] = jitter

        return jitter_per_seq

    def add_sample(self, seq, delay, timeslot, time_at_tx: Optional[int] = None) -> Delay:
        sample = self.get_sample(seq)
        if sample:
            if time_at_tx is not None:
                sample.time_at_tx = time_at_tx
            return
        logger.debug(
            f'Node {self.node.id}: add delay {delay}, seq {seq}')
        delay_sample = Delay(seq=seq, delay=delay,
                             timeslot=timeslot, time_at_tx=time_at_tx)
        self.samples.update({seq: delay_sample})
        # Fire callback
        if self.callback:
            self.callback(id=self.node.id, seq=seq, delay=delay)
        return delay_sample

    def update_time_at_rx(self, seq, time_at_rx):
        sample = self.get_sample(seq)
        if sample:
            sample.time_at_rx = time_at_rx
            # Calculate the delay
            delay = sample.time_at_rx - sample.time_at_tx
            sample.delay = delay
        else:
            logger.warning(
                f'Node {self.node.id}: delay sample not found, seq {seq}')

    def get_plr_over_sequence(self) -> Optional[Dict[int, float]]:
        """
        Returns a dictionary mapping sequence numbers to the
        cumulative packet loss rate (PLR) up to that sequence.

        Example:
            {0: 0.0, 1: 0.5, 2: 0.33, 3: 0.25, ...}
        """
        if not self.samples:
            return None

        # Sort samples by seq to ensure correct order
        sorted_seqs = sorted(self.samples.keys())
        plr_per_seq = {}

        received_count = 0
        total_count = 0

        for seq in sorted_seqs:
            total_count += 1
            sample = self.samples[seq]
            if sample.time_at_rx is not None:
                received_count += 1

            plr = 1 - (received_count / total_count)
            plr_per_seq[seq] = plr

        return plr_per_seq

    def get_pdr_over_sequence(self) -> Optional[Dict[int, float]]:
        """
        Returns a dictionary mapping sequence numbers to the
        packet delivery ratio (PDR) up to that sequence.
        """
        if not self.samples:
            return None

        # Sort samples by seq to ensure correct order
        sorted_seqs = sorted(self.samples.keys())
        pdr_per_seq = {}

        received_count = 0
        total_count = 0

        for seq in sorted_seqs:
            sample = self.samples[seq]
            if sample.time_at_rx is not None:
                received_count += 1
            total_count += 1
            pdr = received_count / total_count
            pdr_per_seq[seq] = pdr

        return pdr_per_seq

    def print(self):
        logger.info(f'Node {self.node.id} delay samples:')
        for sample in self.samples.values():
            if sample.time_at_rx is None:
                continue
            logger.info(
                f'  seq {sample.seq}: delay {sample.delay} (tx {sample.time_at_tx}, rx {sample.time_at_rx})')
