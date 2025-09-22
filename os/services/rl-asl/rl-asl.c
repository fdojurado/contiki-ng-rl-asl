#include "rl-asl.h"
#include <stdlib.h>
#include "rl-asl-conf.h"
#include "rl-asl-q-learning.h"
#include "rl-asl-ds-nbr.h"
#include "rl-asl-decision-buffer.h"
#include "os/services/orchestra/orchestra.h"
#include <math.h>  // expf(), sqrtf(), fmodf(), ceilf()
#include <float.h> // FLT_MAX
#include <stdint.h>

/* log */
#include "sys/log.h"
#define LOG_MODULE "rl-asl"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL

#define RL_ASL_INVALID_EXPECTED_REWARD -1000.0f

/* Tunables for the "nearest neighbor" checks */
#define SIGMA_FRACTION 0.08f                      /* fallback fraction of lambda to use as sigma (0.05-0.2 typical) */
#define MIN_SIGMA 1.0f                            /* minimum sigma (in slots) to avoid divide-by-zero */
#define MISSED_SIGMA_MULTIPLIER 1.0f              /* elapsed > lambda + MISSED_SIGMA_MULTIPLIER * sigma => treat as missed/failure */
#define NEAR_TX_WINDOW (ORCHESTRA_UNICAST_PERIOD) /* if next tx within this many slots, skipping likely misses it */

#define NEAR_MULT 1.0f
#define MAX_NEAR_COUNT 6

/* clamp helper */
static float clampf_local(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

/* ---- compute p using distance-to-nearest (so p is high before & after the expected moment) ---- */
static float rl_asl_compute_p(const uint64_t *curr_asn64)
{
    float prod_no_tx = 1.0f;

    for (rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_head(); nbr != NULL; nbr = rl_asl_ds_nbr_next(nbr))
    {
        if (!(nbr->is_child) || nbr->last_heard_asn == 0 || nbr->asn_diff_ewma == 0)
            continue;

        /* use last_expected_asn if we advanced predictions after misses */
        uint64_t ref_asn = (nbr->last_expected_asn > nbr->last_heard_asn) ? nbr->last_expected_asn : nbr->last_heard_asn;
        uint64_t elapsed64 = (*curr_asn64 >= ref_asn) ? (*curr_asn64 - ref_asn) : 0;
        uint32_t elapsed_asn = (elapsed64 > UINT32_MAX) ? UINT32_MAX : (uint32_t)elapsed64;

        float lambda = (float)nbr->asn_diff_ewma;
        if (!(lambda > 0.0f))
            continue;

        /* variance -> sigma, be robust to zero */
        float var = (float)nbr->asn_diff_var_ewma; /* prefer storing float variance in nbr struct */
        float sigma = sqrtf(var);

        /* floor and cap sigma to avoid degenerate gaussians */
        float sigma_floor = fmaxf(MIN_SIGMA, SIGMA_FRACTION * lambda);
        if (sigma < sigma_floor)
            sigma = sigma_floor;
        sigma = fminf(sigma, 0.5f * lambda);

        float phase = fmodf((float)elapsed_asn, lambda);   /* [0, lambda) */
        float dist_nearest = fminf(phase, lambda - phase); /* small when close to a multiple */

        /* gaussian of distance-to-nearest -> gives bump before & after expected moment */
        float exponent = -0.5f * (dist_nearest * dist_nearest) / (sigma * sigma);
        float p_tx = expf(exponent);

        p_tx = clampf_local(p_tx, 0.0f, 1.0f);
        prod_no_tx *= (1.0f - p_tx);

        LOG_DBG("Neighbor %02x:%02x elapsed=%u λ=%.1f var=%.1f sigma=%.2f phase=%.1f dist_nearest=%.1f p_tx=%.3f\n",
                rl_asl_ds_nbr_get_addr(nbr)->u8[0],
                rl_asl_ds_nbr_get_addr(nbr)->u8[1],
                elapsed_asn,
                lambda,
                var,
                sigma,
                phase,
                dist_nearest,
                p_tx);
    }

    float p_any = 1.0f - prod_no_tx;
    return clampf_local(p_any, 0.001f, 0.99f);
}

static float rl_asl_expected_reward_for_action(int action, float p)
{
    if (action == RL_ASL_ACTION_SKIP_RX)
    {
        return p * PENALTY_SKIP_RX_TX + (1.0f - p) * REWARD_SKIP_RX_NO_TX;
    }
    return RL_ASL_INVALID_EXPECTED_REWARD;
}

/* Called when a listen slot finished and a packet was (or wasn't) received.
 * This only handles outcomes for LISTEN actions (the SKIP action is handled
 * in rl_asl_check_skip_rx when it occurs). */
void rl_asl_on_slot_outcome(uint32_t asn_low32, bool packet_received)
{
    int prev_state, prev_action;
    if (!rl_asl_decision_buffer_consume(asn_low32, &prev_state, &prev_action))
    {
        return;
    }

    if (prev_state < 0 || prev_state >= RL_ASL_NUM_STATES ||
        prev_action < 0 || prev_action >= RL_ASL_NUM_ACTIONS)
    {
        LOG_ERR("rl_asl_on_slot_outcome: invalid state=%d action=%d\n",
                prev_state, prev_action);
        return;
    }

    if (prev_action == RL_ASL_ACTION_SKIP_RX)
    {
        LOG_WARN("rl_asl_on_slot_outcome: consumed SKIP entry unexpectedly\n");
        return;
    }

    float actual_reward = packet_received ? REWARD_RX_TX : PENALTY_RX_NO_TX;

    if (packet_received)
    {
        actual_reward += REWARD_SUCCESS; // success bonus inside horizon
    }

    LOG_INFO("TRACE_OUTCOME,ASN=%u,ACTION=LISTEN,SUCCESS=%d\n",
             asn_low32, packet_received ? 1 : 0);

    int next_state = prev_state;
    rl_asl_q_learning_update(prev_state, prev_action, actual_reward, next_state);

    rl_asl_q_learning_step_done();
}

/* Main decision function: choose to skip or listen. If SKIP, compute expected reward
 * and apply additional heuristics based on the neighbor nearest to its next TX. */
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

    uint64_t curr_asn64 = ((uint64_t)tsch_current_asn.ms1b << 32) | tsch_current_asn.ls4b;

    /* collect per-neighbor estimates for aggregated state encoding */
    for (rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_head(); nbr != NULL && nb_count < RL_ASL_MAX_NEIGHBORS; nbr = rl_asl_ds_nbr_next(nbr))
    {
        if (!(nbr->is_child) || nbr->last_heard_asn == 0 || nbr->asn_diff_ewma == 0)
            continue;

        uint64_t est_diff64 = (curr_asn64 >= nbr->last_heard_asn) ? (curr_asn64 - nbr->last_heard_asn) : 0;
        uint32_t estimated_neighbor_asn = (est_diff64 > UINT32_MAX) ? UINT32_MAX : (uint32_t)est_diff64;

        int interarrival_bin = rl_asl_q_bin_interarrival(estimated_neighbor_asn, nbr->asn_diff_ewma);
        if (interarrival_bin < 0)
            interarrival_bin = 0;
        if (interarrival_bin >= RL_ASL_B_INTERARRIVAL)
            interarrival_bin = RL_ASL_B_INTERARRIVAL - 1;

        bins[nb_count++] = interarrival_bin;
    }

    if (nb_count == 0)
    {
        *skip_rx = false;
        return;
    }

    /* Build aggregated state from bins (feature-engineered state) */
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

    /* Decision log (before outcome known) */
    LOG_INFO("TRACE_ACTION,ASN=%u,ACTION=%s\n",
             asn_low32,
             (chosen_action == RL_ASL_ACTION_SKIP_RX ? "SKIP" : "LISTEN"));

    if (chosen_action == RL_ASL_ACTION_SKIP_RX)
    {
        float p = rl_asl_compute_p(&curr_asn64);
        float expected_reward = rl_asl_expected_reward_for_action(chosen_action, p);
        int pkt = 0;
        bool any_terminal = false;
        int near_count = 0;

        for (rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_head(); nbr != NULL; nbr = rl_asl_ds_nbr_next(nbr))
        {
            if (!(nbr->is_child) || nbr->last_heard_asn == 0 || nbr->asn_diff_ewma == 0)
                continue;

            uint64_t ref_asn = (nbr->last_expected_asn > nbr->last_heard_asn) ? nbr->last_expected_asn : nbr->last_heard_asn;
            uint64_t elapsed64 = (curr_asn64 >= ref_asn) ? (curr_asn64 - ref_asn) : 0;
            uint32_t elapsed_asn = (elapsed64 > UINT32_MAX) ? UINT32_MAX : (uint32_t)elapsed64;

            float lambda = (float)nbr->asn_diff_ewma;
            if (!(lambda > 0.0f))
                continue;

            /* robust sigma */
            float var = (float)nbr->asn_diff_var_ewma;
            float sigma = sqrtf(var);
            float sigma_floor = fmaxf(MIN_SIGMA, SIGMA_FRACTION * lambda);
            sigma = fmaxf(sigma, sigma_floor);
            sigma = fminf(sigma, 0.5f * lambda);

            float phase = fmodf((float)elapsed_asn, lambda);
            float dist_to_next = lambda - phase;
            float dist_nearest = fminf(phase, dist_to_next);

            /* Missed check (terminal-like reward). Do NOT multiple-apply huge penalties;
               mark a single terminal occurrence and advance expectation to avoid drift. */
            float missed_threshold = lambda + MISSED_SIGMA_MULTIPLIER * sigma;
            if ((float)elapsed_asn >= missed_threshold)
            {
                any_terminal = true;
                pkt = 1;

                /* advance expectation to avoid drift and record predicted skip */
                uint64_t delta = (uint64_t)(lambda + 0.5f);
                nbr->last_expected_asn += delta;
                if (nbr->predicted_skips < 255)
                    nbr->predicted_skips++;

                LOG_DBG("Neighbor %02x:%02x missed: elapsed=%u >= %.1f -> terminal mark, last_expected_asn=%" PRIu64 "\n",
                        rl_asl_ds_nbr_get_addr(nbr)->u8[0],
                        rl_asl_ds_nbr_get_addr(nbr)->u8[1],
                        elapsed_asn, missed_threshold, nbr->last_expected_asn);
                /* continue loop: still count other near neighbors, but the terminal penalty will be applied once */
                continue;
            }

            /* Near check (before or after expected moment): use dist_nearest and a multiplier */
            if (dist_nearest <= NEAR_MULT * sigma)
            {
                near_count++;
                pkt = 1;
                LOG_DBG("Neighbor %02x:%02x near-TX: dist_nearest=%.1f <= %.1f (NEAR_MULT*sigma)\n",
                        rl_asl_ds_nbr_get_addr(nbr)->u8[0],
                        rl_asl_ds_nbr_get_addr(nbr)->u8[1],
                        dist_nearest, NEAR_MULT * sigma);
            }
        } /* for neighbors */

        /* Apply penalties: single terminal penalty if any terminal occurred.
           If multiple missed neighbors are realistically possible, you can scale mildly,
           but avoid exploding the penalty. */
        if (any_terminal)
        {
            expected_reward += PENALTY_FAILURE; /* single application */
            LOG_DBG("One or more neighbors marked missed -> apply single failure penalty\n");
        }

        /* Apply near penalties: linear up to cap */
        if (near_count > 0)
        {
            int effective_near = near_count;
            if (effective_near > MAX_NEAR_COUNT)
                effective_near = MAX_NEAR_COUNT;
            expected_reward += (float)effective_near * PENALTY_SKIP_RX_TX;
            if (near_count > 1)
            {
                LOG_DBG("Multiple (%d) neighbors near-TX -> near penalty applied (%d counted)\n", near_count, effective_near);
            }
        }

        LOG_INFO("TRACE_OUTCOME,ASN=%u,ACTION=SKIP,SUCCESS=%d\n", asn_low32, pkt ? 0 : 1);

        rl_asl_q_learning_update(aggregated_state, chosen_action, expected_reward, aggregated_state);
        rl_asl_q_learning_step_done();
    }

    *skip_rx = (chosen_action == RL_ASL_ACTION_SKIP_RX);
}
