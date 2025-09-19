class RLASLTrace():
    def __init__(
        self,
        seq: int,
        asn: int,
        state: int,
        action: str,
        reward: float,
        packet: int = None,
        epsilon: float = None,
        episode: str = None,
        time=None
    ) -> None:
        assert isinstance(seq, int)
        assert isinstance(asn, int)
        assert isinstance(state, int)
        assert isinstance(action, str)
        if reward is not None:
            assert isinstance(reward, float)

        self.seq = seq              # like cycle counter or log seq
        self.asn = asn
        self.state = state
        self.action = action        # "LISTEN" or "SKIP"
        self.reward = reward
        self.packet = packet        # 0/1 if known
        self.epsilon = epsilon      # exploration rate
        self.episode = episode      # "SUCCESS", "FAIL", or None
        self.time = time            # log timestamp if you have it

    def __str__(self):
        return (f"RLASLTrace(seq={self.seq}, asn={self.asn}, state={self.state}, "
                f"action={self.action}, pkt={self.packet}, reward={self.reward:.2f}, "
                f"eps={self.epsilon}, episode={self.episode}, time={self.time})")


class RLASLTraceSamples():
    def __init__(self, node) -> None:
        self.node = node
        self.samples = {}   # seq → RLASLTrace
        self.last_seq = 0

    def add_sample(self, seq: int, data: dict, time=None) -> RLASLTrace:
        trace = RLASLTrace(
            seq=seq,
            asn=data.get("asn"),
            state=data.get("state"),
            action=data.get("action"),
            reward=data.get("reward"),
            packet=data.get("packet"),
            epsilon=data.get("epsilon"),
            episode=data.get("episode"),
            time=time
        )
        self.samples[seq] = trace
        if seq > self.last_seq:
            self.last_seq = seq
        return trace

    def get_sample(self, seq: int) -> RLASLTrace:
        return self.samples.get(seq)

    def get_samples(self) -> dict[int, RLASLTrace]:
        return self.samples

    def get_average_reward(self):
        if not self.samples:
            return None
        total = 0.0
        count = 0
        for s in self.samples.values():
            total += s.reward
            count += 1
        return total / count if count else None
