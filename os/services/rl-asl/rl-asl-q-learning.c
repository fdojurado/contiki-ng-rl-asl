#include "rl-asl-q-learning.h"
#include "rl-asl-decision-buffer.h"
#include <stdlib.h> // For rand()

/* Log system */
#include "sys/log.h"
#define LOG_MODULE "rl-asl-q-learning"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_Q_LEARNING

#if RL_ASL_IS_EVAL
#include "rl-asl-federated-q-global.h"
#endif

rl_asl_q_table_t rl_asl_q_table;

static float best_rolling_avg = -1e9; // very low initial value

/***************************************************************/
void rl_asl_q_learning_init(void)
{
#if RL_ASL_IS_TRAIN
    // Initialize Q-table with zeros
    rl_asl_q_learning_reset_table();
    rl_asl_decision_buffer_reset();

    LOG_INFO("Q-Learning initialized with %d states, %d actions and number of steps per episode %d\n",
             RL_ASL_NUM_STATES, RL_ASL_NUM_ACTIONS, RL_ASL_EPISODE_LENGTH);
#elif RL_ASL_IS_EVAL
    // Load pretrained Q-table
    for (int i = 0; i < RL_ASL_NUM_STATES; i++)
    {
        for (int j = 0; j < RL_ASL_NUM_ACTIONS; j++)
        {
            rl_asl_q_table.q_values[i][j] = rl_asl_federated_q_global[i][j];
        }
    }
    rl_asl_decision_buffer_reset();

    LOG_INFO("Using model from %s with %d states, %d actions\n",
             RL_ASL_MODEL_SCENARIO, RL_ASL_NUM_STATES, RL_ASL_NUM_ACTIONS);
#endif
}
/***************************************************************/
void rl_asl_q_learning_update(const int state, const int action,
                              const float reward, const int next_state)
{
    if (state < 0 || state >= RL_ASL_NUM_STATES ||
        action < 0 || action >= RL_ASL_NUM_ACTIONS ||
        next_state < 0 || next_state >= RL_ASL_NUM_STATES)
    {
        LOG_ERR("Invalid parameters for Q-learning update: state=%d, action=%d, next_state=%d\n",
                state, action, next_state);
        return; // Invalid state, do not update
    }

#if RL_ASL_IS_TRAIN
    // Q-learning update rule
    int old_q = rl_asl_q_table.q_values[state][action];
    int max_next_q = rl_asl_q_learning_get_max_q_value(next_state);

    int target = reward + (RL_ASL_Q_LEARNING_GAMMA * max_next_q) / Q_SCALE;
    int new_q = old_q + (RL_ASL_Q_LEARNING_ALPHA * (target - old_q)) / Q_SCALE;

    rl_asl_q_table.q_values[state][action] = (uint16_t)new_q;

    rl_asl_q_table.state = next_state;

    rl_asl_q_table.episode_return += reward;

    // LOG_DBG("Step state=%d action=%d reward=%.3f next_state=%d q=%.3f\n",
    //         state, action, reward, next_state, new_q_value);
#elif RL_ASL_IS_EVAL
    // In evaluation mode, we do not update the Q-table
    (void)state;
    (void)action;
    (void)reward;
    (void)next_state;
#endif
}
/***************************************************************/
int rl_asl_q_learning_select_action(int state)
{
#if RL_ASL_IS_TRAIN
    // Epsilon-greedy action selection
    int rand_val = rand() % 1000; // 0..999
    if (rand_val < rl_asl_q_table.epsilon)
    {
        // Explore: select a random action
        int action = rand() % RL_ASL_NUM_ACTIONS;
        LOG_DBG("Exploring: selected random action %d (epsilon=%d)\n", action, rl_asl_q_table.epsilon);
        return action;
    }
    else
    {
        // Exploit: select the action with the highest Q-value
        int action = rl_asl_q_learning_get_best_action(state);
        LOG_DBG("Exploiting: selected best action %d (epsilon=%d)\n", action, rl_asl_q_table.epsilon);
        return action;
    }
#elif RL_ASL_IS_EVAL
    // Always exploit in evaluation mode
    int action = rl_asl_q_learning_get_best_action(state);
    LOG_DBG("EVAL: selected best action %d\n", action);
    return action;
#endif
}
/***************************************************************/
int rl_asl_q_learning_get_aggregated_state_from_bins(const int *bins,
                                                     int num_bins,
                                                     int dist_nearest_bin,
                                                     int near_count)
{
    if (bins == NULL || num_bins <= 0)
    {
        LOG_ERR("Invalid bins pointer or zero num_bins\n");
        return -1;
    }

    /* Clip num_bins */
    if (num_bins > RL_ASL_MAX_NEIGHBORS)
    {
        num_bins = RL_ASL_MAX_NEIGHBORS;
    }

    int sum = 0;
    int count_short = 0;

    for (int i = 0; i < num_bins; i++)
    {
        int b = bins[i];
        if (b < 0)
            b = 0;
        if (b >= RL_ASL_B_INTERARRIVAL)
            b = RL_ASL_B_INTERARRIVAL - 1;

        sum += b;
        if (b < RL_ASL_SHORT_BIN_THRESHOLD)
        {
            count_short++;
        }
    }

    /* average bin */
    int avg_bin = (int)((float)sum / num_bins + 0.5f);
    if (avg_bin < 0)
        avg_bin = 0;
    if (avg_bin >= RL_ASL_B_INTERARRIVAL)
        avg_bin = RL_ASL_B_INTERARRIVAL - 1;

    /* clip count_short */
    if (count_short < 0)
        count_short = 0;
    if (count_short > RL_ASL_MAX_NEIGHBORS)
        count_short = RL_ASL_MAX_NEIGHBORS;

    /* clip dist_nearest_bin */
    if (dist_nearest_bin < 0)
        dist_nearest_bin = 0;
    if (dist_nearest_bin >= RL_ASL_DIST_NEAREST_BINS)
        dist_nearest_bin = RL_ASL_DIST_NEAREST_BINS - 1;

    /* clip near_count */
    if (near_count < 0)
        near_count = 0;
    if (near_count > RL_ASL_NEAR_COUNT_MAX)
        near_count = RL_ASL_NEAR_COUNT_MAX;

    /* Encode 4D state */
    int state = avg_bin + RL_ASL_B_INTERARRIVAL * count_short + RL_ASL_B_INTERARRIVAL * (RL_ASL_MAX_NEIGHBORS + 1) * dist_nearest_bin + RL_ASL_B_INTERARRIVAL * (RL_ASL_MAX_NEIGHBORS + 1) * RL_ASL_DIST_NEAREST_BINS * near_count;

    if (state < 0 || state >= RL_ASL_NUM_STATES)
    {
        LOG_ERR("Aggregated state out of range: %d (avg=%d count_short=%d dist_bin=%d near_count=%d)\n",
                state, avg_bin, count_short, dist_nearest_bin, near_count);
        return -1;
    }

    return state;
}

/***************************************************************/
int rl_asl_q_learning_get_state(int interarrival)
{
    // Ensure inputs are within bounds
    if (interarrival < 0 || interarrival >= RL_ASL_B_INTERARRIVAL)
    {
        LOG_ERR("Invalid state parameters: interarrival=%d\n",
                interarrival);
        return -1; // Indicate error
    }

    // Compute state index
    int state = interarrival;
    if (state < 0 || state >= RL_ASL_NUM_STATES)
    {
        LOG_ERR("Computed invalid state index: %d\n", state);
        return -1; // Indicate error
    }
    return state;
}
/***************************************************************/
void rl_asl_q_learning_decay_epsilon(float decay_rate)
{
    rl_asl_q_table.epsilon = (rl_asl_q_table.epsilon * RL_ASL_Q_LEARNING_EPSILON_DECAY) / 1000;
    if (rl_asl_q_table.epsilon < RL_ASL_Q_LEARNING_MIN_EPSILON)
        rl_asl_q_table.epsilon = RL_ASL_Q_LEARNING_MIN_EPSILON;

    LOG_DBG("Decayed epsilon to %.3f\n", (double)rl_asl_q_table.epsilon);
}
/***************************************************************/
float rl_asl_q_learning_get_max_q_value(int state)
{
    float max_q_value = rl_asl_q_table.q_values[state][0];
    for (int a = 1; a < RL_ASL_NUM_ACTIONS; a++)
    {
        if (rl_asl_q_table.q_values[state][a] > max_q_value)
        {
            max_q_value = rl_asl_q_table.q_values[state][a];
        }
    }
    return max_q_value;
}
/***************************************************************/
int rl_asl_q_learning_get_best_action(int state)
{
    int best_action = 0;
    float max_q_value = rl_asl_q_table.q_values[state][0];
    for (int a = 1; a < RL_ASL_NUM_ACTIONS; a++)
    {
        if (rl_asl_q_table.q_values[state][a] > max_q_value)
        {
            max_q_value = rl_asl_q_table.q_values[state][a];
            best_action = a;
        }
    }
    return best_action;
}
/***************************************************************/
void rl_asl_q_learning_step_done(void)
{
    rl_asl_q_table.step_count++;
    if (rl_asl_q_table.step_count >= RL_ASL_EPISODE_LENGTH)
        rl_asl_q_learning_end_episode();
}
/***************************************************************/
void rl_asl_q_learning_end_episode(void)
{
    rl_asl_q_table.episode_count++;

    // Insert return into rolling buffer
    rl_asl_q_table.episode_returns_buffer[rl_asl_q_table.buffer_index] = rl_asl_q_table.episode_return;
    rl_asl_q_table.buffer_index = (rl_asl_q_table.buffer_index + 1) % RL_ASL_EPISODE_AVG_WINDOW;
    if (rl_asl_q_table.buffer_filled < RL_ASL_EPISODE_AVG_WINDOW)
        rl_asl_q_table.buffer_filled++;

    // Compute rolling average
    float sum = 0.0f;
    for (int i = 0; i < rl_asl_q_table.buffer_filled; i++)
        sum += rl_asl_q_table.episode_returns_buffer[i];

    float rolling_avg = (rl_asl_q_table.buffer_filled > 0) ? sum / rl_asl_q_table.buffer_filled : 0.0f;

    // --- New: track best rolling average ---
    if (rolling_avg > best_rolling_avg)
    {
        best_rolling_avg = rolling_avg;
        LOG_INFO("NEW_BEST_AGENT,EPISODE=%lu,ROLLING_AVG=%.3f\n",
                 rl_asl_q_table.episode_count,
                 (double)best_rolling_avg);
        rl_asl_q_learning_print_table();
    }

    // Log episode summary in CSV-like format
    LOG_INFO("EPISODE_END,%lu,%.3f,%.3f,%lu,%.3f\n",
             rl_asl_q_table.episode_count,
             (double)rl_asl_q_table.episode_return,
             (double)rl_asl_q_table.epsilon,
             rl_asl_q_table.step_count,
             (double)rolling_avg);

    // Reset episode accumulator
    rl_asl_q_table.episode_return = 0.0f;
    rl_asl_q_table.step_count = 0;

    // Decay epsilon
    rl_asl_q_table.epsilon *= RL_ASL_Q_LEARNING_EPSILON_DECAY;
    if (rl_asl_q_table.epsilon < RL_ASL_Q_LEARNING_MIN_EPSILON)
    {
        rl_asl_q_table.epsilon = RL_ASL_Q_LEARNING_MIN_EPSILON;
    }
}
/***************************************************************/
int rl_asl_q_bin_interarrival(uint32_t interarrival, uint32_t asn_diff_ewma)
{
    uint32_t bin_size = asn_diff_ewma / RL_ASL_B_INTERARRIVAL;
    if (asn_diff_ewma < RL_ASL_B_INTERARRIVAL)
        bin_size = 1;
    for (int i = 1; i < RL_ASL_B_INTERARRIVAL; i++)
    {
        if (interarrival < i * bin_size)
        {
            return i - 1;
        }
    }
    return RL_ASL_B_INTERARRIVAL - 1;
}
/***************************************************************/
int rl_asl_q_bin_dist_nearest(float dist_nearest)
{
    if (dist_nearest <= 2.0f)
        return 0; // very close
    else if (dist_nearest <= 8.0f)
        return 1; // near
    else if (dist_nearest <= 32.0f)
        return 2; // medium
    else
        return 3; // far
}
/***************************************************************/
void rl_asl_q_learning_print_table(void)
{
    // Print Q-table in a compact CSV format to minimize log overhead
    // Format: state,action,q_value (only for q_value > 0)
    for (int i = 0; i < RL_ASL_NUM_STATES; i++)
    {
        for (int j = 0; j < RL_ASL_NUM_ACTIONS; j++)
        {
            uint16_t q = rl_asl_q_table.q_values[i][j];
            if (q > 0)
            {
                printf("%d,%d,%d\n", i, j, q);
            }
        }
    }
}
/***************************************************************/
void rl_asl_q_learning_reset_table(void)
{
    for (int i = 0; i < RL_ASL_NUM_STATES; i++)
    {
        for (int j = 0; j < RL_ASL_NUM_ACTIONS; j++)
        {
            rl_asl_q_table.q_values[i][j] = 0.0;
        }
    }
    rl_asl_q_table.epsilon = RL_ASL_Q_LEARNING_EPSILON;
    rl_asl_q_table.state = -1;
    rl_asl_q_table.action = -1;
    rl_asl_q_table.step_count = 0;
    rl_asl_q_table.episode_count = 0;
    rl_asl_q_table.episode_return = 0.0f;
    rl_asl_q_table.buffer_index = 0;
    rl_asl_q_table.buffer_filled = 0;
    memset(rl_asl_q_table.episode_returns_buffer, 0, sizeof(rl_asl_q_table.episode_returns_buffer));
    LOG_INFO("Q-Learning table reset\n");
}
/***************************************************************/