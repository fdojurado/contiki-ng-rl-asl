#include "rl-asl-ds-nbr.h"
#include "net/routing/routing.h"

/* log */
#include "sys/log.h"
#define LOG_MODULE "rl-asl-ds-nbr"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_DS_NBR

NBR_TABLE(rl_asl_ds_nbr_t, rl_asl_ds_nbr_table);

#define EWMA_ALPHA 0.2

static int count_neighbors = 0;

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
void rl_asl_ds_nbr_update(const linkaddr_t *addr, uint32_t seqno, uint64_t asn, int8_t is_child)
{
    rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_get(addr);
    int32_t asn_diff = 0;

    if (nbr == NULL)
    {
        nbr = nbr_table_add_lladdr(rl_asl_ds_nbr_table, addr, NBR_TABLE_REASON_IPV6_ND, NULL);
        if (nbr == NULL)
        {
            LOG_ERR("Failed to add neighbor %02x:%02x to table\n", addr->u8[0], addr->u8[1]);
            return;
        }
        nbr->first_seqno = seqno;
        nbr->last_seqno = seqno;
        nbr->is_child = is_child;
        nbr->last_heard_asn = asn;
        nbr->asn_diff_ewma = 0;
        nbr->asn_diff_var_ewma = 0;
        nbr->last_expected_asn = 0;
        nbr->predicted_skips = 0;
        LOG_INFO("Added neighbor %02x:%02x with seqno %u and ASN %" PRIu64 " is child=%d\n",
                 addr->u8[0], addr->u8[1], seqno, asn, nbr->is_child);
        count_neighbors++;
        return;
    }

    if (seqno > nbr->last_seqno)
    {
        nbr->last_seqno = seqno;
    }

    if (nbr->last_heard_asn != 0 && asn > nbr->last_heard_asn)
    {
        asn_diff = (int32_t)(asn - nbr->last_heard_asn);
        if (nbr->asn_diff_ewma == 0)
        {
            nbr->asn_diff_ewma = asn_diff;
            nbr->asn_diff_var_ewma = 0;
            LOG_INFO("Neighbor %02x:%02x ASN diff EWMA initialized to %f\n",
                     addr->u8[0], addr->u8[1], nbr->asn_diff_ewma);
        }
        else
        {
            nbr->asn_diff_ewma = (float)(EWMA_ALPHA * asn_diff + (1.0f - EWMA_ALPHA) * nbr->asn_diff_ewma);
            int32_t diff = (int32_t)asn_diff - (int32_t)nbr->asn_diff_ewma;
            nbr->asn_diff_var_ewma = (float)(EWMA_ALPHA * (diff * diff) + (1.0f - EWMA_ALPHA) * nbr->asn_diff_var_ewma);
            LOG_INFO("Neighbor %02x:%02x ASN diff: %d, EWMA updated to %f, Var EWMA updated to %f\n",
                     addr->u8[0], addr->u8[1], asn_diff, nbr->asn_diff_ewma, nbr->asn_diff_var_ewma);
        }
    }

    nbr->last_heard_asn = asn;
    nbr->last_expected_asn = asn;
    nbr->predicted_skips = 0;

    LOG_INFO("Updated neighbor %02x:%02x to seqno %u and ASN %" PRIu64 "\n",
             addr->u8[0], addr->u8[1], nbr->last_seqno, asn);
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
rl_asl_ds_nbr_t *rl_asl_ds_nbr_get_any(void)
{
    return nbr_table_head(rl_asl_ds_nbr_table);
}
/***********************************************************************/
int rl_asl_ds_nbr_is_nbr_paired(const linkaddr_t *addr)
{
    rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_get(addr);
    if (nbr == NULL)
    {
        return 0;
    }
    return (nbr->last_seqno - nbr->first_seqno >= 3) ? 1 : 0;
}
/***********************************************************************/
bool rl_asl_ds_nbr_is_there_a_non_paired_child(void)
{
    rl_asl_ds_nbr_t *nbr = nbr_table_head(rl_asl_ds_nbr_table);
    while (nbr != NULL)
    {
        if (nbr->is_child && (nbr->last_seqno - nbr->first_seqno < 3))
        {
            return true;
        }
        nbr = nbr_table_next(rl_asl_ds_nbr_table, nbr);
    }
    return false;
}
/***********************************************************************/
rl_asl_ds_nbr_t *rl_asl_ds_nbr_head(void)
{
    return nbr_table_head(rl_asl_ds_nbr_table);
}
/***********************************************************************/
rl_asl_ds_nbr_t *rl_asl_ds_nbr_next(rl_asl_ds_nbr_t *nbr)
{
    return nbr_table_next(rl_asl_ds_nbr_table, nbr);
}
/***********************************************************************/
int rl_asl_ds_nbr_child_count(void)
{
    int child_count = 0;
    rl_asl_ds_nbr_t *nbr = nbr_table_head(rl_asl_ds_nbr_table);
    while (nbr != NULL)
    {
        if (nbr->is_child)
        {
            child_count++;
        }
        nbr = nbr_table_next(rl_asl_ds_nbr_table, nbr);
    }
    return child_count;
}
/***********************************************************************/
int rl_asl_ds_nbr_count(void)
{
    return count_neighbors;
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