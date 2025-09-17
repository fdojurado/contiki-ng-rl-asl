#include "rl-asl.h"
#include <stdlib.h>
#include "rl-asl-conf.h"
#include "rl-asl-q-learning.h"
#include "rl-asl-ds-nbr.h"

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

// ---------- corrected rl_asl_check_skip_rx ----------
void rl_asl_check_skip_rx(const struct tsch_link *link, bool *skip_rx)
{
    // Early returns for basic connectivity
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

    // Pick one neighbor for now
    rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_get_any();
    if (nbr == NULL)
    {
        LOG_ERR("No neighbor found, cannot decide on skipping RX\n");
        *skip_rx = false;
        return;
    }

    uint64_t last_heard_asn = nbr->last_heard_asn; // 64-bit stored ASN
    uint32_t asn_diff_ewma = nbr->asn_diff_ewma;   // EWMA in slots

    // --- obtain full 64-bit current ASN robustly ---
    uint64_t curr_asn64 = 0;

    curr_asn64 = ((uint64_t)tsch_current_asn.ms1b << 32) | tsch_current_asn.ls4b;

    uint64_t est_diff64 = 0;
    if (curr_asn64 >= last_heard_asn)
    {
        est_diff64 = curr_asn64 - last_heard_asn;
    }
    else
    {
        // Handle wrap (shouldn't happen if we use full 64-bit), but guard nonetheless
        est_diff64 = 0; // treat as very recent — conservative choice
    }

    // clamp to uint32 for functions expecting 32-bit
    uint32_t estimated_neighbor_asn = (est_diff64 > UINT32_MAX) ? UINT32_MAX : (uint32_t)est_diff64;

    LOG_DBG("Neighbor last_heard_asn=%llu curr_asn=%llu est_diff=%u EWMA=%u",
            (unsigned long long)last_heard_asn,
            (unsigned long long)curr_asn64,
            estimated_neighbor_asn,
            asn_diff_ewma);

    // --- bin interarrival safely (ensure bin func handles zero EWMA) ---
    int interarrival_bin = rl_asl_q_bin_interarrival(estimated_neighbor_asn, asn_diff_ewma);

    // Guarantee bin range
    if (interarrival_bin < 0)
        interarrival_bin = 0;
    if (interarrival_bin >= RL_ASL_B_INTERARRIVAL)
        interarrival_bin = RL_ASL_B_INTERARRIVAL - 1;

    LOG_INFO("Estimated neighbor ASN diff: %u, interarrival bin: %d (EWMA=%u)",
             estimated_neighbor_asn, interarrival_bin, asn_diff_ewma);

    // --- compute current state for decision ---
    int current_state = rl_asl_q_learning_get_state(interarrival_bin);
    if (current_state == -1)
    {
        // Cannot map to a valid state -> be conservative: listen
        *skip_rx = RL_ASL_ACTION_DO_NOT_SKIP_RX;
        return;
    }

    // --- if we have a previous decision stored, update Q using expected reward now ---
    if (rl_asl_q_table.state >= 0 && rl_asl_q_table.action >= 0 &&
        rl_asl_q_table.state < RL_ASL_NUM_STATES && rl_asl_q_table.action < RL_ASL_NUM_ACTIONS)
    {

        // compute probability p (uses asn_diff_ewma and estimated_neighbor_asn)
        float p = rl_asl_compute_p(asn_diff_ewma, estimated_neighbor_asn);

        // expected reward for previous action
        float expected_reward = rl_asl_expected_reward_for_action(rl_asl_q_table.action, p);

        // update Q table: (prev_state, prev_action) => current_state as next_state
        rl_asl_q_learning_update(rl_asl_q_table.state,
                                 rl_asl_q_table.action,
                                 expected_reward,
                                 current_state);

        LOG_INFO("Exp-update: prev_state=%d prev_action=%d expected_r=%.3f -> next_state=%d",
                 rl_asl_q_table.state, rl_asl_q_table.action, expected_reward, current_state);
    }
    else
    {
        // No previous state/action to update (first decision). No-op.
        LOG_DBG("No prev decision to update (initialization).");
    }

    // --- choose action for this current_state ---
    int chosen_action = rl_asl_q_learning_select_action(current_state);
    rl_asl_q_table.state = current_state;
    rl_asl_q_table.action = chosen_action;

    *skip_rx = (chosen_action == RL_ASL_ACTION_SKIP_RX);
    LOG_INFO("Chosen action=%d for state=%d skip_rx=%d (epsilon=%.3f)",
             chosen_action, current_state, (int)*skip_rx, rl_asl_q_table.epsilon);

    // advance step / maybe end-of-episode
    rl_asl_q_learning_step_done();

    // Note: later, when you observe the real outcome for this ASN (did a packet arrive?),
    // call a separate function to apply a correction update using the actual reward.
    // You should store a tiny history keyed by ASN to match outcomes (see notes).
    return;
}
