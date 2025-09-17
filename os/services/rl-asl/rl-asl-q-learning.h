#ifndef RL_ASL_Q_LEARNING_H
#define RL_ASL_Q_LEARNING_H

#include "rl-asl.h"

/*****************************************************************
 * Q-Learning parameters
 *****************************************************************/

#define RL_ASL_Q_LEARNING_ALPHA 0.1
#define RL_ASL_Q_LEARNING_GAMMA 0.9
#define RL_ASL_Q_LEARNING_EPSILON 0.
#define RL_ASL_Q_LEARNING_MIN_EPSILON 0.01
#define RL_ASL_Q_LEARNING_EPSILON_DECAY 0.99

#define RL_ASL_B_LISTEN 4
#define RL_ASL_B_REWARD 3
#define RL_ASL_B_INTERARRIVAL 4
#define RL_ASL_B_ASN 3

#define RL_ASL_NUM_STATES (RL_ASL_B_LISTEN * RL_ASL_B_REWARD * RL_ASL_B_INTERARRIVAL * RL_ASL_B_ASN)

// enum actions
typedef enum
{
    RL_ASL_ACTION_SKIP_RX,
    RL_ASL_ACTION_DO_NOT_SKIP_RX,
    RL_ASL_NUM_ACTIONS
} rl_asl_action_t;


// Q-Learning table
typedef struct
{
    float q_values[RL_ASL_NUM_STATES][RL_ASL_NUM_ACTIONS];
    float epsilon;
    int state;
} rl_asl_q_table_t;

extern rl_asl_q_table_t rl_asl_q_table;

/*****************************************************************
 * Q-Learning functions
 *****************************************************************/

void rl_asl_q_learning_init(void);
void rl_asl_q_learning_update_q_value(int state, int action, float reward);
void rl_asl_q_learning_update(const struct tsch_link *link, bool skip_rx);
int rl_asl_q_learning_select_action(int state);
int rl_asl_q_learning_get_state(int listen, int reward, int interarrival, int asn);
void rl_asl_q_learning_decay_epsilon(float decay_rate);
float rl_asl_q_learning_get_max_q_value(int state);
int rl_asl_q_learning_get_best_action(int state);
void rl_asl_q_learning_print_table(void);
void rl_asl_q_learning_reset_table(void);

#endif /* RL_ASL_Q_LEARNING_H */