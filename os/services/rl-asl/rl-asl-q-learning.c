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

    LOG_INFO("Q-Learning initialized with %d states and %d actions\n", RL_ASL_NUM_STATES, RL_ASL_NUM_ACTIONS);
}
/***************************************************************/
void rl_asl_q_learning_update_q_value(int state, int action, float reward)
{
    // Get the maximum Q-value for the next state (which is the same as current state in this context)
    float max_next_q_value = rl_asl_q_learning_get_max_q_value(state);

    // Q-learning update rule
    float old_q_value = rl_asl_q_table.q_values[state][action];
    float new_q_value = old_q_value + RL_ASL_Q_LEARNING_ALPHA * (reward + RL_ASL_Q_LEARNING_GAMMA * max_next_q_value - old_q_value);
    rl_asl_q_table.q_values[state][action] = new_q_value;

    LOG_DBG("Updated Q-value for state %d, action %d: old=%.3f, reward=%.3f, new=%.3f\n",
            state, action, old_q_value, reward, new_q_value);
}
/***************************************************************/
void rl_asl_q_learning_update(const struct tsch_link *link, bool skip_rx)
{
    // Extract state parameters from the link and other metrics
    int listen = 0;
    int reward_mean = 0;
    int interarrival = 0;
    int asn = 0; // Placeholder
    int state = rl_asl_q_learning_get_state(listen, reward_mean, interarrival, asn);
    if (state == -1)
    {
        return; // Invalid state, do not update
    }
    int action = skip_rx ? 0 : 1; // Action taken
    float reward = 0.0;           // Placeholder for reward calculation
    rl_asl_q_learning_update_q_value(state, action, reward);
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
int rl_asl_q_learning_get_state(int listen, int reward, int interarrival, int asn)
{
    // Ensure inputs are within bounds
    if (listen < 0 || listen >= RL_ASL_B_LISTEN ||
        reward < 0 || reward >= RL_ASL_B_REWARD ||
        interarrival < 0 || interarrival >= RL_ASL_B_INTERARRIVAL ||
        asn < 0 || asn >= RL_ASL_B_ASN)
    {
        LOG_ERR("Invalid state parameters: listen=%d, reward=%d, interarrival=%d, asn=%d\n",
                listen, reward, interarrival, asn);
        return -1; // Indicate error
    }

    // Compute state index
    int state = listen * RL_ASL_B_REWARD * RL_ASL_B_INTERARRIVAL * RL_ASL_B_ASN +
                reward * RL_ASL_B_INTERARRIVAL * RL_ASL_B_ASN +
                interarrival * RL_ASL_B_ASN +
                asn;

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
    LOG_INFO("Q-Learning table reset\n");
}
/***************************************************************/