#!/usr/bin/env python3

import math
import random

# -----------------------------
# Configuration
# -----------------------------
NODE_ID = 3
OUTPUT_FILE = "simple-topology-mobility.dat"

# Circle definition
CENTER_X = 0.0
CENTER_Y = 40.0
RADIUS = 40.0

# Motion parameters
SPEED_MPS = 0.5          # meters per second
TIME_STEP = 1.0          # seconds per mobility update

# Pause parameters (seconds)
PAUSE_MIN = 3
PAUSE_MAX = 6

RANDOM_SEED = 24


def write_pause(f, time_s, x, y, duration):
    for _ in range(duration):
        f.write(f"{NODE_ID} {time_s:.1f} {x:.2f} {y:.2f}\n")
        time_s += 1.0
    return time_s


def main():
    random.seed(RANDOM_SEED)

    # Angular limits
    theta_start = 0.0          # (40, 40)
    theta_end = math.pi        # (-40, 40)

    # Arc length and timing
    arc_length = RADIUS * abs(theta_end - theta_start)
    total_time = arc_length / SPEED_MPS
    steps = int(total_time / TIME_STEP)
    dtheta = (theta_end - theta_start) / steps

    time_s = 0.0

    with open(OUTPUT_FILE, "w") as f:

        # ---- Forward arc: (40,40) -> (-40,40) ----
        for i in range(steps + 1):
            theta = theta_start + i * dtheta
            x = CENTER_X + RADIUS * math.cos(theta)
            y = CENTER_Y + RADIUS * math.sin(theta)
            f.write(f"{NODE_ID} {time_s:.1f} {x:.2f} {y:.2f}\n")
            time_s += TIME_STEP

        # Pause at far end
        pause = random.randint(PAUSE_MIN, PAUSE_MAX)
        time_s = write_pause(f, time_s, x, y, pause)

        # ---- Return arc: (-40,40) -> (40,40) ----
        for i in range(steps + 1):
            theta = theta_end - i * dtheta
            x = CENTER_X + RADIUS * math.cos(theta)
            y = CENTER_Y + RADIUS * math.sin(theta)
            f.write(f"{NODE_ID} {time_s:.1f} {x:.2f} {y:.2f}\n")
            time_s += TIME_STEP

        # Pause at initial position
        pause = random.randint(PAUSE_MIN, PAUSE_MAX)
        time_s = write_pause(f, time_s, x, y, pause)

    print(f"Generated {OUTPUT_FILE}")
    print(f"Total simulation time: {time_s:.1f} s")
    print("Mobility pattern: circular forward and return (repeatable)")


if __name__ == "__main__":
    main()
