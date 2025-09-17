#include "rl-asl-decision-buffer.h"

/* Log system */
#include "sys/log.h"
#define LOG_MODULE "rl-asl-decision-buffer"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_BUF

rl_asl_decision_t rl_asl_decision_buffer[RL_ASL_DECISION_BUFFER_SIZE];
static int decision_buffer_index = 0;

/***************************************************************/
void rl_asl_decision_buffer_add(uint32_t asn_low32, int state, int action)
{
    rl_asl_decision_buffer[decision_buffer_index].asn_low32 = asn_low32;
    rl_asl_decision_buffer[decision_buffer_index].state = state;
    rl_asl_decision_buffer[decision_buffer_index].action = action;
    rl_asl_decision_buffer[decision_buffer_index].valid = true;

    decision_buffer_index = (decision_buffer_index + 1) % RL_ASL_DECISION_BUFFER_SIZE;

    LOG_DBG("Added decision to buffer: ASN low32=%u, state=%d, action=%d\n",
            asn_low32, state, action);
}
/***************************************************************/
bool rl_asl_decision_buffer_consume(uint32_t asn_low32, int *state, int *action)
{
    for (int i = 0; i < RL_ASL_DECISION_BUFFER_SIZE; i++)
    {
        if (rl_asl_decision_buffer[i].valid && rl_asl_decision_buffer[i].asn_low32 == asn_low32)
        {
            *state = rl_asl_decision_buffer[i].state;
            *action = rl_asl_decision_buffer[i].action;
            rl_asl_decision_buffer[i].valid = false; // Mark as consumed
            return true;
        }
    }
    return false;
}
/***************************************************************/
void rl_asl_decision_buffer_reset(void)
{
    for (int i = 0; i < RL_ASL_DECISION_BUFFER_SIZE; i++)
    {
        rl_asl_decision_buffer[i].valid = false;
    }
    decision_buffer_index = 0;
    LOG_INFO("Decision buffer reset\n");
}