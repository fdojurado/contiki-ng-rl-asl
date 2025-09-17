#ifndef RL_ASL_Q_LEARNING_H
#define RL_ASL_Q_LEARNING_H

#include "rl-asl.h"
#include "rl-asl-data-packet-generator.h"

/*****************************************************************
 * Q-Learning parameters
 *****************************************************************/

#define RL_ASL_EPISODE_LENGTH (int)(RL_ASL_DATA_PACKET_GENERATOR_TX_INTERVAL_S * 1e6 / TSCH_DEFAULT_TIMESLOT_TIMING[tsch_ts_timeslot_length] + 1)

#define RL_ASL_Q_LEARNING_ALPHA 0.1
#define RL_ASL_Q_LEARNING_GAMMA 0.9
#define RL_ASL_Q_LEARNING_EPSILON 0.8
#define RL_ASL_Q_LEARNING_MIN_EPSILON 0.01
#define RL_ASL_Q_LEARNING_EPSILON_DECAY 0.99

#define RL_ASL_B_INTERARRIVAL 10

#define REWARD_RX_TX 10.0
#define REWARD_SKIP_RX_NO_TX 1.0
#define PENALTY_RX_NO_TX -1.0
#define PENALTY_SKIP_RX_TX -10.0

#define RL_ASL_NUM_STATES RL_ASL_B_INTERARRIVAL

// enum actions
enum
{
    RL_ASL_ACTION_SKIP_RX,
    RL_ASL_ACTION_DO_NOT_SKIP_RX,
    RL_ASL_NUM_ACTIONS
};

// Q-Learning table
typedef struct
{
    float q_values[RL_ASL_NUM_STATES][RL_ASL_NUM_ACTIONS];
    float epsilon;
    int state;
    unsigned long step_count;    // total steps in current episode
    unsigned long episode_count; // total episodes completed
} rl_asl_q_table_t;

extern rl_asl_q_table_t rl_asl_q_table;

/*****************************************************************
 * Q-Learning functions
 *****************************************************************/

void rl_asl_q_learning_init(void);
void rl_asl_q_learning_update(const int, const int, const float, const int);
int rl_asl_q_learning_select_action(int state);
int rl_asl_q_learning_get_state(int interarrival);
void rl_asl_q_learning_decay_epsilon(float decay_rate);
float rl_asl_q_learning_get_max_q_value(int state);
int rl_asl_q_learning_get_best_action(int state);
void rl_asl_q_learning_step_done(void);
void rl_asl_q_learning_end_episode(void);
int rl_asl_q_bin_interarrival(uint32_t interarrival, uint32_t asn_diff_ewma);
void rl_asl_q_learning_print_table(void);
void rl_asl_q_learning_reset_table(void);

#endif /* RL_ASL_Q_LEARNING_H */