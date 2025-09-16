#ifndef RL_ASL_DS_NBR_H_
#define RL_ASL_DS_NBR_H_

#include "contiki.h"
#include "net/linkaddr.h"
#include "net/nbr-table.h"
#include "lib/list.h"

/* Max neighbor cache entries */
#ifdef RL_ASL_DS_NBR_CONF_MAX_NEIGHBOR_CACHES
#define RL_ASL_DS_NBR_MAX_NEIGHBOR_CACHES RL_ASL_DS_NBR_CONF_MAX_NEIGHBOR_CACHES
#else
#define RL_ASL_DS_NBR_MAX_NEIGHBOR_CACHES (NBR_TABLE_MAX_NEIGHBORS * 2)
#endif

#ifdef RL_ASL_DS_NBR_CONF_MAX_NEIGHBOR_HISTORY
#define RL_ASL_DS_NBR_MAX_NEIGHBOR_HISTORY RL_ASL_DS_NBR_CONF_MAX_NEIGHBOR_HISTORY
#else
#define RL_ASL_DS_NBR_MAX_NEIGHBOR_HISTORY 15 // Default max history per neighbor
#endif

#ifdef RL_ASL_BROADCAST_SCHEDULE_CONF_INTERVAL
#define RL_ASL_BROADCAST_SCHEDULE_INTERVAL RL_ASL_BROADCAST_SCHEDULE_CONF_INTERVAL
#else
#define RL_ASL_BROADCAST_SCHEDULE_INTERVAL 180 // Default interval for broadcasting schedule
#endif                                       /* RL_ASL_BROADCAST_SCHEDULE_CONF_INTERVAL */

#ifdef RL_ASL_DATA_PACKET_GENERATOR_CONF_TX_INTERVAL_S
#define RL_ASL_DATA_PACKET_GENERATOR_TX_INTERVAL_S RL_ASL_DATA_PACKET_GENERATOR_CONF_TX_INTERVAL_S
#else
#define RL_ASL_DATA_PACKET_GENERATOR_TX_INTERVAL_S 13 // Default transmission interval in seconds
#endif                                              /* RL_ASL_DATA_PACKET_GENERATOR_CONF_TX_INTERVAL_S */

#ifdef RL_ASL_DS_NBR_CONF_MAX_NEIGHBOR_FUTURE_PREDICTIONS
#define RL_ASL_DS_NBR_MAX_NEIGHBOR_FUTURE_PREDICTIONS RL_ASL_DS_NBR_CONF_MAX_NEIGHBOR_FUTURE_PREDICTIONS
#else
#define RL_ASL_DS_NBR_MAX_NEIGHBOR_FUTURE_PREDICTIONS ((int)(RL_ASL_BROADCAST_SCHEDULE_INTERVAL / RL_ASL_DATA_PACKET_GENERATOR_TX_INTERVAL_S)) // Default max future predictions
#endif

typedef struct rl_asl_ds_nbr_flow_list
{
    struct rl_asl_ds_nbr_flow_list *next;
    uint8_t flow_id;
    int16_t seqnum;
    uint64_t generation_time_offset_asn; // ASN at the time of generation
    uint64_t tx_asn_history[RL_ASL_DS_NBR_MAX_NEIGHBOR_HISTORY];
    uint64_t future_predictions[RL_ASL_DS_NBR_MAX_NEIGHBOR_FUTURE_PREDICTIONS]; // Placeholder for future use
    int tx_asn_history_len;
    int tx_asn_history_index;
    uint8_t timeslots[10]; // Network of depth 10, can be adjusted
} rl_asl_ds_nbr_flow_list_t;

typedef struct rl_asl_ds_nbr
{
    struct rl_asl_ds_nbr *next;
    linkaddr_t addr;
    int16_t rssi;           // RSSI is neighbor-wide
    LIST_STRUCT(flow_list); // List of flows for this neighbor
} rl_asl_ds_nbr_t;

void rl_asl_ds_nbr_init(void);
rl_asl_ds_nbr_t *rl_asl_ds_nbr_add(const linkaddr_t *addr, int16_t rssi,
                                    uint8_t flow_id, int16_t seqnum,
                                    uint64_t generation_time_offset_asn,
                                    uint8_t *timeslots, int8_t timeslot_count);
rl_asl_ds_nbr_t *rl_asl_ds_nbr_lookup(const linkaddr_t *addr);
rl_asl_ds_nbr_flow_list_t *rl_asl_ds_nbr_lookup_flow(const linkaddr_t *addr, uint8_t flow_id);
rl_asl_ds_nbr_t *rl_asl_ds_nbr_get_head(void);
rl_asl_ds_nbr_flow_list_t *rl_asl_ds_flow_head(const rl_asl_ds_nbr_t *nbr);
rl_asl_ds_nbr_t *rl_asl_ds_nbr_get_next(rl_asl_ds_nbr_t *nbr);
rl_asl_ds_nbr_flow_list_t *rl_asl_ds_flow_next(rl_asl_ds_nbr_flow_list_t *flow);
int rl_asl_ds_nbr_flow_has_future_predictions(const rl_asl_ds_nbr_t *nbr, uint8_t flow_id);
uint64_t *rl_asl_ds_nbr_get_flow_future_predictions(const rl_asl_ds_nbr_t *nbr, uint8_t flow_id);
void predict_future_value(uint64_t *buffer, uint64_t *predictions,
                          int *len, int *index, int num_predictions);
void rl_asl_ds_nbr_print(void);
void rl_asl_ds_flows_print(list_t flows);
void rl_asl_ds_flow_print(rl_asl_ds_nbr_flow_list_t *flow);

#endif /* RL_ASL_DS_NBR_H_ */
