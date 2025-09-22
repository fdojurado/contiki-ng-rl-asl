#ifndef RL_ASL_DS_NBR_H_
#define RL_ASL_DS_NBR_H_

#include "contiki.h"
#include "net/linkaddr.h"
#include "net/nbr-table.h"

typedef struct
{
    uint32_t last_seqno;
    uint32_t first_seqno;
    uint8_t is_child;        // 1 if this neighbor is a child, 0 otherwise
    uint64_t last_heard_asn; // Last ASN when a packet was heard from this neighbor
    // EWMA of the time difference between the received packet's ASN and the last_heard_asn
    // This can be used to estimate the neighbor's current ASN
    uint32_t asn_diff_ewma;
    uint32_t asn_diff_var_ewma;
} rl_asl_ds_nbr_t;

void rl_asl_ds_nbr_init(void);
rl_asl_ds_nbr_t *rl_asl_ds_nbr_get(const linkaddr_t *addr);
const linkaddr_t *rl_asl_ds_nbr_get_addr(rl_asl_ds_nbr_t *nbr);
void rl_asl_ds_nbr_update(const linkaddr_t *addr, const uint32_t seqno, const uint64_t asn, const int8_t is_child);
void rl_asl_ds_nbr_remove(const linkaddr_t *addr);
rl_asl_ds_nbr_t *rl_asl_ds_nbr_get_any(void);
int rl_asl_ds_nbr_is_nbr_paired(const linkaddr_t *addr);
bool rl_asl_ds_nbr_is_there_a_non_paired_child(void);
rl_asl_ds_nbr_t *rl_asl_ds_nbr_head(void);
rl_asl_ds_nbr_t *rl_asl_ds_nbr_next(rl_asl_ds_nbr_t *nbr);
int rl_asl_ds_nbr_child_count(void);
int rl_asl_ds_nbr_count(void);
void rl_asl_ds_nbr_print(void);

#endif /* RL_ASL_DS_NBR_H_ */
