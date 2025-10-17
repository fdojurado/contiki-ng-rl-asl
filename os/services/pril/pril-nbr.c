#include "pril-nbr.h"
#include "net/mac/tsch/tsch.h"

/* log */
#include "sys/log.h"
#define LOG_MODULE "pril-nbr"
#define LOG_LEVEL LOG_LEVEL_INFO

NBR_TABLE(pril_nbr_t, pril_nbr_table);

static int count_neighbors = 0;

static int16_t min_gen_period = INT16_MAX;
static pril_nbr_t *min_gen_period_nbr = NULL;

/***********************************************************************/
void pril_nbr_init(void)
{
    nbr_table_register(pril_nbr_table, NULL);
    LOG_INFO("PRIL neighbor table initialized\n");
}
/***********************************************************************/
pril_nbr_t *pril_nbr_get(const linkaddr_t *addr)
{
    return nbr_table_get_from_lladdr(pril_nbr_table, addr);
}
/***********************************************************************/
pril_nbr_t *pril_nbr_get_by_rx_link(const struct tsch_link *link)
{
    if (link == NULL)
        return NULL;

    pril_nbr_t *nbr = nbr_table_head(pril_nbr_table);
    while (nbr != NULL)
    {
        if (nbr->rx_link == NULL)
        {
            nbr = nbr_table_next(pril_nbr_table, nbr);
            continue;
        }

        if (nbr->rx_link == link)
            return nbr;

        nbr = nbr_table_next(pril_nbr_table, nbr);
    }
    return NULL;
}
/***********************************************************************/
pril_nbr_t *pril_nbr_get_by_tx_link(const struct tsch_link *link)
{
    if (link == NULL)
        return NULL;

    pril_nbr_t *nbr = nbr_table_head(pril_nbr_table);
    while (nbr != NULL)
    {
        if (nbr->tx_link == NULL)
        {
            nbr = nbr_table_next(pril_nbr_table, nbr);
            continue;
        }

        if (nbr->tx_link == link)
            return nbr;

        nbr = nbr_table_next(pril_nbr_table, nbr);
    }
    return NULL;
}
/***********************************************************************/
const linkaddr_t *pril_nbr_get_addr(pril_nbr_t *nbr)
{
    return nbr_table_get_lladdr(pril_nbr_table, nbr);
}
/***********************************************************************/
pril_nbr_t *pril_nbr_update(const linkaddr_t *addr, int16_t seqno,
                            uint64_t sleep_ie_asn, uint8_t timing_ie_s, int8_t parent)
{
    pril_nbr_t *nbr = pril_nbr_get(addr);

    if (nbr == NULL)
    {
        nbr = nbr_table_add_lladdr(pril_nbr_table, addr, NBR_TABLE_REASON_IPV6_ND, NULL);
        if (nbr == NULL)
        {
            LOG_ERR("Failed to add neighbor %02x:%02x to table\n", addr->u8[0], addr->u8[1]);
            return NULL;
        }
        nbr->first_seqno = seqno;
        nbr->last_seqno = seqno;
        nbr->sleep_ie_asn = sleep_ie_asn;
        nbr->sleep_ie_asn_secondary = -1; // Initialize secondary sleep_ie_asn
        nbr->timing_ie_s = timing_ie_s;
        nbr->rx_link = NULL;
        nbr->tx_link = NULL;
        nbr->parent = parent;
        count_neighbors++;
        LOG_INFO("Added neighbor %02x:%02x with seqno %d, sleep_ie_asn %" PRIu64 ", timing_ie_s %u\n",
                 addr->u8[0], addr->u8[1], seqno, sleep_ie_asn, timing_ie_s);
    }
    else
    {
        // Only update first_seqno if it is uninitialized
        if (nbr->first_seqno == -1 && seqno != -1)
        {
            nbr->first_seqno = seqno;
        }
        // Always update last_seqno if seqno is newer
        if (seqno > nbr->last_seqno)
        {
            nbr->last_seqno = seqno;
        }
        nbr->sleep_ie_asn = sleep_ie_asn;
        nbr->timing_ie_s = timing_ie_s;
        nbr->parent = parent;
        LOG_INFO("Updated neighbor %02x:%02x with seqno %d, sleep_ie_asn %" PRIu64 ", timing_ie_s %u\n",
                 addr->u8[0], addr->u8[1], seqno, sleep_ie_asn, timing_ie_s);
    }

    // Update min_gen_period and min_gen_period_nbr
    if (timing_ie_s > 0 && timing_ie_s < min_gen_period)
    {
        min_gen_period = timing_ie_s;
        min_gen_period_nbr = nbr;
    }

    return nbr;
}
/***********************************************************************/
void pril_nbr_rx_link_set(pril_nbr_t *nbr, const struct tsch_link *link)
{
    nbr->rx_link = (struct tsch_link *)link;
}
/***********************************************************************/
void pril_nbr_tx_link_set(pril_nbr_t *nbr, const struct tsch_link *link)
{
    nbr->tx_link = (struct tsch_link *)link;
}
/***********************************************************************/
pril_nbr_t *pril_nbr_min_gen_period_neighbor(void)
{
    return min_gen_period_nbr;
}
/***********************************************************************/
void pril_nbr_remove(const linkaddr_t *addr)
{
    pril_nbr_t *nbr = pril_nbr_get(addr);
    if (nbr != NULL)
    {
        nbr_table_remove(pril_nbr_table, nbr);
        LOG_INFO("Removed neighbor %02x:%02x from table\n", addr->u8[0], addr->u8[1]);
        count_neighbors--;
    }
    else
    {
        LOG_WARN("Neighbor %02x:%02x not found in table for removal\n", addr->u8[0], addr->u8[1]);
    }
}
/***********************************************************************/
bool pril_nbr_is_there_a_non_paired_child(void)
{
    pril_nbr_t *nbr = nbr_table_head(pril_nbr_table);
    while (nbr != NULL)
    {

        if (nbr->parent == 1)
        {
            nbr = nbr_table_next(pril_nbr_table, nbr);
            continue;
        }

        if (nbr->last_seqno - nbr->first_seqno < 3)
        {
            return true;
        }
        nbr = nbr_table_next(pril_nbr_table, nbr);
    }
    return false;
}
/***********************************************************************/
pril_nbr_t *pril_nbr_head(void)
{
    return nbr_table_head(pril_nbr_table);
}
/***********************************************************************/
pril_nbr_t *pril_nbr_next(pril_nbr_t *nbr)
{
    return nbr_table_next(pril_nbr_table, nbr);
}
/***********************************************************************/
int pril_nbr_count(void)
{
    return count_neighbors;
}
/***********************************************************************/
void pril_nbr_print(void)
{
    pril_nbr_t *nbr = nbr_table_head(pril_nbr_table);
    if (nbr == NULL)
    {
        LOG_INFO("PRIL Neighbor table is empty\n");
        return;
    }
    LOG_INFO("PRIL Neighbor Table:\n");
    while (nbr != NULL)
    {
        const linkaddr_t *addr = pril_nbr_get_addr(nbr);
        LOG_INFO("  Neighbor %02x:%02x - First Seqno: %d, Last Seqno: %d, Sleep IE ASN: %" PRIu64 ", Secondary Sleep IE ASN: %" PRIu64 " Timing IE(s): %d, rx ts = %u, rx ch = %u, tx ts = %u, tx ch = %u, parent = %d\n",
                 addr->u8[0],
                 addr->u8[1],
                 nbr->first_seqno,
                 nbr->last_seqno,
                 nbr->sleep_ie_asn,
                 nbr->sleep_ie_asn_secondary,
                 nbr->timing_ie_s,
                 nbr->rx_link ? nbr->rx_link->timeslot : 0xFFFF,
                 nbr->rx_link ? nbr->rx_link->channel_offset : 0xFFFF,
                 nbr->tx_link ? nbr->tx_link->timeslot : 0xFFFF,
                 nbr->tx_link ? nbr->tx_link->channel_offset : 0xFFFF,
                 nbr->parent);
        nbr = nbr_table_next(pril_nbr_table, nbr);
    }
}
/***********************************************************************/