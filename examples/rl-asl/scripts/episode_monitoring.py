class EpisodeSample():
    def __init__(
            self,
            episode_count: int,
            episode_reward: float,
            epsilon: float,
            steps: int,
            avg_reward: float,
            time=None
    ) -> None:
        assert isinstance(episode_count, int)
        assert isinstance(episode_reward, float)
        assert isinstance(epsilon, float)
        assert isinstance(steps, int)
        assert isinstance(avg_reward, float)
        self.episode_count = episode_count
        self.episode_reward = episode_reward
        self.epsilon = epsilon
        self.steps = steps
        self.avg_reward = avg_reward
        self.time = time
        
    def __str__(self):
        return (f"EpisodeSample(episode_count={self.episode_count}, episode_reward={self.episode_reward}, "
                f"epsilon={self.epsilon}, steps={self.steps}, avg_reward={self.avg_reward}, time={self.time})")
        
        
class EpisodeSamples():
    def __init__(self, node) -> None:
        self.node = node
        self.samples = {}   # episode_count → EpisodeSample

    def add_sample(self, data: dict, time=None) -> EpisodeSample:
        sample = EpisodeSample(
            episode_count=data.get("episode_count"),
            episode_reward=data.get("episode_reward"),
            epsilon=data.get("epsilon"),
            steps=data.get("steps"),
            avg_reward=data.get("avg_reward"),
            time=time
        )
        self.samples[sample.episode_count] = sample
        return sample

    def get_sample(self, episode_count: int) -> EpisodeSample:
        return self.samples.get(episode_count)

    def get_samples(self) -> dict[int, EpisodeSample]:
        return self.samples
