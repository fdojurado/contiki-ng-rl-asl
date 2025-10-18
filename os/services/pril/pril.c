#include "pril.h"
#include "pril-nbr.h"
#include "rl-asl-utils.h"
#include "rl-asl-packets.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/routing.h"
#include "rl-asl-routing.h"
#include "net/queuebuf.h"
#include "pril-utils.h"
#include "rl-asl-buf.h"
#include "orchestra.h"

#include "os/sys/log.h"
#define LOG_MODULE "pril"
#define LOG_LEVEL LOG_LEVEL_DBG

/*---------------------------------------------------------------------------*/
static void pril_on_tx_success(pril_nbr_t *nbr, const linkaddr_t *neighbor_addr)
{
    if (nbr == NULL)
        return;

    nbr->tx_state = PRIL_STATE_OFF;
    nbr->retr_count = 0;
    LOG_INFO("PRIL TX: sleep frame sent and ACKed -> TX OFF for %02x:%02x (sleep_end=%d)\n",
             neighbor_addr->u8[0], neighbor_addr->u8[1], nbr->sleep_end);
}
/*---------------------------------------------------------------------------*/
static void pril_on_tx_noack(pril_nbr_t *nbr, const linkaddr_t *neighbor_addr)
{
    if (nbr == NULL)
        return;

    nbr->retr_count++;
    if (nbr->retr_count >= nbr->max_retries)
    {
        // Exceeded retries: per paper move to OFF (uncertain)
        nbr->tx_state = PRIL_STATE_OFF;
        nbr->retr_count = 0;
        LOG_WARN("PRIL TX: sleep frame exceeded retries -> move TX to OFF (uncertain) for %02x:%02x\n",
                 neighbor_addr->u8[0], neighbor_addr->u8[1]);
    }
    else
    {
        nbr->tx_state = PRIL_STATE_RETR;
        LOG_INFO("PRIL TX: sleep frame no ACK -> RETR (count=%d) for %02x:%02x\n",
                 nbr->retr_count,
                 neighbor_addr->u8[0], neighbor_addr->u8[1]);
    }
}
/*---------------------------------------------------------------------------*/
void pril_packet_sent(int mac_status)
{
    const linkaddr_t *neighbor_addr;
    pril_nbr_t *nbr;

    /* Does this packet have a sleep_end > 0? */
    int was_sleep_end_set = rl_asl_buf_get_attr(RL_ASL_BUF_ATTR_PRIL_SLEEP_FLAG);
    if (!was_sleep_end_set)
    {
        LOG_DBG("PRIL packet sent: no sleep_end set, ignoring: %d\n", was_sleep_end_set);
        return;
    }

    LOG_DBG("PRIL packet sent: sleep_end was set, processing: %d\n", was_sleep_end_set);

    neighbor_addr = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
    nbr = pril_nbr_get(neighbor_addr);
    if (nbr == NULL)
        return;

    LOG_INFO("PRIL packet sent to %02x:%02x, mac_status=%d\n",
             neighbor_addr->u8[0], neighbor_addr->u8[1], mac_status);

    if (mac_status == MAC_TX_OK)
    {
        pril_on_tx_success(nbr, neighbor_addr);
    }
    else
    {
        pril_on_tx_noack(nbr, neighbor_addr);
    }
    rl_asl_buf_set_attr(RL_ASL_BUF_ATTR_PRIL_SLEEP_FLAG, 0);
}
/*---------------------------------------------------------------------------*/
static void pril_process_data_packet_from_nref(pril_nbr_t *nbr)
{
    if (nbr == NULL)
        return;

    if (pril_nbr_count() == 0 || pril_nbr_is_there_a_non_paired_child())
        return;

    pril_nbr_t *min_gen_nbr = pril_nbr_min_gen_period_neighbor();
    if (min_gen_nbr == NULL)
        return;

    const linkaddr_t *min_gen_nbr_addr = pril_nbr_get_addr(min_gen_nbr);
    if (min_gen_nbr_addr == NULL)
        return;

    // Did we receive a data packet from the neighbor with the minimum generation period?
    const linkaddr_t *nbr_addr = pril_nbr_get_addr(nbr);
    if (nbr_addr == NULL)
        return;

    if (!linkaddr_cmp(nbr_addr, min_gen_nbr_addr))
        return;

    LOG_INFO("Data packet received from neighbor with minimum generation period %02x:%02x\n",
             min_gen_nbr_addr->u8[0], min_gen_nbr_addr->u8[1]);

    // Lets get the next hop towards the root
    const linkaddr_t *nxthop = NETSTACK_ROUTING.nexthop(&linkaddr_node_addr, &root_node_addr);

    if (nxthop == NULL)
        return;

    pril_nbr_t *nxt_hop_nbr = pril_nbr_get(nxthop);
    if (nxt_hop_nbr == NULL)
        return;

    int Tmin_cells = pril_compute_cells_from_seconds(min_gen_nbr->timing_T_s);

    if (nxt_hop_nbr->sleep_end > 0)
    {
        nxt_hop_nbr->new_sleep_end = Tmin_cells;
        LOG_INFO("  Updating next hop neighbor %02x:%02x new_sleep_end to %d\n",
                 nxthop->u8[0], nxthop->u8[1], nxt_hop_nbr->new_sleep_end);
    }
    else
    {
        nxt_hop_nbr->sleep_end = Tmin_cells;
        LOG_INFO("  Updating next hop neighbor %02x:%02x sleep_end to %d\n",
                 nxthop->u8[0], nxthop->u8[1], nxt_hop_nbr->sleep_end);
    }
}
/*---------------------------------------------------------------------------*/
void pril_data_packet_input(const linkaddr_t *src)
{
    LOG_INFO("Received data packet from %02x:%02x\n", src->u8[0], src->u8[1]);
    print_data_header();
    int16_t seqnum = rl_asl_ip_htons(RL_ASL_DATA_BUF->seqnum);
    int16_t sleep_end = rl_asl_ip_htons(RL_ASL_DATA_BUF->sleep_end);
    uint8_t timing_T_s = RL_ASL_DATA_BUF->timing_T_s;
    LOG_DBG("PRIL Data IE: sleep_end=%d, timing_T_s=%u\n", sleep_end, timing_T_s);
    pril_nbr_t *nbr = pril_nbr_add(src, seqnum, sleep_end, timing_T_s, false);
    if (sleep_end > 0)
        pril_nbr_rx_set_state(nbr, PRIL_STATE_OFF);
    pril_process_data_packet_from_nref(nbr);
    RL_ASL_DATA_BUF->sleep_end = 0; // Clear sleep_end after processing
    // Recalculate the data checksum
    RL_ASL_DATA_BUF->datachksum = 0;
    RL_ASL_DATA_BUF->datachksum = ~rl_asl_data_chksum();
    pril_nbr_print();
}
/*---------------------------------------------------------------------------*/
void pril_check_skip_rx(const struct tsch_link *link, bool *skip_rx)
{
    *skip_rx = false;
    if (link == NULL)
        return;
    if (!tsch_is_associated)
        return;
    if (link->slotframe_handle != 1)
        return;

    pril_nbr_t *nbr = pril_nbr_get_by_rx_link(link);
    if (nbr == NULL)
        return;

    if (pril_nbr_count() == 0 || pril_nbr_is_there_a_non_paired_child())
        return;

    // LOG_DBG("Checking PRIL RX skip for link timeslot %u, channel offset %u\n",
    //         link->timeslot, link->channel_offset);

    const linkaddr_t *neighbor_addr = pril_nbr_get_addr(nbr);

    if (neighbor_addr == NULL)
    {
        LOG_WARN("No address found for PRIL neighbor, not skipping RX\n");
        return;
    }

    /* If RX state is OFF and sleep_end>0, skip */
    if (nbr->rx_state == PRIL_STATE_OFF && nbr->sleep_end > 0)
    {
        *skip_rx = true;
        // LOG_DBG("PRIL RX skip for %02x:%02x sleep_end=%d\n",
        //         neighbor_addr->u8[0], neighbor_addr->u8[1], nbr->sleep_end);
        return;
    }

    LOG_DBG("PRIL RX not skipped for %02x:%02x (state=%d, sleep_end=%d)\n",
            neighbor_addr->u8[0], neighbor_addr->u8[1],
            nbr->rx_state, nbr->sleep_end);

    *skip_rx = false;

    return;
}
/*---------------------------------------------------------------------------*/
void pril_check_skip_tx(const struct tsch_link *link, bool *skip_tx)
{
    *skip_tx = false;
    if (link == NULL)
        return;
    if (!tsch_is_associated)
        return;
    if (link->slotframe_handle != 1)
        return;

    pril_nbr_t *nbr = pril_nbr_get_by_tx_link(link);
    if (nbr == NULL)
        return;

    if (pril_nbr_count() == 0 || pril_nbr_is_there_a_non_paired_child())
        return;

    LOG_DBG("Checking PRIL TX skip for link timeslot %u, channel offset %u\n",
            link->timeslot, link->channel_offset);

    const linkaddr_t *neighbor_addr = pril_nbr_get_addr(nbr);
    if (neighbor_addr == NULL)
    {
        LOG_WARN("No address found for PRIL neighbor, not skipping TX\n");
        return;
    }

    /* If TX is OFF and sleep_end>0, skip TX (do not attempt to send until ON) */
    if (nbr->tx_state == PRIL_STATE_OFF && nbr->sleep_end > 0)
    {
        *skip_tx = true;
        LOG_DBG("PRIL TX skip for %02x:%02x sleep_end=%d\n",
                neighbor_addr->u8[0], neighbor_addr->u8[1], nbr->sleep_end);
        return;
    }

    /* If in RETR state we should attempt to send (TSCH will try retries) */
    if (nbr->tx_state == PRIL_STATE_RETR)
    {
        *skip_tx = false;
        return;
    }

    /* If we have pending attach and q>0, allow TX (we'll attach if q==1) */
    struct tsch_neighbor *t_nbr = tsch_queue_get_nbr(neighbor_addr);
    if (t_nbr == NULL)
    {
        *skip_tx = false;
        return;
    }
    int q = tsch_queue_nbr_packet_count(t_nbr);
    if (q > 0)
    {
        *skip_tx = false;
        return;
    }

    /* Default allow */
    *skip_tx = false;
}
/*---------------------------------------------------------------------------*/
int pril_link_state_tick(pril_nbr_t *nbr)
{
    if (nbr == NULL)
        return -1;

    const linkaddr_t *neighbor_addr = pril_nbr_get_addr(nbr);

    if (neighbor_addr == NULL)
        return -1;

    /* Decrement main and new counters if >0 */
    if (nbr->sleep_end > 0)
    {
        nbr->sleep_end--;
        if (nbr->sleep_end == 0)
        {
            /* Transition OFF -> ON (or RETR -> ON) handled by caller logic */
            if (nbr->rx_state == PRIL_STATE_OFF)
            {
                nbr->rx_state = PRIL_STATE_ON;
                LOG_INFO("PRIL link %02x:%02x RX -> ON (sleep_end reached 0)\n",
                         neighbor_addr->u8[0], neighbor_addr->u8[1]);
            }
            if (nbr->tx_state == PRIL_STATE_OFF || nbr->tx_state == PRIL_STATE_RETR)
            {
                nbr->tx_state = PRIL_STATE_ON;
                LOG_INFO("PRIL link %02x:%02x TX -> ON (sleep_end reached 0)\n",
                         neighbor_addr->u8[0], neighbor_addr->u8[1]);
                /* If new_sleep_end was queued, transfer it now */
                if (nbr->new_sleep_end > 0)
                {
                    nbr->sleep_end = nbr->new_sleep_end;
                    nbr->new_sleep_end = 0;
                    // nbr->tx_state = PRIL_STATE_OFF;
                    LOG_INFO("PRIL link %02x:%02x applying queued new_sleep_end=%d\n",
                             neighbor_addr->u8[0], neighbor_addr->u8[1], nbr->sleep_end);
                }
            }
        }
    }
    if (nbr->new_sleep_end > 0)
    {
        nbr->new_sleep_end--;
        // when both counters tick, logic from paper is: both decreased contextually.
        // The copy happens when returning to ON as above.
    }
    return 0;
}
/*---------------------------------------------------------------------------*/
void pril_slot_tick_for_link(const struct tsch_link *link)
{
    if (link == NULL)
        return;

    if (!tsch_is_associated)
        return;

    if (link->slotframe_handle != 1) // Only check for the dedicated PRIL slotframe (handle 1)
        return;

    // Lets loop through all neighbors that has the same timeslot
    uint64_t current_asn = ((uint64_t)tsch_current_asn.ms1b << 32) | tsch_current_asn.ls4b;
    int64_t asn_diff;
    pril_nbr_t *nbr = pril_nbr_head();

    while (nbr != NULL)
    {
        const linkaddr_t *nbr_addr = pril_nbr_get_addr(nbr);
        if (nbr_addr == NULL)
        {
            nbr = pril_nbr_next(nbr);
            continue;
        }

        bool matches_tx = (nbr->tx_link != NULL && nbr->tx_link->timeslot == link->timeslot);
        bool matches_rx = (nbr->rx_link != NULL && nbr->rx_link->timeslot == link->timeslot);

        if (matches_tx || matches_rx)
        {
            const char *which = matches_tx && matches_rx ? "TX/RX" : (matches_tx ? "TX" : "RX");
            // LOG_DBG("Found PRIL %s link for neighbor %02x:%02x\n",
            //         which, nbr_addr->u8[0], nbr_addr->u8[1]);

            asn_diff = (int64_t)(current_asn - (((uint64_t)nbr->last_sleep_asn.ms1b << 32) | nbr->last_sleep_asn.ls4b));
            if (asn_diff > 0)
            {
                // LOG_DBG("Processing PRIL %s link timeslot %u, channel offset %u, neighbor %02x:%02x\n",
                //         which, link->timeslot, link->channel_offset,
                //         nbr_addr->u8[0], nbr_addr->u8[1]);
                int result = pril_link_state_tick(nbr);
                if (result == 0)
                {
                    LOG_DBG("  Updated PRIL %s link state for %02x:%02x: sleep_end=%d, new_sleep_end=%d, tx_state=%d, rx_state=%d\n",
                            which, nbr_addr->u8[0], nbr_addr->u8[1],
                            nbr->sleep_end, nbr->new_sleep_end, nbr->tx_state, nbr->rx_state);
                    nbr->last_sleep_asn.ms1b = tsch_current_asn.ms1b;
                    nbr->last_sleep_asn.ls4b = tsch_current_asn.ls4b;
                }
            }
        }

        nbr = pril_nbr_next(nbr);
    }
}
/*---------------------------------------------------------------------------*/
void pril_attach_sleep_if_last(const struct tsch_link *link, const struct tsch_packet *current_packet)
{
    if (link == NULL)
        return;
    if (!tsch_is_associated)
        return;
    if (link->slotframe_handle != 1)
        return;

    pril_nbr_t *nbr = pril_nbr_get_by_tx_link(link);
    if (nbr == NULL)
        return;

    if (pril_nbr_count() == 0 || pril_nbr_is_there_a_non_paired_child())
        return;

    const linkaddr_t *neighbor_addr = pril_nbr_get_addr(nbr);
    if (neighbor_addr == NULL)
        return;

    int q = tsch_queue_nbr_packet_count(tsch_queue_get_nbr(neighbor_addr));
    if (q == 1 && nbr->tx_state == PRIL_STATE_ON && nbr->sleep_end > 0)
    {
        if (current_packet == NULL || current_packet->qb == NULL)
            return;

        LOG_DBG("Attaching Sleep IE for next TX to %02x:%02x (q=1, state=ON, sleep_end=%d)\n",
                neighbor_addr->u8[0], neighbor_addr->u8[1], nbr->sleep_end);

        /* Here we first need to check which children has the minimum timing_T_s */
        pril_nbr_t *child_with_min_T_s = pril_nbr_min_gen_period_neighbor();
        if (child_with_min_T_s == NULL)
            return;

        const linkaddr_t *child_addr = pril_nbr_get_addr(child_with_min_T_s);
        if (child_addr == NULL)
            return;

        LOG_DBG("Child with min T_s is %02x:%02x with T_s=%u\n",
                child_addr->u8[0], child_addr->u8[1],
                child_with_min_T_s->timing_T_s);

        void *packet;
        packet = queuebuf_dataptr(current_packet->qb);
        uint8_t packet_len = queuebuf_datalen(current_packet->qb);
        LOG_INFO("Current packet length: %u\n", packet_len);
        //  lets skip the IEEE 802.15.4 header (assuming no security)
        if (packet_len <= 9)
        {
            LOG_WARN("Packet too short to attach Sleep IE\n");
            return;
        }

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
        // Lets update the sleep
        data_hdr->sleep_end = rl_asl_ip_htons(nbr->sleep_end);
        LOG_DBG("  Sleep End: %d\n", rl_asl_ip_htons(data_hdr->sleep_end));
        LOG_DBG("  Timing T_s: %u\n", data_hdr->timing_T_s);
        LOG_DBG("  Data Checksum: %04x\n", rl_asl_ip_htons(data_hdr->datachksum));
        LOG_DBG("  updated Sleep End in packet to: %d\n", rl_asl_ip_htons(data_hdr->sleep_end));
        // Recalculate the data checksum
        data_hdr->datachksum = 0;
        data_hdr->datachksum = ~rl_asl_data_chksum_from_buffer((uint8_t *)data_hdr);
        LOG_DBG("  updated Data Checksum in packet to: %04x\n", rl_asl_ip_htons(data_hdr->datachksum));
        // We need to set 'RL_ASL_BUF_ATTR_PRIL_SLEEP_FLAG' so that pril_packet_sent knows this packet had sleep info
        rl_asl_buf_set_attr(RL_ASL_BUF_ATTR_PRIL_SLEEP_FLAG, 1);
        // lets see if we set it
    }
    else if (nbr->tx_state == PRIL_STATE_RETR && nbr->sleep_end > 0) // If this is a retransmission, we still need to update the sleep_end IE
    {
        if (current_packet == NULL || current_packet->qb == NULL)
            return;

        LOG_DBG("Re-attaching Sleep IE for RETR TX to %02x:%02x (state=RETR, sleep_end=%d)\n",
                neighbor_addr->u8[0], neighbor_addr->u8[1], nbr->sleep_end);

        void *packet;
        packet = queuebuf_dataptr(current_packet->qb);
        uint8_t packet_len = queuebuf_datalen(current_packet->qb);
        LOG_INFO("Current packet length: %u\n", packet_len);
        //  lets skip the IEEE 802.15.4 header (assuming no security)
        if (packet_len <= 9)
        {
            LOG_WARN("Packet too short to attach Sleep IE\n");
            return;
        }

        struct rl_asl_uip_hdr *ip_hdr = (struct rl_asl_uip_hdr *)((uint8_t *)packet + 9);
        struct rl_asl_data_hdr *data_hdr = (struct rl_asl_data_hdr *)((uint8_t *)ip_hdr + sizeof(struct rl_asl_uip_hdr));
        // Lets update the sleep
        data_hdr->sleep_end = rl_asl_ip_htons(nbr->sleep_end);
        LOG_DBG("  Sleep End: %d\n", rl_asl_ip_htons(data_hdr->sleep_end));
        // Recalculate the data checksum
        data_hdr->datachksum = 0;
        data_hdr->datachksum = ~rl_asl_data_chksum_from_buffer((uint8_t *)data_hdr);
        LOG_DBG("  updated Data Checksum in packet to: %04x\n", rl_asl_ip_htons(data_hdr->datachksum));
        // We need to set 'RL_ASL_BUF_ATTR_PRIL_SLEEP_FLAG' so that pril_packet_sent knows this packet had sleep info
        rl_asl_buf_set_attr(RL_ASL_BUF_ATTR_PRIL_SLEEP_FLAG, 1);
    }
    else
    {
        LOG_DBG("Not attaching Sleep IE for next TX to %02x:%02x (q=%d, state=%d, sleep_end=%d)\n",
                neighbor_addr->u8[0], neighbor_addr->u8[1], q, nbr->tx_state, nbr->sleep_end);
    }
}