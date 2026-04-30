<img src="https://github.com/contiki-ng/contiki-ng.github.io/blob/master/images/logo/Contiki_logo_2RGB.png" alt="Logo" width="256">

# Contiki-NG: The OS for Next Generation IoT Devices

[![Github Actions](https://github.com/contiki-ng/contiki-ng/workflows/CI/badge.svg?branch=develop)](https://github.com/contiki-ng/contiki-ng/actions)
[![Documentation Status](https://readthedocs.org/projects/contiki-ng/badge/?version=master)](https://contiki-ng.readthedocs.io/en/master/?badge=master)
[![license](https://img.shields.io/badge/license-3--clause%20bsd-brightgreen.svg)](https://github.com/contiki-ng/contiki-ng/blob/master/LICENSE.md)
[![Latest release](https://img.shields.io/github/release/contiki-ng/contiki-ng.svg)](https://github.com/contiki-ng/contiki-ng/releases/latest)
[![GitHub Release Date](https://img.shields.io/github/release-date/contiki-ng/contiki-ng.svg)](https://github.com/contiki-ng/contiki-ng/releases/latest)
[![Last commit](https://img.shields.io/github/last-commit/contiki-ng/contiki-ng.svg)](https://github.com/contiki-ng/contiki-ng/commit/HEAD)

[![Stack Overflow Tag](https://img.shields.io/badge/Stack%20Overflow%20tag-Contiki--NG-blue?logo=stackoverflow)](https://stackoverflow.com/questions/tagged/contiki-ng)
[![Gitter](https://img.shields.io/badge/Gitter-Contiki--NG-blue?logo=gitter)](https://gitter.im/contiki-ng)
[![Twitter](https://img.shields.io/badge/Twitter-%40contiki__ng-blue?logo=twitter)](https://twitter.com/contiki_ng)

Contiki-NG is an open-source, cross-platform operating system for Next-Generation IoT devices. It focuses on dependable (secure and reliable) low-power communication and standard protocols, such as IPv6/6LoWPAN, 6TiSCH, RPL, and CoAP. Contiki-NG comes with extensive documentation, tutorials, a roadmap, release cycle, and well-defined development flow for smooth integration of community contributions.

Unless explicitly stated otherwise, Contiki-NG sources are distributed under
the terms of the [3-clause BSD license](LICENSE.md). This license gives
everyone the right to use and distribute the code, either in binary or
source code format, as long as the copyright license is retained in
the source code.

Contiki-NG started as a fork of the Contiki OS and retains some of its original features.

Find out more:

* GitHub repository: https://github.com/contiki-ng/contiki-ng
* Documentation: https://docs.contiki-ng.org/
* List of releases and changes: https://github.com/contiki-ng/contiki-ng/releases
* Web site: http://contiki-ng.org

Engage with the community:

* Discussions on GitHub: https://github.com/contiki-ng/contiki-ng/discussions
* Contiki-NG tag on Stack Overflow: https://stackoverflow.com/questions/tagged/contiki-ng
* Gitter: https://gitter.im/contiki-ng
* Twitter: https://twitter.com/contiki_ng

---

## RL-ASL: Reader's Guide

This fork extends Contiki-NG **release/v5.1** (tagged 2025-07-10) with the
reference implementation of the paper **RL-ASL — A Dynamic Listening
Optimization for TSCH Networks Using Reinforcement Learning**
(doi:[10.1109/TMC.2026.3688437](https://doi.org/10.1109/TMC.2026.3688437)).

All RL-ASL changes were authored on top of upstream `release/v5.1`. The
first RL-ASL commit is `8887a896c` (2025-09-15, *"feat(routing): add RL
ASL routing driver implementation and header"*); running
`git log release/v5.1..HEAD` shows the full set of modifications.

> **Note — this is research code.** The implementation is split across
> two feature branches and was not consolidated before publication. The
> sections below are the recommended reading order.

### Branches

| Branch        | What it contains                                             |
|---------------|--------------------------------------------------------------|
| `multi-nodes` | **Main implementation.** RL-ASL service, Q-learning agent, PRIL baseline, Orchestra rules, static-topology Cooja experiments, and the Python analysis pipeline. **Read this branch first.** |
| `mobility`    | Adds the mobility experiments on top of `multi-nodes`: `examples/rl-asl/idle-listening-mobility/`, `examples/rl-asl/rpl-mobility/`, and the `mobile-pattern.py` Cooja motion script. The mobility work was not merged back into `multi-nodes`. |

### FIT IoT-LAB testbed (separate repository)

The Cooja simulation results in the paper can be reproduced from this
repository. The **FIT IoT-LAB testbed results**, however, were produced
from a separate fork because FIT IoT-LAB's current firmware build flow
targets Contiki-NG 4.4, not 5.1:

> **`contiki-ng-sage-4.4`** —
> `git@github.com:fdojurado/contiki-ng-sage-4.4.git`

That repository backports the same RL-ASL service, Q-learning agent,
TSCH integration, and PRIL baseline onto Contiki-NG 4.4 so the firmware
images can be flashed onto FIT IoT-LAB's M3 nodes. The protocol logic is
identical; only the host OS version and a handful of API shims differ.

### Where to read the code

#### Core RL-ASL module — `os/services/rl-asl/`

| File                              | Maps to                                               |
|-----------------------------------|-------------------------------------------------------|
| `rl-asl.{c,h}`, `rl-asl-conf.h`   | Service entry points and per-slot decision hook (`rl_asl_check_skip_rx()`). |
| `rl-asl-q-learning.{c,h}`         | Q-table, ε-greedy action selection, training loop, and eval-mode inference on the mote. |
| `rl-asl-decision-buffer.{c,h}`    | Ring buffer mapping `(ASN, state, action)` → delayed reward, used to credit slots once their outcome is observed. |
| `rl-asl-unicast-rule.c`           | RL-ASL Orchestra rule (per-neighbor variant). |
| `rl-asl-unicast-link-based.c`     | RL-ASL-LB variant (link-based scheduling, the *RL-ASL-LB* configuration in the paper). |

The pretrained, federated-averaged Q-table consumed in eval mode lives
under `examples/rl-asl/models/topology-a/rl-asl-federated-q-global.h`.

#### PRIL baseline — `os/services/pril/`

The deterministic predictive baseline that RL-ASL is compared against
(referred to as **PRIL-M** in the paper). File layout mirrors the RL-ASL
service: `pril.{c,h}`, `pril-nbr.{c,h}`, `pril-unicast-link-based.c`,
`pril-utils.{c,h}`.

#### TSCH and OS integration

| File                                          | Change                                                   |
|-----------------------------------------------|----------------------------------------------------------|
| `os/net/mac/tsch/tsch-slot-operation.{c,h}`   | Slot-skip hooks for RL-ASL/PRIL, the RX-skip path, and the per-slot outcome callback feeding Q-learning. |
| `os/sys/energest.h`                           | New `ENERGEST_TYPE_LISTEN_IDLE` counter so idle-listening time is measured separately from active RX. |
| `os/services/simple-energest/simple-energest.c` | Periodic logging of the new idle-listening counter. |
| `os/contiki-main.c`                           | Initializes the RL-ASL service at boot. |
| `os/net/routing/routing.h`                    | Hooks for the RL-ASL net processor. |
| `os/services/orchestra/orchestra-rule-unicast-link-based-static-routing.c` | New Orchestra rule for static-routing deployments, used by the link-based variants. |

#### Example application — `examples/rl-asl/`

| Path                              | Purpose                                                  |
|-----------------------------------|----------------------------------------------------------|
| `idle-listening/`                 | Main multi-scenario Cooja experiments. `.csc` files cover Topology A and B, scenarios 1–4, and each protocol variant (`orchestra`, `rl-asl-train`, `rl-asl-eval`, `rl-asl-link-based`, `pril`). |
| `idle-listening-mobility/`        | Mobility variant of the idle-listening experiment. **`mobility` branch only.** |
| `rpl-mobility/`                   | Vanilla RPL+TSCH mobility baseline. **`mobility` branch only.** |
| `orchestra/`                      | Orchestra-only baseline application. |
| `models/topology-a/`              | Pretrained per-topology Q-tables consumed in eval mode. |
| `script.js`, `script-training.js`, `script-eval.js` | Cooja test scripts that drive training and evaluation runs. |
| `scripts/`                        | Python analysis pipeline: log parsing (`process_logs.py`, `process_folder_samples.py`), per-flow latency/jitter (`delay.py`), energest decoding (`power_trace.py`), federated averaging (`fedavg.py`), Q-learning trace tools (`rl_asl_trace.py`, `rl_asl_q_table.py`), the `mobile-pattern.py` motion script, and the matplotlib `plot/` package used to produce the paper's figures. |
| Root-level glue: `rl-asl-handshake.{c,h}`, `rl-asl-data-packet-generator.{c,h}`, `rl-asl-data-packet-processor.{c,h}`, `rl-asl-net*.{c,h}`, `rl-asl-routing.{c,h}`, `rl-asl-packets.h`, `rl-asl-buf.{c,h}`, `rl-asl-ds-nbr.{c,h}`, `rl-asl-utils.{c,h}` | Application-level glue used by the example: parent/child handshake, traffic generator, packet processor, neighbor bookkeeping. `idle-listening-mobility/` carries a partly-duplicated copy of these files for the mobility variant. |

### A note on disorganization

A few rough edges to be aware of when reading the code:

- The mobility implementation lives on its own branch (`mobility`)
  rather than being merged into `multi-nodes`, so a checkout of
  `multi-nodes` alone will not contain the mobility experiments.
- `examples/rl-asl/idle-listening-mobility/` carries a partly duplicated
  copy of the `examples/rl-asl/*.{c,h}` glue files; the copies under
  `idle-listening-mobility/` are the ones used by the mobility runs.
- The FIT IoT-LAB testbed code lives in a separate repository
  (`contiki-ng-sage-4.4`, see above), not on a branch of this repo.

If you are reproducing a specific paper result, find the matching
`.csc` file under `examples/rl-asl/idle-listening/` — its filename
encodes the topology (`topology-a` / `topology-b`), scenario
(`scenario-1..4`), and protocol variant.

### Citing RL-ASL

If you use any part of this code — the RL-ASL service, the Q-learning
agent, the PRIL baseline, the Orchestra rules, the example
applications, or the Python analysis pipeline — in academic work,
please cite the paper:

> F. F. Jurado-Lasso and J. F. Jurado, *"RL-ASL: A Dynamic Listening
> Optimization for TSCH Networks Using Reinforcement Learning,"* IEEE
> Transactions on Mobile Computing, 2026.
> doi: [10.1109/TMC.2026.3688437](https://doi.org/10.1109/TMC.2026.3688437)

BibTeX:

```bibtex
@article{juradolasso2026rlasl,
  author  = {Jurado-Lasso, F. Fernando and Jurado, J. F.},
  title   = {{RL-ASL}: A Dynamic Listening Optimization for {TSCH} Networks Using Reinforcement Learning},
  journal = {IEEE Transactions on Mobile Computing},
  year    = {2026},
  doi     = {10.1109/TMC.2026.3688437}
}
```
