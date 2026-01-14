#include "rl-asl-data-packet-processor.h"
#include "rl-asl-packets.h"
#include "rl-asl-ds-nbr.h"
#include "tsch.h"

#if BUILD_WITH_RL_ASL
#include "net/mac/tsch/tsch-slot-operation.h"
#endif /* BUILD_WITH_RL_ASL */

#include "os/sys/log.h"
#define LOG_MODULE "rl-asl-data-packet-processor"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_DATA_PACKET_PROCESSOR

/*---------------------------------------------------------------------------*/
void rl_asl_data_packet_input(const uip_ipaddr_t *src, const uint16_t seqnum)
{
    uint64_t full_asn = ((uint64_t)last_rx_asn.ms1b << 32) | last_rx_asn.ls4b;
    LOG_INFO("Processing data packet input with seqnum %d at ASN %" PRIu64 ", from ",
             seqnum, full_asn);
    LOG_INFO_6ADDR(src);
    LOG_INFO_("\n");
#if BUILD_WITH_RL_ASL
    uip_ds6_nbr_t *nbr;

    nbr = uip_ds6_nbr_lookup(src);
    if (nbr == NULL)
    {
        nbr = uip_ds6_nbr_add(src,
                              (uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER),
                              0, NBR_REACHABLE, NBR_TABLE_REASON_IPV6_ND, NULL);
        if (nbr != NULL)
        {
            LOG_INFO("Neighbor added to neighbor cache ");
            LOG_INFO_6ADDR(src);
            LOG_INFO_(", ");
            LOG_INFO_LLADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
            LOG_INFO_("\n");
        }
    }
    const uip_lladdr_t *lladdr = uip_ds6_nbr_lladdr_from_ipaddr(src);
    if (lladdr == NULL)
    {
        LOG_WARN("rl_asl_data_packet_input: Could not find link-layer address for source ");
        LOG_WARN_6ADDR(src);
        LOG_WARN_("\n");
        return;
    }

    rl_asl_ds_nbr_update((const linkaddr_t *)lladdr, seqnum, full_asn, 1);
#endif /* BUILD_WITH_RL_ASL */
}