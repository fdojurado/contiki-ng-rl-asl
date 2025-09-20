#include "rl-asl.h"
#include <stdlib.h>
#include "rl-asl-conf.h"
#include "rl-asl-q-learning.h"
#include "rl-asl-ds-nbr.h"
#include "rl-asl-decision-buffer.h"
#include "os/services/orchestra/orchestra.h" // For ORCHESTRA_UNICAST_PERIOD
#include <math.h>                            // For expf()

/* log */
#include "sys/log.h"
#define LOG_MODULE "rl-asl"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL

#define RL_ASL_INVALID_EXPECTED_REWARD -1000.0f

static float clampf(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static float rl_asl_compute_p(uint32_t asn_diff_ewma, uint32_t elapsed)
{
    if (asn_diff_ewma < 1)
        return 0.5f; // fallback

    float lambda = (float)asn_diff_ewma;
    float r = (float)elapsed / lambda;

    // Poisson model: probability that >=1 event arrived in elapsed slots
    float p = 1.0f - expf(-r);

    return clampf(p, 0.001f, 0.95f);
}

static float rl_asl_expected_reward_for_action(int action, float p)
{
    if (action == RL_ASL_ACTION_SKIP_RX)
    {
        return p * PENALTY_SKIP_RX_TX + (1.0f - p) * REWARD_SKIP_RX_NO_TX;
    }
    return RL_ASL_INVALID_EXPECTED_REWARD;
}

void rl_asl_on_slot_outcome(uint32_t asn_low32, bool packet_received)
{
    int prev_state, prev_action;
    if (!rl_asl_decision_buffer_consume(asn_low32, &prev_state, &prev_action))
    {
        return;
    }

    // Defensive checks
    if (prev_state < 0 || prev_state >= RL_ASL_NUM_STATES ||
        prev_action < 0 || prev_action >= RL_ASL_NUM_ACTIONS)
    {
        LOG_ERR("rl_asl_on_slot_outcome: invalid stored decision state=%d action=%d\n",
                prev_state, prev_action);
        return;
    }

    if (prev_action == RL_ASL_ACTION_SKIP_RX)
    {
        LOG_ERR("rl_asl_on_slot_outcome: unexpected action SKIP_RX in rl_asl_on_slot_outcome\n");
        return;
    }

    float actual_reward = packet_received ? REWARD_RX_TX : PENALTY_RX_NO_TX;

    // Terminal on successful reception
    if (packet_received)
    {
        rl_asl_q_learning_end_episode();
        actual_reward += REWARD_SUCCESS; // bonus
    }

    LOG_INFO("TRACE_OUTCOME,ASN=%u,ACTION=LISTEN,SUCCESS=%d\n",
             asn_low32,
             packet_received ? 1 : 0);

    int next_state = prev_state;
    rl_asl_q_learning_update(prev_state, prev_action, actual_reward, next_state);
}

void rl_asl_check_skip_rx(const struct tsch_link *link, bool *skip_rx)
{
    if (tsch_is_associated == 0)
    {
        *skip_rx = false;
        return;
    }

    if (link->slotframe_handle != RL_ASL_UNICAST_SLOTFRAME_HANDLE)
    {
        *skip_rx = false;
        return;
    }

    if (rl_asl_ds_nbr_count() == 0 || rl_asl_ds_nbr_is_there_a_non_paired_child())
    {
        *skip_rx = false;
        return;
    }

    rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_get_any();
    if (nbr == NULL)
    {
        *skip_rx = false;
        return;
    }

    uint64_t last_heard_asn = nbr->last_heard_asn;
    uint32_t asn_diff_ewma = nbr->asn_diff_ewma;
    if (last_heard_asn == 0 || asn_diff_ewma == 0)
    {
        *skip_rx = false;
        return;
    }

    uint64_t curr_asn64 = ((uint64_t)tsch_current_asn.ms1b << 32) | tsch_current_asn.ls4b;
    uint64_t est_diff64 = curr_asn64 >= last_heard_asn ? curr_asn64 - last_heard_asn : 0;
    uint32_t estimated_neighbor_asn = (est_diff64 > UINT32_MAX) ? UINT32_MAX : (uint32_t)est_diff64;

    int interarrival_bin = rl_asl_q_bin_interarrival(estimated_neighbor_asn, asn_diff_ewma);
    if (interarrival_bin < 0)
        interarrival_bin = 0;
    if (interarrival_bin >= RL_ASL_B_INTERARRIVAL)
        interarrival_bin = RL_ASL_B_INTERARRIVAL - 1;

    int current_state = rl_asl_q_learning_get_state(interarrival_bin);
    if (current_state == -1)
    {
        *skip_rx = RL_ASL_ACTION_DO_NOT_SKIP_RX;
        return;
    }

    int chosen_action = rl_asl_q_learning_select_action(current_state);

    uint32_t asn_low32 = (uint32_t)curr_asn64;
    rl_asl_decision_buffer_add(asn_low32, current_state, chosen_action);

    rl_asl_q_table.state = current_state;
    rl_asl_q_table.action = chosen_action;

    // Decision log (before outcome known)
    LOG_INFO("TRACE_ACTION,ASN=%u,ACTION=%s\n",
             asn_low32,
             (chosen_action == RL_ASL_ACTION_SKIP_RX ? "SKIP" : "LISTEN"));

    if (chosen_action == RL_ASL_ACTION_SKIP_RX)
    {
        float p = rl_asl_compute_p(asn_diff_ewma, estimated_neighbor_asn);
        float expected_reward = rl_asl_expected_reward_for_action(chosen_action, p);
        int pkt = 0;

        if (estimated_neighbor_asn >= (asn_diff_ewma + ORCHESTRA_UNICAST_PERIOD * 7))
        {
            rl_asl_q_learning_end_episode();
            expected_reward += PENALTY_FAILURE;
            pkt = 1;
        }

        LOG_INFO("TRACE_OUTCOME,ASN=%u,ACTION=SKIP,SUCCESS=%d\n",
                 asn_low32,
                 pkt ? 0 : 1);

        rl_asl_q_learning_update(current_state, chosen_action, expected_reward, current_state);
    }

    *skip_rx = (chosen_action == RL_ASL_ACTION_SKIP_RX);
    rl_asl_q_learning_step_done();
}
