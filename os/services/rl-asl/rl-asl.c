#include "rl-asl.h"
#include <stdlib.h>
#include "rl-asl-conf.h"
#include "rl-asl-q-learning.h"
#include "rl-asl-ds-nbr.h"
#include "rl-asl-decision-buffer.h"

/* log */
#include "sys/log.h"
#define LOG_MODULE "rl-asl"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL

static float clampf(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static float rl_asl_compute_p(uint32_t asn_diff_ewma, uint32_t estimated_elapsed_s)
{
    // params: tune these
    const float p_min = 0.001f;  // minimal belief
    const float p_max = 0.95f;   // maximal belief
    const float alpha = 1.0f;    // modulation strength (tune)
    const float mult_min = 0.2f; // lower multiplier bound
    const float mult_max = 4.0f; // upper multiplier bound

    // safety
    float lambda = (asn_diff_ewma < 1u) ? 1.0f : (float)asn_diff_ewma;
    float s = (float)estimated_elapsed_s;

    // base per-slot probability (cheap approx)
    float p0 = 1.0f / lambda;

    // elapsed ratio
    float r = s / lambda;

    // linear modulation (simple, robust)
    float multiplier = 1.0f + alpha * (r - 1.0f);

    // clamp multiplier to avoid extreme scaling
    multiplier = clampf(multiplier, mult_min, mult_max);

    float p = p0 * multiplier;
    p = clampf(p, p_min, p_max);

    return p;
}

static float rl_asl_expected_reward_for_action(int action, float p)
{
    if (action == RL_ASL_ACTION_SKIP_RX)
    {
        return p * PENALTY_SKIP_RX_TX + (1.0f - p) * REWARD_SKIP_RX_NO_TX;
    }
    else
    { // listen
        return p * REWARD_RX_TX + (1.0f - p) * PENALTY_RX_NO_TX;
    }
}

void rl_asl_on_slot_outcome(uint32_t asn_low32, bool packet_received)
{
    int prev_state, prev_action;
    if (!rl_asl_decision_buffer_consume(asn_low32, &prev_state, &prev_action))
    {
        // LOG_DBG("rl_asl_on_slot_outcome: no stored decision for ASN %u\n", asn_low32);
        return;
    }

    // Compute actual reward for previous action (listen or skip)
    if (prev_action == RL_ASL_ACTION_SKIP_RX)
    {
        LOG_ERR("Unexpected: outcome for SKIP action should not be reported here\n");
        return;
    }

    float actual_reward;
    actual_reward = packet_received ? REWARD_RX_TX : PENALTY_RX_NO_TX;

    // Compute next_state (optional): re-evaluate interarrival bin / neighbor stats now, or reuse prev_state
    // For simplicity use prev_state as next_state; this is acceptable for immediate correction.
    int next_state = prev_state;
    rl_asl_q_learning_update(prev_state, prev_action, actual_reward, next_state);

    LOG_INFO("Outcome ASN=%u: prev_state=%d action=%d packet=%d reward=%.3f\n",
             asn_low32, prev_state, prev_action, (int)packet_received, actual_reward);
}

// ---------- corrected rl_asl_check_skip_rx ----------
void rl_asl_check_skip_rx(const struct tsch_link *link, bool *skip_rx)
{
    if (tsch_is_associated == 0)
    {
        *skip_rx = false;
        return;
    }
    if (rl_asl_ds_nbr_count() == 0)
    {
        *skip_rx = false;
        return;
    }

    rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_get_any();
    if (nbr == NULL)
    {
        LOG_ERR("No neighbor found, cannot decide on skipping RX\n");
        *skip_rx = false;
        return;
    }

    uint64_t last_heard_asn = nbr->last_heard_asn;
    uint32_t asn_diff_ewma = nbr->asn_diff_ewma;

    /* get current ASN robustly */
    uint64_t curr_asn64 = ((uint64_t)tsch_current_asn.ms1b << 32) | tsch_current_asn.ls4b;

    uint64_t est_diff64 = 0;
    if (curr_asn64 >= last_heard_asn)
    {
        est_diff64 = curr_asn64 - last_heard_asn;
    }
    else
    {
        est_diff64 = 0;
    }
    uint32_t estimated_neighbor_asn = (est_diff64 > UINT32_MAX) ? UINT32_MAX : (uint32_t)est_diff64;

    LOG_DBG("Neighbor last=%llu curr=%llu est_diff=%u EWMA=%u\n",
            (unsigned long long)last_heard_asn,
            (unsigned long long)curr_asn64,
            estimated_neighbor_asn,
            asn_diff_ewma);

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

    /* If previous decision exists and was SKIP, apply expected reward now.
       If previous decision was LISTEN, defer update until rl_asl_on_slot_outcome() is called. */
    if (rl_asl_q_table.state >= 0 && rl_asl_q_table.action >= 0 &&
        rl_asl_q_table.state < RL_ASL_NUM_STATES && rl_asl_q_table.action < RL_ASL_NUM_ACTIONS)
    {

        if (rl_asl_q_table.action == RL_ASL_ACTION_SKIP_RX)
        {
            // apply expected reward (we don't yet know the real outcome)
            float p = rl_asl_compute_p(asn_diff_ewma, estimated_neighbor_asn);
            float expected_reward = rl_asl_expected_reward_for_action(rl_asl_q_table.action, p);
            rl_asl_q_learning_update(rl_asl_q_table.state, rl_asl_q_table.action, expected_reward, current_state);
            LOG_DBG("Exp-update prev_state=%d action=SKIP expected_r=%.3f next=%d\n",
                    rl_asl_q_table.state, expected_reward, current_state);
        }
        else
        {
            // previous was LISTEN — do nothing now; we will update in rl_asl_on_slot_outcome()
            LOG_DBG("Previous action LISTEN -> deferring actual update until slot outcome observed\n");
        }
    }

    /* Choose action for current state */
    int chosen_action = rl_asl_q_learning_select_action(current_state);

    /* Store decision in history keyed by ASN low32 so we can apply true reward later */
    uint32_t asn_low32 = (uint32_t)curr_asn64; // low 32 bits for matching
    rl_asl_decision_buffer_add(asn_low32, current_state, chosen_action);

    rl_asl_q_table.state = current_state;
    rl_asl_q_table.action = chosen_action;

    *skip_rx = (chosen_action == RL_ASL_ACTION_SKIP_RX);
    LOG_DBG("Decision ASN=%u state=%d action=%d skip=%d (eps=%.3f)\n",
            asn_low32, current_state, chosen_action, (int)*skip_rx, rl_asl_q_table.epsilon);

    rl_asl_q_learning_step_done();
    return;
}
