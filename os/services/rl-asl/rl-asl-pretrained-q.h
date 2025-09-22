#ifndef RL_ASL_PRETRAINED_Q_H
#define RL_ASL_PRETRAINED_Q_H

#include "rl-asl-q-learning.h"

static const float rl_asl_pretrained_q[RL_ASL_NUM_STATES][RL_ASL_NUM_ACTIONS] = {
    {0.0f, 0.0f},
    {0.0f, 0.0f},
    {0.0f, 0.0f},
    {1.5f, -0.5f}, // State N-1
    {2.0f, -1.0f}  // State N
};

#endif /* RL_ASL_PRETRAINED_Q_H */
