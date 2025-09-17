#include "rl-asl.h"
#include <stdlib.h>
#include "rl-asl-conf.h"
#include "rl-asl-q-learning.h"

/* log */
#include "sys/log.h"
#define LOG_MODULE "rl-asl"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL

void rl_asl_check_skip_rx(const struct tsch_link *link, bool *skip_rx)
{
    // uint16_t sf_handle = link->slotframe_handle;
    // if (sf_handle != RL_ASL_UNICAST_SLOTFRAME_HANDLE)
    // {
    //     *skip_rx = false;
    //     return;
    // }

    if (tsch_is_associated == 0)
    {
        *skip_rx = false;
        return;
    }

    int listen = 0;
    int reward_mean = 0;
    int interarrival = 0;
    int asn = 0; // Placeholder
    int state = rl_asl_q_learning_get_state(listen, reward_mean, interarrival, asn);
    if (state == -1)
    {
        *skip_rx = RL_ASL_ACTION_DO_NOT_SKIP_RX;
        return;
    }
    int action = rl_asl_q_learning_select_action(state);
    *skip_rx = (action == RL_ASL_ACTION_SKIP_RX);
    // rl_asl_q_learning_update(state, action, 0.0, state); // Placeholder for reward and next_state
    rl_asl_q_learning_decay_epsilon(RL_ASL_Q_LEARNING_EPSILON_DECAY);
}
