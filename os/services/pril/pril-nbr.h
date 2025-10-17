#ifndef PRIL_NBR_H
#define PRIL_NBR_H

#include "contiki.h"
#include "net/linkaddr.h"
#include "net/nbr-table.h"

typedef struct
{
    uint64_t sleep_ie_asn; // Last received PRIL Sleep information element (asn)
    uint64_t sleep_ie_asn_secondary;
    int16_t first_seqno;
    int16_t last_seqno;
    uint8_t timing_ie_s;       // Last received PRIL Generation period in seconds
    struct tsch_link *rx_link; // Pointer to the RX link in the dedicated PRIL slotframe
    struct tsch_link *tx_link; // Pointer to the TX link in the dedicated PRIL slotframe
    int8_t parent;             // Whether this neighbor is a parent (1) or not (0)
} pril_nbr_t;

void pril_nbr_init(void);
pril_nbr_t *pril_nbr_get(const linkaddr_t *addr);
pril_nbr_t *pril_nbr_get_by_rx_link(const struct tsch_link *link);
pril_nbr_t *pril_nbr_get_by_tx_link(const struct tsch_link *link);
const linkaddr_t *pril_nbr_get_addr(pril_nbr_t *nbr);
pril_nbr_t *pril_nbr_update(const linkaddr_t *addr, const int16_t seqno,
                            const uint64_t sleep_ie_asn, const uint8_t timing_ie_s, int8_t parent);
void pril_nbr_rx_link_set(pril_nbr_t *nbr, const struct tsch_link *link);
void pril_nbr_tx_link_set(pril_nbr_t *nbr, const struct tsch_link *link);
pril_nbr_t *pril_nbr_min_gen_period_neighbor(void);
void pril_nbr_remove(const linkaddr_t *addr);
bool pril_nbr_is_there_a_non_paired_child(void);
pril_nbr_t *pril_nbr_head(void);
pril_nbr_t *pril_nbr_next(pril_nbr_t *nbr);
int pril_nbr_count(void);
void pril_nbr_print(void);

#endif /* PRIL_NBR_H */
