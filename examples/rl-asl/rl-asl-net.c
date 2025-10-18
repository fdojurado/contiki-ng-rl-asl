#include "contiki.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "rl-asl-buf.h"
#include "rl-asl-packets.h"
#include "rl-asl-net-processor.h"
#include "rl-asl-utils.h"
#include "os/sys/log.h"

#ifdef BUILD_WITH_PRIL
#include "pril.h"
#endif /* BUILD_WITH_PRIL */

#define LOG_MODULE "rl-asl-net"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_NET

static uint8_t *packetbuf_ptr;
static int last_rssi;

/*---------------------------------------------------------------------------*/
static struct netstack_sniffer *callback = NULL;

void netstack_sniffer_add(struct netstack_sniffer *s)
{
    LOG_INFO("adding netstack packet sniffer\n");
    callback = s;
}

void netstack_sniffer_remove(struct netstack_sniffer *s)
{
    callback = NULL;
}

static void
set_packet_attrs(void)
{
    /* set protocol in NETWORK_ID */
    rl_asl_buf_set_attr(RL_ASL_BUF_ATTR_NETWORK_ID, RL_ASL_IP_BUF->proto);
#ifdef BUILD_WITH_PRIL
    /* set PRIL sleep flag */
    rl_asl_buf_set_attr(RL_ASL_BUF_ATTR_PRIL_SLEEP_FLAG, RL_ASL_DATA_BUF->sleep_end > 0 ? 1 : 0);
#endif /* BUILD_WITH_PRIL */
}

/*--------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static void rl_asl_net_init(void)
{
    LOG_INFO("RL-ASL Net driver initialized\n");
    rl_asl_init();
}
/*---------------------------------------------------------------------------*/
static void rl_asl_net_input(void)
{
    LOG_INFO("RL-ASL Net input received\n");
    uint8_t *buffer;

    /* The MAC puts the 15.4 payload inside the packetbuf data buffer */
    packetbuf_ptr = packetbuf_dataptr();

    if (packetbuf_datalen() == 0)
    {
        LOG_WARN("input: empty packet\n");
        return;
    }

    /* Clear buf and set default attributes */
    rl_asl_buf_clear();

    /* This is default buf since we assume that this is not fragmented */
    buffer = (uint8_t *)RL_ASL_IP_BUF;

    last_rssi = (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);

    rl_asl_len = packetbuf_datalen();

    memcpy(buffer, packetbuf_ptr, rl_asl_len);

    if (callback)
    {
        set_packet_attrs();
        callback->input_callback();
    }

    LOG_INFO("received %u bytes from ", rl_asl_len);
    LOG_INFO_LLADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
    LOG_INFO_("\n");

    print_raw_buffer(buffer, rl_asl_len);

    rl_asl_ip_input();
}
/*---------------------------------------------------------------------------*/
static void packet_sent(void *ptr, int status, int transmissions)
{
    if (callback != NULL)
    {
        callback->output_callback(status);
    }
#ifdef BUILD_WITH_PRIL
    pril_packet_sent(status);
#endif /* BUILD_WITH_RL_ASL */
}
/*---------------------------------------------------------------------------*/
static uint8_t rl_asl_net_output(const linkaddr_t *localdest)
{
    // LOG_INFO("RL-ASL Net output to %02x:%02x\n", localdest->u8[0], localdest->u8[1]);
    linkaddr_t dest;
    packetbuf_clear();
    packetbuf_copyfrom(rl_asl_buf, rl_asl_len);

    if (localdest == NULL)
    {
        linkaddr_copy(&dest, &linkaddr_null);
    }
    else
    {
        linkaddr_copy(&dest, localdest);
    }

    if (callback)
    {
        set_packet_attrs();
    }

    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &dest);
    packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
    packetbuf_set_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS,
                       rl_asl_buf_get_attr(RL_ASL_BUF_ATTR_MAX_MAC_TRANSMISSIONS));

    LOG_INFO("sending %u bytes to ", packetbuf_datalen());
    LOG_PRINT_LLADDR(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
    LOG_INFO_("\n");

    NETSTACK_MAC.send(&packet_sent, NULL);

    watchdog_periodic();
    return 1; // Indicating success
}
/*---------------------------------------------------------------------------*/
int rl_asl_net_get_last_rssi(void)
{
    return last_rssi;
}
/*---------------------------------------------------------------------------*/
const struct network_driver rl_asl_net_driver = {
    "rl-asl-net",
    rl_asl_net_init,
    rl_asl_net_input,
    rl_asl_net_output};