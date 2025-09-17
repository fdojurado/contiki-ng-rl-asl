#include "rl-asl-ds-nbr.h"

/* log */
#include "sys/log.h"
#define LOG_MODULE "rl-asl-ds-nbr"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_DS_NBR

NBR_TABLE(rl_asl_ds_nbr_t, rl_asl_ds_nbr_table);

#define EWMA_ALPHA 0.2

/***********************************************************************/
void rl_asl_ds_nbr_init(void)
{
    nbr_table_register(rl_asl_ds_nbr_table, NULL);
    LOG_INFO("RL-ASL neighbor table initialized\n");
}
/***********************************************************************/
rl_asl_ds_nbr_t *rl_asl_ds_nbr_get(const linkaddr_t *addr)
{
    return nbr_table_get_from_lladdr(rl_asl_ds_nbr_table, addr);
}
/***********************************************************************/
const linkaddr_t *rl_asl_ds_nbr_get_addr(rl_asl_ds_nbr_t *nbr)
{
    return nbr_table_get_lladdr(rl_asl_ds_nbr_table, nbr);
}
/***********************************************************************/
void rl_asl_ds_nbr_update(const linkaddr_t *addr, const uint32_t seqno, const uint64_t asn)
{
    rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_get(addr);
    if (nbr == NULL)
    {
        nbr = nbr_table_add_lladdr(rl_asl_ds_nbr_table, addr, NBR_TABLE_REASON_IPV6_ND, NULL);
        if (nbr == NULL)
        {
            LOG_ERR("Failed to add neighbor %02x:%02x to table\n", addr->u8[0], addr->u8[1]);
            return;
        }
        nbr->last_seqno = seqno;
        nbr->last_heard_asn = asn;
        LOG_INFO("Added neighbor %02x:%02x with seqno %u and ASN %" PRIu64 "\n",
                 addr->u8[0], addr->u8[1], seqno, asn);
    }
    else
    {
        if (seqno > nbr->last_seqno)
        {
            nbr->last_seqno = seqno;
        }
        LOG_INFO("Updated neighbor %02x:%02x to seqno %u and ASN %" PRIu64 "\n",
                 addr->u8[0], addr->u8[1], nbr->last_seqno, asn);
    }
    // Update ASN difference EWMA
    int32_t asn_diff = (int32_t)(asn - nbr->last_heard_asn);
    LOG_INFO("Neighbor %02x:%02x ASN diff: %d\n",
             addr->u8[0], addr->u8[1], asn_diff);
    if (nbr->asn_diff_ewma == 0){
        nbr->asn_diff_ewma = asn_diff;
        LOG_INFO("Neighbor %02x:%02x ASN diff EWMA initialized to %u\n",
                 addr->u8[0], addr->u8[1], nbr->asn_diff_ewma);
    }
    else
    {
        nbr->asn_diff_ewma = (EWMA_ALPHA * asn_diff) + ((1 - EWMA_ALPHA) * nbr->asn_diff_ewma);
    }
    nbr->last_heard_asn = asn;

    LOG_INFO("Neighbor %02x:%02x ASN diff EWMA updated to %u\n",
             addr->u8[0], addr->u8[1], nbr->asn_diff_ewma);
}
/***********************************************************************/
void rl_asl_ds_nbr_remove(const linkaddr_t *addr)
{
    rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_get(addr);
    if (nbr != NULL)
    {
        nbr_table_remove(rl_asl_ds_nbr_table, nbr);
        LOG_INFO("Removed neighbor %02x:%02x from table\n", addr->u8[0], addr->u8[1]);
    }
    else
    {
        LOG_WARN("Neighbor %02x:%02x not found in table for removal\n", addr->u8[0], addr->u8[1]);
    }
}
/***********************************************************************/
void rl_asl_ds_nbr_print(void)
{
    rl_asl_ds_nbr_t *nbr = nbr_table_head(rl_asl_ds_nbr_table);
    if (nbr == NULL)
    {
        LOG_INFO("Neighbor table is empty\n");
        return;
    }
    LOG_INFO("RL-ASL Neighbor Table:\n");
    while (nbr != NULL)
    {
        const linkaddr_t *addr = rl_asl_ds_nbr_get_addr(nbr);
        if (addr != NULL)
        {
            LOG_INFO("  Neighbor %02x:%02x - Last Seqno: %u, Last Heard ASN: %" PRIu64 "\n",
                     addr->u8[0], addr->u8[1], nbr->last_seqno, nbr->last_heard_asn);
        }
        nbr = nbr_table_next(rl_asl_ds_nbr_table, nbr);
    }
}
/***********************************************************************/