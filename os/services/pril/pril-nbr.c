#include "pril-nbr.h"
#include "net/mac/tsch/tsch.h"

/* log */
#include "sys/log.h"
#define LOG_MODULE "pril-nbr"
#define LOG_LEVEL LOG_LEVEL_ERR

NBR_TABLE(pril_nbr_t, pril_nbr_table);

static int count_neighbors = 0;

static int min_timing_T_s = INT32_MAX;
static pril_nbr_t *nbr_with_min_T_s = NULL;
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
pril_nbr_t *pril_nbr_add(const linkaddr_t *addr, int16_t seqnum,
                         int16_t sleep_end, uint8_t timing_T_s, bool is_parent)

{
    pril_nbr_t *nbr = pril_nbr_get(addr);
    if (nbr != NULL)
    {
        // Neighbor already exists, update fields
        if (nbr->first_seqno == -1)
            nbr->first_seqno = seqnum;
        nbr->last_seqno = seqnum;
        nbr->sleep_end = sleep_end;
        nbr->timing_T_s = timing_T_s;
        if (nbr->timing_T_s < min_timing_T_s)
        {
            min_timing_T_s = nbr->timing_T_s;
            nbr_with_min_T_s = nbr;
        }
        LOG_INFO("Updated existing neighbor %02x:%02x\n", addr->u8[0], addr->u8[1]);
        return nbr;
    }

    // Add new neighbor
    nbr = nbr_table_add_lladdr(pril_nbr_table, addr, NBR_TABLE_REASON_IPV6_ND, NULL);
    if (nbr != NULL)
    {
        nbr->parent = is_parent;
        nbr->first_seqno = seqnum;
        nbr->tx_state = PRIL_STATE_ON;
        nbr->rx_state = PRIL_STATE_ON;
        nbr->last_seqno = seqnum;
        nbr->sleep_end = sleep_end;
        nbr->new_sleep_end = -1;
        nbr->retr_count = 0;
        nbr->max_retries = 4; // Default max retries
        nbr->timing_T_s = timing_T_s;
        nbr->last_sleep_asn.ms1b = 0;
        nbr->last_sleep_asn.ls4b = 0;
        if (nbr->timing_T_s < min_timing_T_s)
        {
            min_timing_T_s = nbr->timing_T_s;
            nbr_with_min_T_s = nbr;
        }
        count_neighbors++;
        LOG_INFO("Added new neighbor %02x:%02x\n", addr->u8[0], addr->u8[1]);
    }
    else
    {
        LOG_ERR("Failed to add neighbor %02x:%02x - table full\n", addr->u8[0], addr->u8[1]);
    }
    return nbr;
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
void pril_nbr_rx_link_set(pril_nbr_t *nbr, const struct tsch_link *link)
{
    if (nbr != NULL)
    {
        nbr->rx_link = (struct tsch_link *)link;
    }
}
/***********************************************************************/
void pril_nbr_tx_link_set(pril_nbr_t *nbr, const struct tsch_link *link)
{
    if (nbr != NULL)
    {
        nbr->tx_link = (struct tsch_link *)link;
    }
}
/***********************************************************************/
void pril_nbr_rx_set_state(pril_nbr_t *nbr, pril_state_t state)
{
    if (nbr != NULL)
    {
        nbr->rx_state = state;
    }
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
        LOG_INFO("  Neighbor %02x:%02x - First Seqno: %d, Last Seqno: %d, Sleep End: %u, New Sleep End: %d, Retries: %d/%d, Timing T_s: %u, Parent: %d, RX Link: (ts=%u, ch_off=%u), TX Link: (ts=%u, ch_off=%u)\n",
                 addr->u8[0], addr->u8[1],
                 nbr->first_seqno,
                 nbr->last_seqno,
                 nbr->sleep_end,
                 nbr->new_sleep_end,
                 nbr->retr_count,
                 nbr->max_retries,
                 nbr->timing_T_s,
                 nbr->parent,
                 nbr->rx_link ? nbr->rx_link->timeslot : 0xFFFF,
                 nbr->rx_link ? nbr->rx_link->channel_offset : 0xFFFF,
                 nbr->tx_link ? nbr->tx_link->timeslot : 0xFFFF,
                 nbr->tx_link ? nbr->tx_link->channel_offset : 0xFFFF);
        nbr = nbr_table_next(pril_nbr_table, nbr);
    }
}
/***********************************************************************/
pril_nbr_t *pril_nbr_min_gen_period_neighbor(void)
{
    return nbr_with_min_T_s;
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