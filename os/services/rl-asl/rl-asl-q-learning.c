#include "rl-asl-q-learning.h"
#include <stdlib.h> // For rand()

/* Log system */
#include "sys/log.h"
#define LOG_MODULE "rl-asl-q-learning"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_Q_LEARNING

rl_asl_q_table_t rl_asl_q_table;

/***************************************************************/
void rl_asl_q_learning_init(void)
{
    // Initialize Q-table with zeros
    rl_asl_q_learning_reset_table();

    LOG_INFO("Q-Learning initialized with %d states, %d actions and number of steps per episode %d\n",
             RL_ASL_NUM_STATES, RL_ASL_NUM_ACTIONS, RL_ASL_EPISODE_LENGTH);
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

    // Q-learning update rule
    float old_q_value = rl_asl_q_table.q_values[state][action];
    float max_next_q_value = rl_asl_q_learning_get_max_q_value(next_state);
    float new_q_value = old_q_value + RL_ASL_Q_LEARNING_ALPHA * (reward + RL_ASL_Q_LEARNING_GAMMA * max_next_q_value - old_q_value);
    rl_asl_q_table.q_values[state][action] = new_q_value;
    rl_asl_q_table.state = next_state;
}
/***************************************************************/
int rl_asl_q_learning_select_action(int state)
{
    // Epsilon-greedy action selection
    float rand_val = (float)rand() / RAND_MAX;
    if (rand_val < rl_asl_q_table.epsilon)
    {
        // Explore: select a random action
        int action = rand() % RL_ASL_NUM_ACTIONS;
        LOG_DBG("Exploring: selected random action %d (epsilon=%.3f)\n", action, rl_asl_q_table.epsilon);
        return action;
    }
    else
    {
        // Exploit: select the action with the highest Q-value
        return rl_asl_q_learning_get_best_action(state);
    }
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
    rl_asl_q_table.epsilon *= decay_rate;
    if (rl_asl_q_table.epsilon < RL_ASL_Q_LEARNING_MIN_EPSILON)
    {
        rl_asl_q_table.epsilon = RL_ASL_Q_LEARNING_MIN_EPSILON;
    }
    LOG_DBG("Decayed epsilon to %.3f\n", rl_asl_q_table.epsilon);
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
    {
        rl_asl_q_table.step_count = 0;
        rl_asl_q_learning_end_episode();
    }
}
/***************************************************************/
void rl_asl_q_learning_end_episode(void)
{
    rl_asl_q_table.episode_count++;
    rl_asl_q_table.epsilon *= RL_ASL_Q_LEARNING_EPSILON_DECAY;
    if (rl_asl_q_table.epsilon < RL_ASL_Q_LEARNING_MIN_EPSILON)
    {
        rl_asl_q_table.epsilon = RL_ASL_Q_LEARNING_MIN_EPSILON;
    }
    LOG_INFO("Episode %lu ended. Epsilon=%.3f\n",
             rl_asl_q_table.episode_count, rl_asl_q_table.epsilon);
}

/***************************************************************/
int rl_asl_q_bin_interarrival(uint32_t interarrival, uint32_t asn_diff_ewma)
{
    uint32_t bin_size = asn_diff_ewma / RL_ASL_B_INTERARRIVAL;
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
void rl_asl_q_learning_print_table(void)
{
    LOG_INFO("Q-Learning Table:\n");
    for (int i = 0; i < RL_ASL_NUM_STATES; i++)
    {
        LOG_INFO("State %d: ", i);
        for (int j = 0; j < RL_ASL_NUM_ACTIONS; j++)
        {
            LOG_INFO("Action %d: %.3f ", j, rl_asl_q_table.q_values[i][j]);
        }
        LOG_INFO_("\n");
    }
    LOG_INFO_("\n");
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
    rl_asl_q_table.state = 0;
    rl_asl_q_table.step_count = 0;
    rl_asl_q_table.episode_count = 0;
    LOG_INFO("Q-Learning table reset\n");
}
/***************************************************************/