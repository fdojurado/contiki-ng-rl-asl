class RLASLTrace():
    def __init__(
        self,
        asn: int,
        action: str,
        success: int,
        time=None
    ) -> None:
        assert isinstance(asn, int)
        assert isinstance(action, int)
        assert isinstance(success, int)

        self.asn = asn
        self.action = action        # "LISTEN" or "SKIP"
        self.success = success
        self.time = time            # log timestamp if you have it

    def __str__(self):
        return (f"RLASLTrace(seq={self.seq}, asn={self.asn}, action={self.action}, "
                f"success={self.success}, time={self.time})")


class RLASLTraceSamples():
    def __init__(self, node) -> None:
        self.node = node
        self.samples = {}   # seq → RLASLTrace
        self.last_seq = 0

    def add_sample(self, data: dict, time=None) -> RLASLTrace:
        trace = RLASLTrace(
            asn=data.get("asn"),
            action=data.get("action"),
            success=data.get("success"),
            time=time
        )
        self.samples[trace.asn] = trace
        return trace

    def get_sample(self, asn: int) -> RLASLTrace:
        return self.samples.get(asn)

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
