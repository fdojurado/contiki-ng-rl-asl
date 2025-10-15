#include "rl-asl-net-processor.h"
#include "net/netstack.h"
#include "net/routing/routing.h"
#include "rl-asl-buf.h"
#include "rl-asl-routing.h"
#include "net/mac/tsch/tsch.h"
#include "rl-asl-packets.h"
#include "rl-asl-utils.h"
#include "rl-asl-data-packet-processor.h"
#if BUILD_WITH_RL_ASL
#include "orchestra.h"
#include "rl-asl-conf.h"
#endif /* BUILD_WITH_RL_ASL */

#if BUILD_WITH_RL_ASL
#include "rl-asl-handshake.h"
#endif /* BUILD_WITH_RL_ASL */

// #if !WITH_RL_ASL_ORCHESTRA
// #ifndef RL_ASL_MINIMAL
// #include "rl-asl-broadcast-schedule.h"
// #endif /* RL_ASL_MINIMAL */
// #endif /* !WITH_RL_ASL_ORCHESTRA */
#include "os/sys/log.h"

#define LOG_MODULE "rl-asl-net-processor"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_NET_PROCESSOR

enum
{
    PACKET_INPUT
};

uint16_t rl_asl_len;
rl_asl_buf_t rl_asl_aligned_buf;

static void packet_input(void);

PROCESS(rl_asl_net_processor_process, "Net Processor Process");
/*---------------------------------------------------------------------------*/
void rl_asl_output()
{
    const linkaddr_t *nexthop;
    linkaddr_t dest;
    if (rl_asl_len == 0)
        return;

    if (((linkaddr_t *)&RL_ASL_IP_BUF->dest) == NULL)
    {
        LOG_DBG("No destination address set\n");
        goto exit;
    }

    if (linkaddr_cmp(&RL_ASL_IP_BUF->dest, &linkaddr_node_addr))
    {
        LOG_DBG("Packet is for this node\n");
        packet_input();
        return;
    }

    dest.u16 = rl_asl_ip_htons(RL_ASL_IP_BUF->dest.u16);
    // @TODO: Find the next hop
    nexthop = NETSTACK_ROUTING.nexthop(&linkaddr_node_addr, &dest);
    if (nexthop == NULL)
    {
        LOG_ERR("No next hop found for %02x:%02x\n", dest.u8[0], dest.u8[1]);
        goto exit;
    }

    LOG_INFO("Sending packet to %02x:%02x\n", nexthop->u8[0], nexthop->u8[1]);

    rl_asl_ip_output(nexthop);

    goto sent;

sent:
    LOG_INFO("output: packet forwarded\n");
    return;

exit:
    LOG_INFO("output: packet not forwarded\n");
    rl_asl_buf_clear();
    return;
}
/*---------------------------------------------------------------------------*/
void rl_asl_callback_joining_network(void)
{
    LOG_INFO("RL ASL node joining network\n");
    // We need to update the time source with the actual parent in the routing layer
    const linkaddr_t *nxthop;
    nxthop = NETSTACK_ROUTING.nexthop(&linkaddr_node_addr, &root_node_addr);
    if (nxthop != NULL)
    {
        LOG_INFO("Setting time source to %02x:%02x\n", nxthop->u8[0], nxthop->u8[1]);
        tsch_queue_update_time_source(nxthop);
    }
#if BUILD_WITH_RL_ASL
    if (!linkaddr_cmp(nxthop, &root_node_addr))
    {
        rl_asl_handshake_update_parent(nxthop);
    }
    else
    {
/* This is the root node, deactivate the rx link to the parent */
#if BUILD_WITH_RL_ASL
        TSCH_CALLBACK_DEACTIVATE_RX_PARENT_LINK(&root_node_addr);
#endif /* BUILD_WITH_RL_ASL */
    }
#endif /* BUILD_WITH_RL_ASL */
}
/*---------------------------------------------------------------------------*/
static void packet_input(void)
{
    LOG_INFO("Processing input packet\n");

    if (rl_asl_len > 0)
    {
        LOG_INFO("input: received %u bytes\n", rl_asl_len);
        rl_asl_ip_process();
        if (rl_asl_len > 0)
            rl_asl_output();
    }
    else
    {
        LOG_INFO("input: no data to process\n");
    }
}
/*---------------------------------------------------------------------------*/
void rl_asl_ip_process(void)
{
    LOG_DBG("Processing RL ASL IP packet\n");
    uint8_t protocol;
    uint8_t *next_header;
    linkaddr_t dest, scr;

    if (rl_asl_ip_chksum() != 0xffff)
    {
        LOG_DBG("Bad IP checksum, dropping packet\n");
        goto drop;
    }

    if (rl_asl_len < RL_ASL_IP_BUF->len)
    {
        LOG_DBG("Packet too short, dropping packet\n");
        goto drop;
    }

    if (rl_asl_len > sizeof(rl_asl_buf))
    {
        LOG_DBG("Packet too long, dropping packet\n");
        goto drop;
    }

    dest.u16 = rl_asl_ip_htons(RL_ASL_IP_BUF->dest.u16);
    scr.u16 = rl_asl_ip_htons(RL_ASL_IP_BUF->scr.u16);

    LOG_DBG("RL ASL IP packet from %02x:%02x to %02x:%02x, length %d\n",
            scr.u8[0], scr.u8[1],
            dest.u8[0], dest.u8[1], RL_ASL_IP_BUF->len);

    int8_t is_for_us = linkaddr_cmp(&dest, &linkaddr_node_addr) ||
                       linkaddr_cmp(&dest, &linkaddr_null);

    next_header = rl_asl_buf_get_ip_next_header(rl_asl_buf, rl_asl_len, &protocol);

    if (!is_for_us)
    {
        LOG_DBG("ip packet Not for us from %d.%d\n",
                scr.u8[0], scr.u8[1]);

        if (!rl_asl_update_ttl())
        {
            LOG_DBG("TTL expired, dropping packet\n");
            goto drop;
        }

        if (next_header != NULL && protocol == RL_ASL_PROTO_DATA)
            goto data_input; /* Process data packet */

        LOG_DBG("Forwarding packet to destination %d.%d\n",
                RL_ASL_IP_BUF->dest.u8[0], RL_ASL_IP_BUF->dest.u8[1]);

        goto send; /* Proceed to forwarding */
    }

    LOG_DBG("ip packet for us from %d.%d\n", scr.u8[0], scr.u8[1]);

    /* Process upper-layer input */
    if (next_header != NULL)
    {
        switch (protocol)
        {
        case RL_ASL_PROTO_DATA:
            goto data_input; /* Process data packet */
        case RL_ASL_PROTO_HANDSHAKE:
#if BUILD_WITH_RL_ASL
            if (rl_asl_handshake_input(&scr, &dest))
            {
                goto drop; // Handshake processed, drop the packet
            }
            else
            {
                LOG_DBG("Handshake processing failed, dropping packet\n");
                goto drop;
            }
#else
            LOG_DBG("Handshake protocol not supported, dropping packet\n");
            goto drop;
#endif /* BUILD_WITH_RL_ASL */
        case RL_ASL_PROTO_HANDSHAKE_ACK:
#if BUILD_WITH_RL_ASL
            if (rl_asl_handshake_ack_input(&scr, &dest))
            {
                goto drop; // Handshake ACK processed, drop the packet
            }
            else
            {
                LOG_DBG("Handshake ACK processing failed, dropping packet\n");
                goto drop;
            }
#else
            LOG_DBG("Handshake ACK protocol not supported, dropping packet\n");
            goto drop;
#endif /* BUILD_WITH_RL_ASL */
        default:
            LOG_DBG("Unknown protocol %d, dropping packet\n", protocol);
            goto drop;
        }
    }

data_input:

    if (rl_asl_data_chksum() != 0xffff)
    {
        LOG_DBG("Bad data checksum, dropping data packet\n");
        goto drop;
    }
    LOG_DBG("Processing data packet from %02x:%02x to %02x:%02x\n",
            scr.u8[0], scr.u8[1], dest.u8[0], dest.u8[1]);

    rl_asl_data_packet_input(&scr, rl_asl_ip_htons(RL_ASL_DATA_BUF->seqnum), is_for_us);

    if (is_for_us)
        goto drop; // If it's for us, we don't forward it

    LOG_DBG("Forwarding data packet to next hop %02x:%02x\n",
            scr.u8[0], scr.u8[1]);

    goto send;

send:
    RL_ASL_IP_BUF->ipchksum = 0;
    RL_ASL_IP_BUF->ipchksum = ~rl_asl_ip_chksum();
    LOG_DBG("Forwarding packet with length %d (%d)\n",
            rl_asl_len, RL_ASL_IP_BUF->len);
    return;
drop:
    LOG_DBG("Dropping packet\n");
    rl_asl_buf_clear();
    return;
}
/*---------------------------------------------------------------------------*/
void rl_asl_ip_input(void)
{
    if (netstack_process_ip_callback(PACKET_INPUT, NULL) == NETSTACK_IP_PROCESS)
    {
        process_post_synch(&rl_asl_net_processor_process, PACKET_INPUT, NULL);
    }
    rl_asl_buf_clear();
}
/*---------------------------------------------------------------------------*/
uint8_t rl_asl_ip_output(const linkaddr_t *dest)
{
    if (rl_asl_len == 0)
    {
        LOG_DBG("No data to send\n");
        return 0; // No data to send
    }
    int ret;
    if (netstack_process_ip_callback(NETSTACK_IP_OUTPUT, (const linkaddr_t *)dest) ==
        NETSTACK_IP_PROCESS)
    {
        ret = NETSTACK_NETWORK.output(dest);
        return ret;
    }
    else
    {
        /* Ok, ignore and drop... */
        rl_asl_buf_clear();
        return 0; // Indicating failure
    }
}
/*---------------------------------------------------------------------------*/
static void
eventhandler(process_event_t ev, process_data_t data)
{
    switch (ev)
    {
    case PACKET_INPUT:
        LOG_DBG("Processing packet input event\n");
        packet_input();
        break;
    default:
        LOG_DBG("Unknown event: %d\n", ev);
        break;
    }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(rl_asl_net_processor_process, ev, data)
{
    static struct etimer timer;
    PROCESS_BEGIN();
    LOG_INFO("Net Processor Process started\n");

    etimer_set(&timer, CLOCK_SECOND / 2);

    NETSTACK_ROUTING.init();

    rl_asl_buf_clear();

#if BUILD_WITH_RL_ASL
    process_start(&rl_asl_handshake_process, NULL);
#endif /* BUILD_WITH_RL_ASL */

    while (1)
    {
        PROCESS_YIELD();
        eventhandler(ev, data);
    }
    PROCESS_END();
}
