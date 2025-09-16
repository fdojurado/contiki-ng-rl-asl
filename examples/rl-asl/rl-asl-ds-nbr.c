#include "rl-asl-ds-nbr.h"
#include "lib/memb.h"
#include "string.h"
#include "math.h"
#include "os/sys/log.h"

#define LOG_MODULE "rl-asl-ds-nbr"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_DS_NBR

#ifdef RL_ASL_DS_NBR_CONF_MAX_FLOWS_PER_NBR
#define RL_ASL_DS_NBR_MAX_FLOWS_PER_NBR RL_ASL_DS_NBR_CONF_MAX_FLOWS_PER_NBR
#else
#define RL_ASL_DS_NBR_MAX_FLOWS_PER_NBR 10 // Default max flows per neighbor
#endif

LIST(rl_asl_ds_nbr_list);
MEMB(rl_asl_ds_nbr_memb, rl_asl_ds_nbr_t, RL_ASL_DS_NBR_MAX_NEIGHBOR_CACHES);

MEMB(rl_asl_ds_nbr_flows_memb, rl_asl_ds_nbr_flow_list_t, RL_ASL_DS_NBR_MAX_FLOWS_PER_NBR);

static int num_flows = 0;
/*---------------------------------------------------------------------------*/
void rl_asl_ds_nbr_init(void)
{
    LOG_INFO("Initializing RL-ASL neighbor cache\n");
    memb_init(&rl_asl_ds_nbr_memb);
    list_init(rl_asl_ds_nbr_list);
}
/*---------------------------------------------------------------------------*/
void predict_future_value(uint64_t *buffer, uint64_t *predictions,
                          int *len, int *index, int num_predictions)
{
    if (num_predictions <= 0 || num_predictions > RL_ASL_DS_NBR_MAX_NEIGHBOR_FUTURE_PREDICTIONS)
    {
        LOG_ERR("Invalid number of predictions requested: %d\n", num_predictions);
        return;
    }

    if (*len < 2)
    {
        for (int i = 0; i < RL_ASL_DS_NBR_MAX_NEIGHBOR_FUTURE_PREDICTIONS; i++)
            predictions[i] = 0;

        LOG_WARN("Not enough data to predict future values, returning zeros\n");

        return;
    }

    // Step 1: Calculate means
    double sum_x = 0, sum_y = 0;
    for (int i = 0; i < *len; i++)
    {
        int idx = i % RL_ASL_DS_NBR_MAX_NEIGHBOR_HISTORY; // Wraparound index
        sum_x += i;
        sum_y += (double)buffer[idx];
    }

    double mean_x = sum_x / *len;
    double mean_y = sum_y / *len;

    // Step 2: Calculate slope (a) and intercept (b)
    double numerator = 0, denominator = 0;
    for (int i = 0; i < *len; i++)
    {
        double xi = i;
        int idx = i % RL_ASL_DS_NBR_MAX_NEIGHBOR_HISTORY; // Wraparound index
        double yi = (double)buffer[idx];
        numerator += (xi - mean_x) * (yi - mean_y);
        denominator += (xi - mean_x) * (xi - mean_x);
    }

    double a = numerator / denominator;
    double b = mean_y - a * mean_x;

    // Step 3: Make predictions
    for (int i = 0; i < num_predictions; i++)
    {
        predictions[i] = (uint64_t)(a * (*len + i) + b);
    }
    // print the predictions for debugging
    // LOG_DBG("Predictions for next %d samples: ", num_predictions);
    // for (int i = 0; i < num_predictions; i++)
    // {
    //     LOG_DBG_("%d: %" PRIu64 "\n", i, predictions[i]);
    // }
    // LOG_DBG("\n");
}
/*---------------------------------------------------------------------------*/
static void add_to_circular_buffer(uint64_t *buffer, uint8_t *timeslots, int size,
                                   uint64_t generation_time_offset_asn, int *len, int *index)
{
    /* LOG_INFO("Adding value to circular buffer: %llu\n", generation_time_offset_asn);
    // The real generation_time_offset_asn is the ASN at the time of generation
    //  Plus the delay of reception at the previous node.

    struct tsch_slotframe *sf = tsch_schedule_get_slotframe_by_handle(1);
    int ts_at_generation = fmod(generation_time_offset_asn, sf->size.val);
    LOG_DBG("Generation ASN %llu corresponds to timeslot %d\n",
            generation_time_offset_asn, ts_at_generation);
    int num_slots = 0;
    for (int i = 0; i < size; i++)
    {
        if (timeslots[i] != 128) // 128 indicates no timeslot
        {
            if (ts_at_generation <= timeslots[i])
                num_slots += (timeslots[i] - ts_at_generation);
            else
                num_slots += (sf->size.val - ts_at_generation + timeslots[i]);
            generation_time_offset_asn += num_slots;
            ts_at_generation = fmod(generation_time_offset_asn, sf->size.val);
        }
    }
    LOG_DBG("Number of slots from generation ASN %llu to current ASN: %d (adjusted ASN: %llu)\n",
            generation_time_offset_asn, num_slots, generation_time_offset_asn); */

    buffer[*index] = generation_time_offset_asn;              // Add new value at current index
    *index = (*index + 1) % RL_ASL_DS_NBR_MAX_NEIGHBOR_HISTORY; // Wrap around index
    if (*len < RL_ASL_DS_NBR_MAX_NEIGHBOR_HISTORY)
    {
        (*len)++; // Increase length if we haven't filled the buffer yet
    }

    // if (*len >= RL_ASL_DS_NBR_MAX_NEIGHBOR_HISTORY)
    // {
    //     predict_future_value(buffer, future_predictions, len, index);
    // }
}
/*---------------------------------------------------------------------------*/
rl_asl_ds_nbr_t *rl_asl_ds_nbr_add(const linkaddr_t *addr, int16_t rssi,
                               uint8_t flow_id, int16_t seqnum,
                               uint64_t generation_time_offset_asn,
                               uint8_t *timeslots, int8_t timeslot_count)
{
    if (addr == NULL)
    {
        LOG_ERR("Invalid address for neighbor\n");
        return NULL;
    }

    rl_asl_ds_nbr_flow_list_t *flow, *new_flow;
    rl_asl_ds_nbr_t *nbr;

    /* First make sure that we don't add a duplicate neighbor */
    nbr = rl_asl_ds_nbr_lookup(addr);
    if (nbr == NULL)
    {
        /* Allocate a new neighbor */
        nbr = memb_alloc(&rl_asl_ds_nbr_memb);
        if (nbr == NULL)
        {
            LOG_ERR("Failed to allocate memory for neighbor\n");
            return NULL;
        }
        linkaddr_copy(&nbr->addr, addr);
        nbr->rssi = rssi;
        list_add(rl_asl_ds_nbr_list, nbr);
        LIST_STRUCT_INIT(nbr, flow_list);
    }

    /* We make sure we dont over pass the number of flows */
    if (list_length(nbr->flow_list) < RL_ASL_DS_NBR_MAX_FLOWS_PER_NBR)
    {
        /* We make sure that we don't add dest node twice */
        flow = rl_asl_ds_nbr_lookup_flow(addr, flow_id);
        if (flow == NULL)
        {
            /* Allocate a new flow */
            new_flow = memb_alloc(&rl_asl_ds_nbr_flows_memb);
            if (new_flow == NULL)
            {
                LOG_ERR("Failed to allocate memory for flow\n");
                return NULL;
            }
            new_flow->flow_id = flow_id;
            new_flow->seqnum = seqnum;
            new_flow->generation_time_offset_asn = generation_time_offset_asn;
            new_flow->tx_asn_history_len = 0;
            new_flow->tx_asn_history_index = 0;
            memset(new_flow->tx_asn_history, 0, sizeof(new_flow->tx_asn_history));
            memset(new_flow->future_predictions, 0, sizeof(new_flow->future_predictions));
            // Fill all timeslots with 128 (indicating no timeslot)
            memset(new_flow->timeslots, 128, sizeof(new_flow->timeslots));
            if (timeslot_count > sizeof(new_flow->timeslots))
                LOG_ERR("Too many timeslots provided, truncating to %zu\n", sizeof(new_flow->timeslots));
            memcpy(new_flow->timeslots, timeslots, timeslot_count);
            // Add generation_time_offset_asn to circular buffer
            add_to_circular_buffer(new_flow->tx_asn_history,
                                   new_flow->timeslots,
                                   sizeof(new_flow->timeslots),
                                   generation_time_offset_asn,
                                   &new_flow->tx_asn_history_len, &new_flow->tx_asn_history_index);

            list_add(nbr->flow_list, new_flow);
            num_flows++;
        }
        else
        {
            LOG_DBG("Flow %u already exists for neighbor %02x:%02x\n",
                    flow_id, addr->u8[0], addr->u8[1]);
            if (seqnum <= flow->seqnum)
            {
                LOG_DBG("Not updating flow %u for neighbor %02x:%02x, existing seqnum %d >= new seqnum %d\n",
                        flow_id, addr->u8[0], addr->u8[1], flow->seqnum, seqnum);
                return nbr; // Do not update if the new seqnum is less than the existing one
            }
            flow->seqnum = seqnum;
            flow->generation_time_offset_asn = generation_time_offset_asn;
            memset(flow->timeslots, 128, sizeof(flow->timeslots)); // Reset timeslots
            if (timeslot_count > sizeof(flow->timeslots))
                LOG_ERR("Too many timeslots provided, truncating to %zu\n", sizeof(flow->timeslots));
            memcpy(flow->timeslots, timeslots, timeslot_count);
            // Update the tx_asn_history with the new generation_time_offset_asn
            add_to_circular_buffer(flow->tx_asn_history,
                                   flow->timeslots,
                                   sizeof(flow->timeslots),
                                   generation_time_offset_asn,
                                   &flow->tx_asn_history_len, &flow->tx_asn_history_index);
        }
    }
    else
    {
        LOG_ERR("Cannot add flow %u for neighbor %02x:%02x, max flows reached\n",
                flow_id, addr->u8[0], addr->u8[1]);
        return NULL; // Max flows reached for this neighbor
    }
    LOG_INFO("Added neighbor %02x:%02x with RSSI %d, flow %u\n",
             addr->u8[0], addr->u8[1], rssi, flow_id);
    LOG_DBG("Neighbor %02x:%02x now has %d flows\n",
            addr->u8[0], addr->u8[1], list_length(nbr->flow_list));
    return nbr;
}
rl_asl_ds_nbr_t *rl_asl_ds_nbr_lookup(const linkaddr_t *addr)
{
    rl_asl_ds_nbr_t *found;
    rl_asl_ds_nbr_t *item;

    LOG_DBG("Looking up neighbor %02x:%02x\n", addr->u8[0], addr->u8[1]);

    if (addr == NULL)
    {
        return NULL;
    }

    found = NULL;

    for (item = rl_asl_ds_nbr_get_head(); item != NULL; item = rl_asl_ds_nbr_get_next(item))
    {
        if (linkaddr_cmp(&item->addr, addr))
        {
            found = item;
            break;
        }
    }

    if (found != NULL)
    {
        LOG_DBG("Found neighbor %02x:%02x with RSSI %d\n", found->addr.u8[0], found->addr.u8[1], found->rssi);
    }
    else
    {
        LOG_DBG("Neighbor %02x:%02x not found\n", addr->u8[0], addr->u8[1]);
    }

    return found;
}

rl_asl_ds_nbr_flow_list_t *rl_asl_ds_nbr_lookup_flow(const linkaddr_t *addr, uint8_t flow_id)
{
    rl_asl_ds_nbr_flow_list_t *flow;
    rl_asl_ds_nbr_flow_list_t *found;
    rl_asl_ds_nbr_t *item;

    LOG_DBG("Looking up flow %u for neighbor %02x:%02x\n", flow_id, addr->u8[0], addr->u8[1]);

    if (addr == NULL)
    {
        return NULL;
    }

    found = NULL;

    for (item = rl_asl_ds_nbr_get_head();
         item != NULL;
         item = rl_asl_ds_nbr_get_next(item))
    {
        if (linkaddr_cmp(&item->addr, addr))
        {
            for (flow = rl_asl_ds_flow_head(item);
                 flow != NULL;
                 flow = rl_asl_ds_flow_next(flow))
            {
                if (flow->flow_id == flow_id)
                {
                    found = flow;
                    break;
                }
            }
        }
    }

    if (found != NULL)
    {
        LOG_DBG("Found flow %u for neighbor %02x:%02x\n", flow_id, addr->u8[0], addr->u8[1]);
    }
    else
    {
        LOG_DBG("Flow %u not found for neighbor %02x:%02x\n", flow_id, addr->u8[0], addr->u8[1]);
    }

    return found;
}

rl_asl_ds_nbr_t *rl_asl_ds_nbr_get_head(void)
{
    return list_head(rl_asl_ds_nbr_list);
}

rl_asl_ds_nbr_flow_list_t *rl_asl_ds_flow_head(const rl_asl_ds_nbr_t *nbr)
{
    if (nbr == NULL || nbr->flow_list == NULL)
    {
        return NULL; // No flows available
    }
    return list_head(nbr->flow_list);
}

rl_asl_ds_nbr_t *rl_asl_ds_nbr_get_next(rl_asl_ds_nbr_t *nbr)
{
    return list_item_next(nbr);
}

rl_asl_ds_nbr_flow_list_t *rl_asl_ds_flow_next(rl_asl_ds_nbr_flow_list_t *flow)
{
    return list_item_next(flow);
}

int rl_asl_ds_nbr_flow_has_future_predictions(const rl_asl_ds_nbr_t *nbr, uint8_t flow_id)
{
    rl_asl_ds_nbr_flow_list_t *flow = rl_asl_ds_nbr_lookup_flow(&nbr->addr, flow_id);
    if (flow != NULL)
    {
        for (int i = 0; i < RL_ASL_DS_NBR_MAX_NEIGHBOR_FUTURE_PREDICTIONS; i++)
        {
            if (flow->future_predictions[i] != 0)
            {
                return 1; // Future predictions exist
            }
        }
    }
    return 0; // No future predictions available
}

uint64_t *rl_asl_ds_nbr_get_flow_future_predictions(const rl_asl_ds_nbr_t *nbr, uint8_t flow_id)
{
    rl_asl_ds_nbr_flow_list_t *flow = rl_asl_ds_nbr_lookup_flow(&nbr->addr, flow_id);
    if (flow != NULL)
    {
        return flow->future_predictions;
    }
    return NULL;
}

void rl_asl_ds_nbr_print(void)
{
    LOG_INFO("Current neighbors in the cache:\n");
    rl_asl_ds_nbr_t *nbr;
    for (nbr = rl_asl_ds_nbr_get_head(); nbr != NULL; nbr = rl_asl_ds_nbr_get_next(nbr))
    {
        LOG_INFO("Neighbor %02x:%02x RSSI=%d, Flows: %d\n",
                 nbr->addr.u8[0], nbr->addr.u8[1], nbr->rssi, list_length(nbr->flow_list));
        if (nbr->flow_list)
        {
            rl_asl_ds_flows_print(nbr->flow_list);
        }
    }
}

void rl_asl_ds_flows_print(list_t flows)
{
    if (flows)
    {
        rl_asl_ds_nbr_flow_list_t *f;
        for (f = list_head(flows); f != NULL; f = list_item_next(f))
        {
            rl_asl_ds_flow_print(f);
        }
    }
}

void rl_asl_ds_flow_print(rl_asl_ds_nbr_flow_list_t *flow)
{
    if (flow)
    {
        LOG_INFO("Flow ID: %u\n", flow->flow_id);
        LOG_INFO(" Sequence Number: %d\n", flow->seqnum);
        LOG_INFO(" Generation Time Offset: %" PRIu64 " ASN\n", flow->generation_time_offset_asn);
        LOG_INFO(" Timeslots: ");
        for (int i = 0; i < (int)sizeof(flow->timeslots); i++)
        {
            if (flow->timeslots[i] != 128)
            {
                LOG_INFO_("%d, ", flow->timeslots[i]);
            }
        }
        LOG_INFO_("\n");
    }
}
