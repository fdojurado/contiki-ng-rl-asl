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

/* clamp helper */
static float clampf_local(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

/* ---- helper: find neighbor whose schedule is closest to *now* ---- */
static rl_asl_ds_nbr_t *rl_asl_nbr_closest_to_tx(const uint64_t *curr_asn64, float *dist_to_nearest)
{
    rl_asl_ds_nbr_t *best = NULL;
    float min_dist_nearest = FLT_MAX;

    for (rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_head(); nbr != NULL; nbr = rl_asl_ds_nbr_next(nbr))
    {
        if (!(nbr->is_child) || nbr->last_heard_asn == 0 || nbr->asn_diff_ewma == 0)
            continue;

        uint64_t elapsed64 = (*curr_asn64 >= nbr->last_heard_asn) ? (*curr_asn64 - nbr->last_heard_asn) : 0;
        float lambda = (float)nbr->asn_diff_ewma;
        if (!(lambda > 0.0f))
            continue;

        float phase = fmodf((float)elapsed64, lambda);     /* [0, lambda) */
        float dist_nearest = fminf(phase, lambda - phase); /* distance to _nearest_ multiple */

        if (dist_nearest < min_dist_nearest)
        {
            min_dist_nearest = dist_nearest;
            best = nbr;
            if (dist_to_nearest)
                *dist_to_nearest = min_dist_nearest;
        }
    }

    if (dist_to_nearest && best == NULL)
        *dist_to_nearest = 0.0f;
    return best;
}

/* ---- compute p using distance-to-nearest (so p is high before & after the expected moment) ---- */
static float rl_asl_compute_p(const uint64_t *curr_asn64)
{
    float prod_no_tx = 1.0f;

    for (rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_head(); nbr != NULL; nbr = rl_asl_ds_nbr_next(nbr))
    {
        if (!(nbr->is_child) || nbr->last_heard_asn == 0 || nbr->asn_diff_ewma == 0)
            continue;

        uint64_t elapsed64 = (*curr_asn64 >= nbr->last_heard_asn) ? (*curr_asn64 - nbr->last_heard_asn) : 0;
        uint32_t elapsed_asn = (elapsed64 > UINT32_MAX) ? UINT32_MAX : (uint32_t)elapsed64;

        float lambda = (float)nbr->asn_diff_ewma;
        if (!(lambda > 0.0f))
            continue;

        float var = (float)nbr->asn_diff_var_ewma; /* prefer storing float variance in nbr struct */
        float sigma = sqrtf(var);

        /* sensible floor & optionally cap */
        float sigma_floor = fmaxf(MIN_SIGMA, SIGMA_FRACTION * lambda);
        if (sigma < sigma_floor)
            sigma = sigma_floor;
        /* Optionally cap: sigma = fminf(sigma, 0.5f * lambda); */

        float phase = fmodf((float)elapsed_asn, lambda);   /* [0, lambda) */
        float dist_nearest = fminf(phase, lambda - phase); /* small when close to a multiple */

        float exponent = -0.5f * (dist_nearest * dist_nearest) / (sigma * sigma);
        float p_tx = expf(exponent);

        p_tx = clampf_local(p_tx, 0.0f, 1.0f);

        prod_no_tx *= (1.0f - p_tx);

        LOG_DBG("Neighbor %02x:%02x elapsed=%u λ=%.1f var=%.1f sigma=%.2f phase=%.1f dist_nearest=%.1f p_tx=%.3f\n",
                rl_asl_ds_nbr_get_addr(nbr)->u8[0], rl_asl_ds_nbr_get_addr(nbr)->u8[1],
                elapsed_asn, lambda, var, sigma, phase, dist_nearest, p_tx);
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

    // Defensive checks
    if (prev_state < 0 || prev_state >= RL_ASL_NUM_STATES ||
        prev_action < 0 || prev_action >= RL_ASL_NUM_ACTIONS)
    {
        LOG_ERR("rl_asl_on_slot_outcome: invalid stored decision state=%d action=%d\n",
                prev_state, prev_action);
        return;
    }

    // this function expects the stored action to have been LISTEN (we update SKIP earlier)
    if (prev_action == RL_ASL_ACTION_SKIP_RX)
    {
        LOG_WARN("rl_asl_on_slot_outcome: consumed SKIP entry unexpectedly (ignoring)\n");
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

    float sum_asn_diff_ewma = 0.0f;
    float sum_estimated_asn = 0.0f;

    uint64_t curr_asn64 = ((uint64_t)tsch_current_asn.ms1b << 32) | tsch_current_asn.ls4b;

    /* collect per-neighbor estimates for aggregated state encoding */
    for (rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_head(); nbr != NULL && nb_count < RL_ASL_MAX_NEIGHBORS; nbr = rl_asl_ds_nbr_next(nbr))
    {
        if (!(nbr->is_child) || nbr->last_heard_asn == 0 || nbr->asn_diff_ewma == 0)
            continue;

        uint64_t est_diff64 = (curr_asn64 >= nbr->last_heard_asn) ? (curr_asn64 - nbr->last_heard_asn) : 0;
        uint32_t estimated_neighbor_asn = (est_diff64 > UINT32_MAX) ? UINT32_MAX : (uint32_t)est_diff64;

        sum_estimated_asn += (float)estimated_neighbor_asn;
        sum_asn_diff_ewma += (float)nbr->asn_diff_ewma;

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

    float avg_asn_diff_ewma = sum_asn_diff_ewma / (float)nb_count;
    float avg_estimated_asn = sum_estimated_asn / (float)nb_count;
    uint32_t asn_diff_ewma = (uint32_t)avg_asn_diff_ewma;          /* aggregated for logging / fallback */
    uint32_t estimated_neighbor_asn = (uint32_t)avg_estimated_asn; /* aggregated fallback */

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
        /* baseline expected reward from our probabilistic model */
        float p = rl_asl_compute_p(&curr_asn64);
        float expected_reward = rl_asl_expected_reward_for_action(chosen_action, p);
        int pkt = 0;

        /* find the neighbor that is closest to its next expected TX (per-neighbor check) */
        float dist_to_nearest = 0.0f;
        rl_asl_ds_nbr_t *nbr_tx = rl_asl_nbr_closest_to_tx(&curr_asn64, &dist_to_nearest);
        if (nbr_tx != NULL)
        {
            LOG_DBG("Nearest neighbor %02x:%02x dist_to_nearest=%.1f slots\n",
                    rl_asl_ds_nbr_get_addr(nbr_tx)->u8[0],
                    rl_asl_ds_nbr_get_addr(nbr_tx)->u8[1],
                    dist_to_nearest);
            /* compute exact elapsed/lambda/sigma for this neighbor */
            uint64_t elapsed64 = (curr_asn64 >= nbr_tx->last_heard_asn) ? (curr_asn64 - nbr_tx->last_heard_asn) : 0;
            uint32_t elapsed_asn = (elapsed64 > UINT32_MAX) ? UINT32_MAX : (uint32_t)elapsed64;
            float lambda = (float)nbr_tx->asn_diff_ewma;
            float var = (float)nbr_tx->asn_diff_var_ewma;
            float sigma = sqrtf(var);
            float sigma_floor = fmaxf(MIN_SIGMA, SIGMA_FRACTION * lambda);
            if (sigma < sigma_floor)
                sigma = sigma_floor;

            /* If next TX is very soon (within NEAR_TX_WINDOW), skipping risks missing -> penalize */
            if (dist_to_nearest <= sigma)
            {
                expected_reward += PENALTY_SKIP_RX_TX;
                pkt = 1;
                LOG_DBG("Nearest neighbor %02x:%02x next TX within %d slots -> penalize skip\n",
                        rl_asl_ds_nbr_get_addr(nbr_tx)->u8[0],
                        rl_asl_ds_nbr_get_addr(nbr_tx)->u8[1],
                        NEAR_TX_WINDOW);
            }

            /* If the neighbor has been silent far beyond its expected period + margin => treat as failure (e.g. lost) */
            float missed_threshold = lambda + MISSED_SIGMA_MULTIPLIER * sigma;
            if ((float)elapsed_asn > missed_threshold)
            {
                rl_asl_q_learning_end_episode();
                expected_reward += PENALTY_FAILURE;
                pkt = 1;
                LOG_DBG("Nearest neighbor %02x:%02x elapsed=%u >= missed_threshold=%.1f -> terminal failure\n",
                        rl_asl_ds_nbr_get_addr(nbr_tx)->u8[0],
                        rl_asl_ds_nbr_get_addr(nbr_tx)->u8[1],
                        elapsed_asn,
                        missed_threshold);
            }
        }

        /* Important: avoid using aggregated 'estimated_neighbor_asn' vs aggregated avg_asn_diff_ewma for terminal check.
         * We already applied per-neighbor failure detection above; keep the old aggregate check as a fallback */
        if (estimated_neighbor_asn >= (asn_diff_ewma + ORCHESTRA_UNICAST_PERIOD * 7))
        {
            rl_asl_q_learning_end_episode();
            expected_reward += PENALTY_FAILURE;
            pkt = 1;
            LOG_DBG("Aggregate fallback: estimated_neighbor_asn %u >= %u -> terminal failure\n",
                    estimated_neighbor_asn, asn_diff_ewma + ORCHESTRA_UNICAST_PERIOD * 7);
        }

        LOG_INFO("TRACE_OUTCOME,ASN=%u,ACTION=SKIP,SUCCESS=%d\n",
                 asn_low32,
                 pkt ? 0 : 1);

        rl_asl_q_learning_update(aggregated_state, chosen_action, expected_reward, aggregated_state);
    }

    *skip_rx = (chosen_action == RL_ASL_ACTION_SKIP_RX);
    rl_asl_q_learning_step_done();
}
