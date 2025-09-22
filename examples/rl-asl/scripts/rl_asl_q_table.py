import numpy as np


class RLASLQTable():
    def __init__(self):
        self.num_states = 0
        self.num_actions = 0
        self.q_table = None

    def is_initialized(self) -> bool:
        return self.q_table is not None

    def initialize(self, num_states: int, num_actions: int) -> None:
        self.num_states = num_states
        self.num_actions = num_actions
        self.q_table = np.zeros((num_states, num_actions))

    def set_q_value(self, state: int, action: int, value: float) -> None:
        assert 0 <= state < self.num_states
        assert 0 <= action < self.num_actions
        self.q_table[state, action] = value

    def get_q_value(self, state: int, action: int) -> float:
        assert 0 <= state < self.num_states
        assert 0 <= action < self.num_actions
        return self.q_table[state, action]
