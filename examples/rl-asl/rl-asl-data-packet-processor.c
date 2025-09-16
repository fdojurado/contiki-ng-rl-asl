#include "rl-asl-data-packet-processor.h"
#include "rl-asl-packets.h"
#include "rl-asl-utils.h"
#include "rl-asl-ds-nbr.h"
#include "rl-asl-net.h"
#include "rl-asl-net-processor.h"
#include "tsch.h"
// #ifndef SAGE_MINIMAL
// #include "orchestra.h"
// #endif /* SAGE_MINIMAL */
#include "os/sys/log.h"
// #if (SAGE_ROOT || SAGE_RELAY) && !WITH_SAGE_ORCHESTRA
// #ifndef SAGE_MINIMAL
// #include "sage.h"
// #include "sage-broadcast-schedule.h"
// #endif /* SAGE_MINIMAL */
// #endif /* (SAGE_ROOT || SAGE_RELAY) && !WITH_SAGE_ORCHESTRA */

#define LOG_MODULE "rl-asl-data-packet-processor"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_DATA_PACKET_PROCESSOR

static struct tsch_asn_t *asn = &tsch_current_asn;

/*---------------------------------------------------------------------------*/
void rl_asl_data_packet_input(int8_t is_for_us)
{
    LOG_INFO("Processing data packet input\n");
    linkaddr_t sender, scr;
    linkaddr_copy(&sender, packetbuf_addr(PACKETBUF_ADDR_SENDER));
    linkaddr_copy(&scr, &RL_ASL_IP_BUF->scr);
    // dest.u16 = rl_asl_ip_htons(RL_ASL_IP_BUF->dest.u16);
    uint64_t generation_time_offset_asn = rl_asl_ip_ntohl64(RL_ASL_DATA_BUF->generation_time_offset_asn);
    print_data_header();
    // what is the payload length?
    uint8_t payload_len = RL_ASL_DATA_BUF->payload_len;
    // lets print raw buffer of the payload
    print_raw_buffer(RL_ASL_DATA_PAYLOAD_PTR, payload_len);
    int rssi = rl_asl_net_get_last_rssi();
    rl_asl_ds_nbr_t *nbr = rl_asl_ds_nbr_add(&sender,
                                         rssi,
                                         RL_ASL_DATA_BUF->flow_id,
                                         rl_asl_ip_htons(RL_ASL_DATA_BUF->seqnum),
                                         generation_time_offset_asn,
                                         RL_ASL_DATA_PAYLOAD_PTR,
                                         payload_len);
    if (nbr == NULL)
    {
        LOG_ERR("Failed to add neighbor %02x:%02x with RSSI %d\n",
                sender.u8[0], sender.u8[1], rssi);
        return; // Exit if neighbor addition failed
    }
    RL_ASL_DATA_BUF->payload_len += sizeof(uint8_t); // Set payload length
    RL_ASL_DATA_BUF->datachksum = 0;                 // Clear checksum before calculating
    RL_ASL_DATA_BUF->datachksum = ~rl_asl_data_chksum();
    rl_asl_ds_nbr_print();

#if (RL_ASL_ROOT || RL_ASL_RELAY) && !WITH_RL_ASL_ORCHESTRA
// #ifndef SAGE_MINIMAL
//     RL_ASL_DATA_BUF->hops_from_leaf += 1;
//     rl_asl_bc_schedule_set_hops_from_leaf(RL_ASL_DATA_BUF->hops_from_leaf,
//                                         rl_asl_ip_ntohl64(RL_ASL_DATA_BUF->expiration_time));
// #endif /* SAGE_MINIMAL */
#endif /* SAGE_ROOT || SAGE_RELAY && !WITH_SAGE_ORCHESTRA */

    uint64_t full_asn = ((uint64_t)asn->ms1b << 32) | asn->ls4b;

    if (is_for_us)
    {
        /* We need to print a log such that this log can be used to calculate the end-to-end delay
         * The log should contain the following information:
         * - Sequence number received
         * - ASN
         */
        LOG_INFO("Data packet received from %02x:%02x with sequence number %d and ASN %" PRIu64 "\n",
                 scr.u8[0], scr.u8[1], rl_asl_ip_htons(RL_ASL_DATA_BUF->seqnum), full_asn);
    }
    else
    {
// #ifndef RL_ASL_MINIMAL
//         // We need to add our tx timeslot to the end of the payload
//         uint8_t *timeslot_ptr = RL_ASL_DATA_PAYLOAD_PTR + payload_len;
//         *timeslot_ptr = TSCH_CALLBACK_UC_RX_TIMESLOT(&sender);
//         LOG_DBG("uc_rx_timeslot: %d for %02x:%02x\n",
//                 *timeslot_ptr, sender.u8[0], sender.u8[1]);
//         rl_asl_len += sizeof(uint8_t); // Update length
// #endif                               /* RL_ASL_MINIMAL */
    }
    RL_ASL_DATA_BUF->datachksum = 0; // Clear checksum before calculating
    RL_ASL_DATA_BUF->datachksum = ~rl_asl_data_chksum();

    print_data_header();

    LOG_INFO("Data packet processed successfully at ASN %" PRIu64 "\n", full_asn);
}