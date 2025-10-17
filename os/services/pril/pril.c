#include "pril.h"
#include "pril-nbr.h"
#include "rl-asl-utils.h"
#include "rl-asl-packets.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/routing.h"
#include "rl-asl-routing.h"
#include "net/queuebuf.h"
#include "orchestra.h"

#include "os/sys/log.h"
#define LOG_MODULE "pril"
#define LOG_LEVEL LOG_LEVEL_DBG

static int keep_tx = 0;
/*---------------------------------------------------------------------------*/
void pril_data_packet_input(const linkaddr_t *src)
{
    LOG_INFO("Received data packet from %02x:%02x\n", src->u8[0], src->u8[1]);
    print_data_header();
    int16_t seqnum = rl_asl_ip_htons(RL_ASL_DATA_BUF->seqnum);
    uint64_t sleep_ie_asn = rl_asl_ip_ntohl64(RL_ASL_DATA_BUF->sleep_ie_asn);
    uint8_t timing_ie_s = RL_ASL_DATA_BUF->timing_ie_s;
    pril_nbr_update(src, seqnum, sleep_ie_asn, timing_ie_s, 0);
    if (pril_nbr_count() > 0 && !pril_nbr_is_there_a_non_paired_child())
    {
        pril_nbr_t *min_gen_nbr = pril_nbr_min_gen_period_neighbor();
        if (min_gen_nbr != NULL)
        {
            const linkaddr_t *min_gen_nbr_addr = pril_nbr_get_addr(min_gen_nbr);
            if (min_gen_nbr_addr != NULL)
            {
                // Did we receive a data packet from the neighbor with the minimum generation period?
                if (linkaddr_cmp(src, min_gen_nbr_addr))
                {
                    LOG_INFO("Data packet received from neighbor with minimum generation period %u s (%02x:%02x)\n",
                             min_gen_nbr->timing_ie_s, src->u8[0], src->u8[1]);
                    // Lets get the next hop towards the root
                    const linkaddr_t *nxthop = NETSTACK_ROUTING.nexthop(&linkaddr_node_addr, &root_node_addr);
                    if (nxthop != NULL)
                    {
                        // We need to convert the tx_interval in seconds to slots
                        uint32_t slots = min_gen_nbr->timing_ie_s * 1000000.0 / (tsch_timing[tsch_ts_timeslot_length]);
                        LOG_DBG("Tx interval in slots: %u\n", slots);
                        struct tsch_asn_t next_asn = tsch_current_asn;
                        TSCH_ASN_INC(next_asn, slots);
                        LOG_INFO("Next ASN for transmission: %" PRIu64 "\n", ((uint64_t)next_asn.ms1b << 32) | next_asn.ls4b);
                        pril_nbr_t *nxt_hop_nbr = pril_nbr_get(nxthop);
                        if (nxt_hop_nbr != NULL)
                        {
                            if (nxt_hop_nbr->sleep_ie_asn != -1)
                            {
                                nxt_hop_nbr->sleep_ie_asn_secondary = ((uint64_t)next_asn.ms1b << 32) | next_asn.ls4b;
                                LOG_INFO("Updated next hop neighbor %02x:%02x with new secondary sleep_ie_asn %" PRIu64 " (current_asn=%" PRIu64 ", slots=%u)\n",
                                         nxthop->u8[0], nxthop->u8[1],
                                         nxt_hop_nbr->sleep_ie_asn_secondary,
                                         ((uint64_t)tsch_current_asn.ms1b << 32) | tsch_current_asn.ls4b,
                                         slots);
                            }
                            else
                            {
                                nxt_hop_nbr->sleep_ie_asn = ((uint64_t)next_asn.ms1b << 32) | next_asn.ls4b;
                                LOG_INFO("Updated next hop neighbor %02x:%02x with new sleep_ie_asn %" PRIu64 " (current_asn=%" PRIu64 ", slots=%u)\n",
                                         nxthop->u8[0], nxthop->u8[1],
                                         nxt_hop_nbr->sleep_ie_asn,
                                         ((uint64_t)tsch_current_asn.ms1b << 32) | tsch_current_asn.ls4b,
                                         slots);
                            }
                            LOG_INFO("Updated next hop neighbor %02x:%02x with new sleep_ie_asn %" PRIu64 " (current_asn=%" PRIu64 ", slots=%u)\n",
                                     nxthop->u8[0], nxthop->u8[1],
                                     nxt_hop_nbr->sleep_ie_asn != -1 ? nxt_hop_nbr->sleep_ie_asn : nxt_hop_nbr->sleep_ie_asn_secondary,
                                     ((uint64_t)tsch_current_asn.ms1b << 32) | tsch_current_asn.ls4b,
                                     slots);
                        }
                    }
                }
            }
        }
    }
}
/*---------------------------------------------------------------------------*/
void pril_check_skip_rx(const struct tsch_link *link, bool *skip_rx)
{
    if (link == NULL || skip_rx == NULL)
    {
        *skip_rx = false;
        return;
    }

    if (!tsch_is_associated)
    {
        *skip_rx = false;
        return;
    }

    if (link->slotframe_handle != 1) // Only check for the dedicated PRIL slotframe (handle 1)
    {
        *skip_rx = false;
        return;
    }

    if (pril_nbr_count() == 0 || pril_nbr_is_there_a_non_paired_child())
    {
        *skip_rx = false;
        return;
    }

    pril_nbr_t *nbr = pril_nbr_get_by_rx_link(link);
    if (nbr == NULL)
    {
        LOG_WARN("No PRIL neighbor found for link, not skipping RX\n");
        *skip_rx = false;
        return;
    }

    const linkaddr_t *neighbor_addr = pril_nbr_get_addr(nbr);
    if (neighbor_addr == NULL)
    {
        LOG_WARN("No address found for PRIL neighbor, not skipping RX\n");
        *skip_rx = false;
        return;
    }

    if (nbr->sleep_ie_asn == -1)
    {
        *skip_rx = false;
        return;
    }

    uint64_t current_asn = ((uint64_t)tsch_current_asn.ms1b << 32) | tsch_current_asn.ls4b;
    int64_t asn_diff = (int64_t)(current_asn - (int64_t)nbr->sleep_ie_asn);

    if (asn_diff >= 0)
    {
        // Neighbor should be awake, do not skip RX
        LOG_INFO("Not skipping RX for neighbor %02x:%02x (current_asn=%" PRIu64 ", sleep_ie_asn=%" PRIu64 ", asn_diff=%" PRId64 ")\n",
                 neighbor_addr->u8[0], neighbor_addr->u8[1],
                 current_asn, nbr->sleep_ie_asn, asn_diff);
        *skip_rx = false;
        return;
    }
    else
    {
        // Neighbor is likely asleep, skip RX
        *skip_rx = true;
        LOG_INFO("Skipping RX (current_asn=%" PRIu64 ", sleep_ie_asn=%" PRIu64 ", asn_diff=%" PRId64 ") for neighbor %02x:%02x\n",
                 current_asn, nbr->sleep_ie_asn, asn_diff,
                 neighbor_addr->u8[0], neighbor_addr->u8[1]);
        return;
    }
}
/*---------------------------------------------------------------------------*/
void pril_check_skip_tx(const struct tsch_link *link, bool *skip_tx, struct tsch_packet *current_packet)
{
    if (link == NULL || skip_tx == NULL)
    {
        *skip_tx = false;
        return;
    }

    if (!tsch_is_associated)
    {
        *skip_tx = false;
        return;
    }

    if (link->slotframe_handle != 1) // Only check for the dedicated PRIL slotframe (handle 1)
    {
        *skip_tx = false;
        return;
    }

    if (pril_nbr_count() == 0 || pril_nbr_is_there_a_non_paired_child())
    {
        *skip_tx = false;
        return;
    }

    pril_nbr_t *nbr = pril_nbr_get_by_tx_link(link);
    if (nbr == NULL)
    {
        LOG_WARN("No PRIL neighbor found for link, not skipping TX\n");
        LOG_DBG("Link timeslot: %u, channel offset: %u\n", link->timeslot, link->channel_offset);
        *skip_tx = false;
        return;
    }

    const linkaddr_t *neighbor_addr = pril_nbr_get_addr(nbr);
    if (neighbor_addr == NULL)
    {
        LOG_WARN("No address found for PRIL neighbor, not skipping TX\n");
        *skip_tx = false;
        return;
    }

    if (keep_tx > 0)
    {
        keep_tx--;
        /* If this is the last packet for this neighbor, we need to set the sleep_ie_asn */
        if (keep_tx == 0)
        {
            LOG_INFO("This is the last TX for neighbor %02x:%02x before setting sleep_ie_asn\n",
                     neighbor_addr->u8[0], neighbor_addr->u8[1]);
            // Lets read from the current packet the sleep_ie_asn
            if (current_packet != NULL && current_packet->qb != NULL)
            {
                void *packet;
                packet = queuebuf_dataptr(current_packet->qb);
                uint8_t packet_len = queuebuf_datalen(current_packet->qb);
                LOG_INFO("Current packet length: %u\n", packet_len);
                //  lets skip the IEEE 802.15.4 header (assuming no security)
                if (packet_len > 9)
                {
                    struct rl_asl_uip_hdr *ip_hdr = (struct rl_asl_uip_hdr *)((uint8_t *)packet + 9);
                    LOG_DBG("IP Header:\n");
                    LOG_DBG("  Length: %d\n", ip_hdr->len);
                    LOG_DBG("  TTL: %d\n", ip_hdr->ttl);
                    LOG_DBG("  Proto: %d\n", ip_hdr->proto);
                    LOG_DBG("  Checksum: %04x\n", ip_hdr->ipchksum);
                    LOG_DBG("  Source: ");
                    LOG_DBG_LLADDR(&ip_hdr->scr);
                    LOG_DBG_("\n");
                    LOG_DBG("  Destination: ");
                    LOG_DBG_LLADDR(&ip_hdr->dest);
                    LOG_DBG_("\n");
                    struct rl_asl_data_hdr *data_hdr = (struct rl_asl_data_hdr *)((uint8_t *)ip_hdr + sizeof(struct rl_asl_uip_hdr));
                    LOG_DBG("Data Header:\n");
                    LOG_DBG("  Payload Length: %d\n", data_hdr->payload_len);
                    LOG_DBG("  Sequence Number: %d\n", rl_asl_ip_htons(data_hdr->seqnum));
                    LOG_DBG("  Sleep IE ASN: %" PRIu64 "\n", rl_asl_ip_ntohl64(data_hdr->sleep_ie_asn));
                    LOG_DBG("  Timing IE (s): %d\n", data_hdr->timing_ie_s);
                    LOG_DBG("  Data Checksum: %04x\n", rl_asl_ip_htons(data_hdr->datachksum));
                    // Lets update the sleep
                    data_hdr->sleep_ie_asn = rl_asl_ip_htonl64(nbr->sleep_ie_asn);
                    LOG_DBG("  updated Sleep IE ASN in packet to: %" PRIu64 "\n", rl_asl_ip_ntohl64(data_hdr->sleep_ie_asn));
                    // Recalculate the data checksum
                    data_hdr->datachksum = 0;
                    data_hdr->datachksum = ~rl_asl_data_chksum();
                    LOG_DBG("  updated Data Checksum in packet to: %04x\n", rl_asl_ip_htons(data_hdr->datachksum));
                }
            }
        }
        *skip_tx = false;
        return;
    }

    if (nbr->sleep_ie_asn == -1)
    {
        *skip_tx = false;
        return;
    }

    uint64_t current_asn = ((uint64_t)tsch_current_asn.ms1b << 32) | tsch_current_asn.ls4b;
    int64_t asn_diff = (int64_t)(current_asn - (int64_t)nbr->sleep_ie_asn);

    if (asn_diff >= 0)
    {
        // Neighbor should be awake, do not skip TX
        LOG_INFO("Not skipping TX for neighbor %02x:%02x (current_asn=%" PRIu64 ", sleep_ie_asn=%" PRIu64 ", asn_diff=%" PRId64 ")\n",
                 neighbor_addr->u8[0], neighbor_addr->u8[1],
                 current_asn, nbr->sleep_ie_asn, asn_diff);
        // How many packets in the queue for this neighbor?
        int num_pkts = tsch_queue_nbr_packet_count(tsch_queue_get_nbr(neighbor_addr));
        LOG_INFO("There are %d packets in the queue for neighbor %02x:%02x\n", num_pkts, neighbor_addr->u8[0], neighbor_addr->u8[1]);
        *skip_tx = false;
        if (nbr->sleep_ie_asn_secondary != -1)
        {
            nbr->sleep_ie_asn = nbr->sleep_ie_asn_secondary;
            LOG_INFO("Updated neighbor %02x:%02x with new sleep_ie_asn %" PRIu64 " from secondary (current_asn=%" PRIu64 ")\n",
                     neighbor_addr->u8[0], neighbor_addr->u8[1],
                     nbr->sleep_ie_asn,
                     current_asn);
            nbr->sleep_ie_asn_secondary = -1; // Reset secondary after using it
        }
        else
        {
            nbr->sleep_ie_asn = -1; // Reset to indicate we don't know the next sleep time
            LOG_INFO("Reset sleep_ie_asn for neighbor %02x:%02x to -1 (current_asn=%" PRIu64 ")\n",
                     neighbor_addr->u8[0], neighbor_addr->u8[1],
                     current_asn);
        }
        keep_tx = num_pkts > 0 ? num_pkts - 1 : 0; // We will send one packet now, so reduce by 1
        return;
    }
    else
    {
        // Neighbor is likely asleep, skip TX
        *skip_tx = true;
        LOG_INFO("Skipping TX (current_asn=%" PRIu64 ", sleep_ie_asn=%" PRIu64 ", asn_diff=%" PRId64 ") for neighbor %02x:%02x\n",
                 current_asn, nbr->sleep_ie_asn, asn_diff,
                 neighbor_addr->u8[0], neighbor_addr->u8[1]);

        return;
    }
}