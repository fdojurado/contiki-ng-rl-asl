#ifndef RL_ASL_Q_LEARNING_H
#define RL_ASL_Q_LEARNING_H

#include "rl-asl.h"
#include "rl-asl-data-packet-generator.h"

/*****************************************************************
 * Q-Learning parameters
 *****************************************************************/

#ifndef RL_ASL_CONF_MODE_TRAIN
#define RL_ASL_CONF_MODE_TRAIN 0
#endif

#ifndef RL_ASL_CONF_MODE_EVAL
#define RL_ASL_CONF_MODE_EVAL 0
#endif

/* Resolve final mode */
#if (RL_ASL_CONF_MODE_TRAIN && RL_ASL_CONF_MODE_EVAL)
#warning "Both TRAIN and EVAL mode requested. Falling back to TRAIN."
#define RL_ASL_MODE_TRAIN 1
#define RL_ASL_MODE_EVAL 0
#elif RL_ASL_CONF_MODE_TRAIN
#define RL_ASL_MODE_TRAIN 1
#define RL_ASL_MODE_EVAL 0
#elif RL_ASL_CONF_MODE_EVAL
#define RL_ASL_MODE_TRAIN 0
#define RL_ASL_MODE_EVAL 1
#else
// #warning "No mode defined. Falling back to TRAIN."
#define RL_ASL_MODE_TRAIN 1
#define RL_ASL_MODE_EVAL 0
#endif

/* Convenience macro: true if training, false if evaluating */
#define RL_ASL_IS_TRAIN (RL_ASL_MODE_TRAIN)
#define RL_ASL_IS_EVAL (RL_ASL_MODE_EVAL)

#define RL_ASL_EPISODE_LENGTH 500 // Number of slotframes per episode (150 was good for scenario 1)

#define Q_SCALE 100      // 1.0 -> 100
#define REWARD_SCALE 100 // keep rewards consistent

#define RL_ASL_Q_LEARNING_ALPHA 15
#define RL_ASL_Q_LEARNING_GAMMA 90
#define RL_ASL_Q_LEARNING_EPSILON 1000
#define RL_ASL_Q_LEARNING_MIN_EPSILON 50
#define RL_ASL_Q_LEARNING_EPSILON_DECAY 997

#define RL_ASL_B_INTERARRIVAL 10

/* Maximum neighbors to consider when building aggregated features.
   Increase if you expect more simultaneous neighbors (but that increases #states). */
#ifndef RL_ASL_MAX_NEIGHBORS
#define RL_ASL_MAX_NEIGHBORS 3
#endif

#ifndef RL_ASL_DIST_NEAREST_BINS
#define RL_ASL_DIST_NEAREST_BINS 4
#endif

#ifndef RL_ASL_NEAR_COUNT_MAX
#define RL_ASL_NEAR_COUNT_MAX 3 // cap near-TX neighbors counted
#endif

/* We encode: state = avg_bin + count_short * RL_ASL_B_INTERARRIVAL
   count_short ranges 0..RL_ASL_MAX_NEIGHBORS => (RL_ASL_MAX_NEIGHBORS + 1) possibilities */
#define RL_ASL_NUM_STATES (RL_ASL_B_INTERARRIVAL *      \
                           (RL_ASL_MAX_NEIGHBORS + 1) * \
                           RL_ASL_DIST_NEAREST_BINS *   \
                           (RL_ASL_NEAR_COUNT_MAX + 1))

/* Short threshold for "short interarrival" counting (bin index) */
#ifndef RL_ASL_SHORT_BIN_THRESHOLD
#define RL_ASL_SHORT_BIN_THRESHOLD 2
#endif

#define REWARD_RX_TX (1 * REWARD_SCALE)
#define REWARD_SKIP_RX_NO_TX (50) // 0.5 * 100
#define PENALTY_RX_NO_TX (-50)    // -0.5 * 100
#define PENALTY_SKIP_RX_TX (-100) // -1.0 * 100

#define REWARD_SUCCESS (500)   // 5.0 * 100
#define PENALTY_FAILURE (-500) // -5.0 * 100

// episode window averages
#define RL_ASL_EPISODE_AVG_WINDOW 100

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
    uint16_t q_values[RL_ASL_NUM_STATES][RL_ASL_NUM_ACTIONS];
    uint16_t epsilon; // store epsilon scaled (e.g., 0–1000 for 0.0–1.0)
    int state;
    int action;
    uint64_t step_count;
    uint64_t episode_count;
    int32_t episode_return; // store scaled reward sum

    int32_t episode_returns_buffer[RL_ASL_EPISODE_AVG_WINDOW];
    int buffer_index;
    int buffer_filled;
} rl_asl_q_table_t;

extern rl_asl_q_table_t rl_asl_q_table;

/*****************************************************************
 * Q-Learning functions
 *****************************************************************/

void rl_asl_q_learning_init(void);
void rl_asl_q_learning_update(int state, int action, int reward, int next_state);
int rl_asl_q_learning_select_action(int state);
int rl_asl_q_learning_get_state(int interarrival_bin);
int rl_asl_q_learning_get_aggregated_state_from_bins(const int *bins,
                                                     int num_bins,
                                                     int dist_nearest_bin,
                                                     int near_count);
void rl_asl_q_learning_decay_epsilon(void);
int rl_asl_q_learning_get_max_q_value(int state);
int rl_asl_q_learning_get_best_action(int state);
void rl_asl_q_learning_step_done(void);
void rl_asl_q_learning_end_episode(void);
int rl_asl_q_bin_interarrival(uint32_t interarrival, uint32_t asn_diff_ewma);
int rl_asl_q_bin_dist_nearest(float dist_nearest);
void rl_asl_q_learning_print_table(void);
void rl_asl_q_learning_reset_table(void);

#endif /* RL_ASL_Q_LEARNING_H */
