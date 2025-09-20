#include "rl-asl.h"
#include <stdlib.h>
#include "rl-asl-conf.h"
#include "rl-asl-q-learning.h"
#include "rl-asl-ds-nbr.h"
#include "rl-asl-decision-buffer.h"
#include "os/services/orchestra/orchestra.h"
#include <math.h> // For expf()

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

static float rl_asl_compute_p(const uint64_t *curr_asn64)
{
    float prod_no_tx = 1.0f;

    for (rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_head();
         nbr != NULL;
         nbr = rl_asl_ds_nbr_next(nbr))
    {
        if (nbr->is_child && nbr->asn_diff_ewma != 0)
        {
            // elapsed slots since last packet from this neighbor
            uint64_t elapsed64 = (*curr_asn64 >= nbr->last_heard_asn)
                                     ? (*curr_asn64 - nbr->last_heard_asn)
                                     : 0;
            uint32_t elapsed_asn = (elapsed64 > UINT32_MAX) ? UINT32_MAX : (uint32_t)elapsed64;

            float lambda = (float)nbr->asn_diff_ewma;
            float p_tx = 1.0f - expf(-(float)elapsed_asn / lambda);
            p_tx = clampf(p_tx, 0.0f, 1.0f);

            // joint probability of no transmission = product of (1 - p_tx)
            prod_no_tx *= (1.0f - p_tx);

            LOG_DBG("Neighbor %02x:%02x elapsed=%u ewma=%u p_tx=%.3f\n",
                    rl_asl_ds_nbr_get_addr(nbr)->u8[0],
                    rl_asl_ds_nbr_get_addr(nbr)->u8[1],
                    elapsed_asn,
                    nbr->asn_diff_ewma,
                    p_tx);
        }
    }

    LOG_DBG("Computed p=%.3f\n", 1.0f - prod_no_tx);

    // probability that at least one child transmits
    return fminf(1.0f - prod_no_tx, 0.9f);
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

    int bins[RL_ASL_MAX_NEIGHBORS];
    int nb_count = 0;

    float avg_asn_diff_ewma;
    float sum_asn_diff_ewma = 0.0f;
    float avg_estimated_asn = 0.0f;
    float sum_estimated_asn = 0.0f;

    uint64_t curr_asn64 = ((uint64_t)tsch_current_asn.ms1b << 32) | tsch_current_asn.ls4b;

    rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_head();
    while (nbr != NULL && nb_count < RL_ASL_MAX_NEIGHBORS)
    {

        if (nbr->is_child && nbr->last_heard_asn != 0 && nbr->asn_diff_ewma != 0)
        {
            uint64_t est_diff64 = curr_asn64 >= nbr->last_heard_asn ? curr_asn64 - nbr->last_heard_asn : 0;
            uint32_t estimated_neighbor_asn = (est_diff64 > UINT32_MAX) ? UINT32_MAX : (uint32_t)est_diff64;

            sum_estimated_asn += (float)estimated_neighbor_asn;
            sum_asn_diff_ewma += (float)nbr->asn_diff_ewma;

            int interarrival_bin = rl_asl_q_bin_interarrival(estimated_neighbor_asn, nbr->asn_diff_ewma);
            if (interarrival_bin < 0)
                interarrival_bin = 0;
            if (interarrival_bin >= RL_ASL_B_INTERARRIVAL)
                interarrival_bin = RL_ASL_B_INTERARRIVAL - 1;

            bins[nb_count] = interarrival_bin;
            nb_count++;
        }
        nbr = rl_asl_ds_nbr_next(nbr);
    }

    if (nb_count == 0)
    {
        *skip_rx = false;
        return;
    }

    avg_asn_diff_ewma = sum_asn_diff_ewma / (float)nb_count;
    avg_estimated_asn = sum_estimated_asn / (float)nb_count;
    uint32_t asn_diff_ewma = (uint32_t)avg_asn_diff_ewma;
    uint32_t estimated_neighbor_asn = (uint32_t)avg_estimated_asn;
    /* Build aggregated state from bins */
    int aggregated_state = rl_asl_q_learning_get_aggregated_state_from_bins(bins, nb_count);
    if (aggregated_state < 0)
    {
        *skip_rx = false;
        return;
    }

    int chosen_action = rl_asl_q_learning_select_action(aggregated_state);

    uint32_t asn_low32 = (uint32_t)curr_asn64;
    rl_asl_decision_buffer_add(asn_low32, aggregated_state, chosen_action);

    rl_asl_q_table.state = aggregated_state;
    rl_asl_q_table.action = chosen_action;

    // Decision log (before outcome known)
    LOG_INFO("TRACE_ACTION,ASN=%u,ACTION=%s\n",
             asn_low32,
             (chosen_action == RL_ASL_ACTION_SKIP_RX ? "SKIP" : "LISTEN"));

    if (chosen_action == RL_ASL_ACTION_SKIP_RX)
    {
        float p = rl_asl_compute_p(&curr_asn64);
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

        rl_asl_q_learning_update(aggregated_state, chosen_action, expected_reward, aggregated_state);
    }

    *skip_rx = (chosen_action == RL_ASL_ACTION_SKIP_RX);
    rl_asl_q_learning_step_done();
}
