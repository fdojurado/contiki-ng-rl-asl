#ifndef PRIL_NBR_H
#define PRIL_NBR_H

#include "contiki.h"
#include "net/linkaddr.h"
#include "net/mac/tsch/tsch.h"
#include "net/nbr-table.h"
#include <stdbool.h>
#include <inttypes.h>

/*---------------------------------------------*/
/* PRIL link state enumeration                 */
/*---------------------------------------------*/
typedef enum
{
    PRIL_STATE_ON = 0,
    PRIL_STATE_OFF,
    PRIL_STATE_RETR
} pril_state_t;

/*---------------------------------------------*/
/* PRIL neighbor entry (per link Ni→Nj)        */
/*---------------------------------------------*/
typedef struct pril_nbr
{
    /* TSCH linkage */
    struct tsch_link *rx_link; // RX cell for this neighbor (child -> me)
    struct tsch_link *tx_link; // TX cell for this neighbor (me -> parent)
    bool parent;               // true if this neighbor is my parent (uplink)

    /* Sequence tracking */
    int16_t first_seqno;
    int16_t last_seqno;

    /* --- PRIL state machine fields --- */
    pril_state_t tx_state;
    pril_state_t rx_state;
    int16_t sleep_end;                // main counter, number of scheduled cells
    int new_sleep_end;                // secondary counter (for queued Tmin)
    int retr_count;                   // retries done for last F.s packet
    int max_retries;                  // configured Ntries
    uint8_t timing_T_s;               // cached Tmin (seconds) for this link
    struct tsch_asn_t last_sleep_asn; // Last time we updated sleep_end

} pril_nbr_t;

/*---------------------------------------------*/
/* API declarations                            */
/*---------------------------------------------*/
void pril_nbr_init(void);

/* Neighbor table operations */
pril_nbr_t *pril_nbr_get(const linkaddr_t *addr);
pril_nbr_t *pril_nbr_add(const linkaddr_t *addr, int16_t seqnum, int16_t sleep_end, uint8_t timing_T_s, bool is_parent);
pril_nbr_t *pril_nbr_get_by_rx_link(const struct tsch_link *link);
pril_nbr_t *pril_nbr_get_by_tx_link(const struct tsch_link *link);
const linkaddr_t *pril_nbr_get_addr(pril_nbr_t *nbr);
void pril_nbr_rx_link_set(pril_nbr_t *nbr, const struct tsch_link *link);
void pril_nbr_tx_link_set(pril_nbr_t *nbr, const struct tsch_link *link);
void pril_nbr_rx_set_state(pril_nbr_t *nbr, pril_state_t state);
void pril_nbr_remove(const linkaddr_t *addr);
pril_nbr_t *pril_nbr_head(void);
pril_nbr_t *pril_nbr_next(pril_nbr_t *nbr);
int pril_nbr_count(void);
void pril_nbr_print(void);

/* Global helpers */
pril_nbr_t *pril_nbr_min_gen_period_neighbor(void);
bool pril_nbr_is_there_a_non_paired_child(void);

#endif /* PRIL_NBR_H */
