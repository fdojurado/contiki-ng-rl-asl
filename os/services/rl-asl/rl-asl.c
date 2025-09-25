#include "rl-asl.h"
#include <stdlib.h>
#include "rl-asl-conf.h"
#include "rl-asl-q-learning.h"
#include "rl-asl-ds-nbr.h"
#include "rl-asl-decision-buffer.h"
#include "os/services/orchestra/orchestra.h"
#include <stdint.h>
#include <float.h> /* Ensure FLT_MAX is available */

#if CONTIKI_TARGET_SKY
/* ---------------- Fixed-point math replacements ---------------- */
typedef int32_t q16_t;
#define Q16_SHIFT 16
#define Q16_ONE (1L << Q16_SHIFT)
#define TO_Q16(x) ((q16_t)((x) * Q16_ONE))
#define FROM_Q16(x) ((float)(x) / (float)Q16_ONE)

static q16_t fxp_mul(q16_t a, q16_t b)
{
    return (q16_t)(((int64_t)a * (int64_t)b) >> Q16_SHIFT);
}
static q16_t fxp_div(q16_t a, q16_t b)
{
    return (q16_t)(((int64_t)a << Q16_SHIFT) / b);
}

/* sqrtf approximation using Newton-Raphson */
static float sqrtf(float x)
{
    if (x <= 0)
    {
        return 0;
    }
    q16_t v = TO_Q16(x);
    q16_t r = v;
    for (int i = 0; i < 6; i++)
    {
        r = (r + fxp_div(v, r)) >> 1;
    }
    return FROM_Q16(r);
}

/* expf approximation: 1 + x + x^2/2 + x^3/6 */
static float expf(float x)
{
    q16_t xv = TO_Q16(x);
    q16_t sum = Q16_ONE;
    q16_t num = xv;

    sum += num; /* +x */
    num = fxp_mul(num, xv);
    sum += fxp_div(num, TO_Q16(2)); /* +x^2/2 */
    num = fxp_mul(num, xv);
    sum += fxp_div(num, TO_Q16(6)); /* +x^3/6 */

    return FROM_Q16(sum);
}

/* fmodf approximation */
static float fmodf(float a, float b)
{
    int div = (int)(a / b);
    return a - div * b;
}

/* fminf / fmaxf */
static float fminf(float a, float b) { return (a < b) ? a : b; }
static float fmaxf(float a, float b) { return (a > b) ? a : b; }

#else
#include <math.h>
#endif

/* log */
#include "sys/log.h"
#define LOG_MODULE "rl-asl"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL

#define RL_ASL_INVALID_EXPECTED_REWARD -1000.0f

/* Tunables for the "nearest neighbor" checks */
#define SIGMA_FRACTION 0.05f
#define MIN_SIGMA 1.0f
#define MISSED_SIGMA_MULTIPLIER 1.0f

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

/* ---- compute p using distance-to-nearest ---- */
static float rl_asl_compute_p(const uint64_t *curr_asn64)
{
    float prod_no_tx = 1.0f;

    for (rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_head();
         nbr != NULL;
         nbr = rl_asl_ds_nbr_next(nbr))
    {

        if (!(nbr->is_child) || nbr->last_heard_asn == 0 || nbr->asn_diff_ewma == 0)
            continue;

        uint64_t ref_asn = (nbr->last_expected_asn > nbr->last_heard_asn) ? nbr->last_expected_asn : nbr->last_heard_asn;
        uint64_t elapsed64 = (*curr_asn64 >= ref_asn) ? (*curr_asn64 - ref_asn) : 0;
        uint32_t elapsed_asn = (elapsed64 > UINT32_MAX) ? UINT32_MAX : (uint32_t)elapsed64;

        float lambda = (float)nbr->asn_diff_ewma;
        if (!(lambda > 0.0f))
            continue;

        float var = (float)nbr->asn_diff_var_ewma;
        float sigma = sqrtf(var);

        float sigma_floor = fmaxf(MIN_SIGMA, SIGMA_FRACTION * lambda);
        if (sigma < sigma_floor)
            sigma = sigma_floor;
        sigma = fminf(sigma, 0.5f * lambda);

        float phase = fmodf((float)elapsed_asn, lambda);
        float dist_nearest = fminf(phase, lambda - phase);

        float exponent = -0.5f * (dist_nearest * dist_nearest) / (sigma * sigma);
        float p_tx = expf(exponent);

        p_tx = clampf_local(p_tx, 0.0f, 1.0f);
        prod_no_tx *= (1.0f - p_tx);

        LOG_DBG("Neighbor %02x:%02x elapsed=%" PRIu32 " λ=%.1f var=%.1f sigma=%.2f phase=%.1f dist_nearest=%.1f p_tx=%.3f\n",
                rl_asl_ds_nbr_get_addr(nbr)->u8[0],
                rl_asl_ds_nbr_get_addr(nbr)->u8[1],
                elapsed_asn,
                (double)lambda,
                (double)var,
                (double)sigma,
                (double)phase,
                (double)dist_nearest,
                (double)p_tx);
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

/* Outcome handling */
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
        actual_reward += REWARD_SUCCESS;
    }

    LOG_INFO("TRACE_OUTCOME,ASN=%" PRIu32 ",ACTION=LISTEN,SUCCESS=%d\n",
             asn_low32, packet_received ? 1 : 0);

    int next_state = prev_state;
    rl_asl_q_learning_update(prev_state, prev_action, actual_reward, next_state);
    rl_asl_q_learning_step_done();
}

/* Decision function */
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
    float best_dist_nearest = FLT_MAX;
    int near_count = 0;

    for (rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_head();
         nbr != NULL && nb_count < RL_ASL_MAX_NEIGHBORS;
         nbr = rl_asl_ds_nbr_next(nbr))
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

        float lambda = (float)nbr->asn_diff_ewma;
        float sigma = sqrtf((float)nbr->asn_diff_var_ewma);
        float sigma_floor = fmaxf(MIN_SIGMA, SIGMA_FRACTION * lambda);
        sigma = fmaxf(sigma, sigma_floor);
        sigma = fminf(sigma, 0.5f * lambda);

        float phase = fmodf((float)estimated_neighbor_asn, lambda);
        float dist = fminf(phase, lambda - phase);

        if (dist < best_dist_nearest)
            best_dist_nearest = dist;
        if (dist <= sigma)
            near_count++;
    }

    if (nb_count == 0)
    {
        *skip_rx = false;
        return;
    }

    int dist_nearest_bin = rl_asl_q_bin_dist_nearest(best_dist_nearest);
    if (near_count > RL_ASL_NEAR_COUNT_MAX)
        near_count = RL_ASL_NEAR_COUNT_MAX;

    int aggregated_state =
        rl_asl_q_learning_get_aggregated_state_from_bins(bins, nb_count, dist_nearest_bin, near_count);
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

    LOG_INFO("TRACE_ACTION,ASN=%" PRIu32 ",ACTION=%s\n",
             asn_low32, (chosen_action == RL_ASL_ACTION_SKIP_RX ? "SKIP" : "LISTEN"));

    if (chosen_action == RL_ASL_ACTION_SKIP_RX)
    {
        float p = rl_asl_compute_p(&curr_asn64);
        float expected_reward = rl_asl_expected_reward_for_action(chosen_action, p);
        int pkt = 0;
        int near_c = 0;
        int terminal_c = 0;

        for (rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_head();
             nbr != NULL;
             nbr = rl_asl_ds_nbr_next(nbr))
        {

            if (!(nbr->is_child) || nbr->last_heard_asn == 0 || nbr->asn_diff_ewma == 0)
                continue;

            uint64_t ref_asn = (nbr->last_expected_asn > nbr->last_heard_asn) ? nbr->last_expected_asn : nbr->last_heard_asn;
            uint64_t elapsed64 = (curr_asn64 >= ref_asn) ? (curr_asn64 - ref_asn) : 0;
            uint32_t elapsed_asn = (elapsed64 > UINT32_MAX) ? UINT32_MAX : (uint32_t)elapsed64;

            float lambda = (float)nbr->asn_diff_ewma;
            if (!(lambda > 0.0f))
                continue;

            float sigma = sqrtf((float)nbr->asn_diff_var_ewma);
            float sigma_floor = fmaxf(MIN_SIGMA, SIGMA_FRACTION * lambda);
            sigma = fmaxf(sigma, sigma_floor);
            sigma = fminf(sigma, 0.5f * lambda);

            float phase = fmodf((float)elapsed_asn, lambda);
            float dist_to_next = lambda - phase;
            float dist_nearest = fminf(phase, dist_to_next);

            float missed_threshold = lambda + MISSED_SIGMA_MULTIPLIER * sigma;
            if ((float)elapsed_asn >= missed_threshold)
            {
                terminal_c++;
                pkt = 1;
                uint64_t delta = (uint64_t)(lambda + 0.5f);
                nbr->last_expected_asn += delta;
                if (nbr->predicted_skips < 255)
                    nbr->predicted_skips++;
                LOG_DBG("Neighbor %02x:%02x missed\n",
                        rl_asl_ds_nbr_get_addr(nbr)->u8[0],
                        rl_asl_ds_nbr_get_addr(nbr)->u8[1]);
            }
            else
            {
                if (dist_nearest <= sigma)
                {
                    near_c++;
                    pkt = 1;
                }
            }
        }

        if (near_c > 0)
            expected_reward += (float)near_c * PENALTY_SKIP_RX_TX;
        if (terminal_c > 0)
            expected_reward += (float)terminal_c * PENALTY_FAILURE;

        LOG_INFO("TRACE_OUTCOME,ASN=%" PRIu32 ",ACTION=SKIP,SUCCESS=%d\n",
                 asn_low32, pkt ? 0 : 1);

        rl_asl_q_learning_update(aggregated_state, chosen_action, expected_reward, aggregated_state);
        rl_asl_q_learning_step_done();
    }

    *skip_rx = (chosen_action == RL_ASL_ACTION_SKIP_RX);
}
