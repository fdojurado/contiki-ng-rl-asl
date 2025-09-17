#ifndef RL_ASL_DS_NBR_H_
#define RL_ASL_DS_NBR_H_

#include "contiki.h"
#include "net/linkaddr.h"
#include "net/nbr-table.h"

typedef struct
{
    uint32_t last_seqno;
    uint64_t last_heard_asn; // Last ASN when a packet was heard from this neighbor
} rl_asl_ds_nbr_t;

void rl_asl_ds_nbr_init(void);
rl_asl_ds_nbr_t *rl_asl_ds_nbr_get(const linkaddr_t *addr);
const linkaddr_t *rl_asl_ds_nbr_get_addr(rl_asl_ds_nbr_t *nbr);
void rl_asl_ds_nbr_update(const linkaddr_t *addr, uint32_t seqno, uint64_t asn);
void rl_asl_ds_nbr_remove(const linkaddr_t *addr);
void rl_asl_ds_nbr_print(void);

#endif /* RL_ASL_DS_NBR_H_ */
