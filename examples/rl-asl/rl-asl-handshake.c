#include "rl-asl-handshake.h"
#include "rl-asl-net-processor.h"
#include "rl-asl-utils.h"
#include "rl-asl-packets.h"

/* Log configuration */
#include "os/sys/log.h"
#define LOG_MODULE "RL ASL Handshake"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_NET_PROCESSOR

PROCESS(rl_asl_handshake_process, "RL ASL Handshake Process");

static linkaddr_t parent_addr; // store parent address

static uint8_t ack_received = 0;

static struct etimer resend_timer;

enum
{
    HANDSHAKE_EVENT_NONE = 0,
    HANDSHAKE_EVENT_SEND,
    HANDSHAKE_EVENT_RESEND
};

/***************************************************************/
void rl_asl_handshake_update_parent(const linkaddr_t *addr)
{
    linkaddr_copy(&parent_addr, addr);
    LOG_INFO("Updated parent address to %02x:%02x\n", parent_addr.u8[0], parent_addr.u8[1]);
    process_post(&rl_asl_handshake_process, HANDSHAKE_EVENT_SEND, NULL);
}
/***************************************************************/
static void send_handshake(const linkaddr_t *addr)
{
    LOG_INFO("Sending handshake to %02x:%02x\n", addr->u8[0], addr->u8[1]);
    RL_ASL_IP_BUF->len = RL_ASL_IPH_LEN + RL_ASL_HANDSHAKEH_LEN;
    RL_ASL_IP_BUF->ttl = 0x40; // Set a default TTL
    RL_ASL_IP_BUF->proto = RL_ASL_PROTO_HANDSHAKE;
    RL_ASL_IP_BUF->scr.u16 = rl_asl_ip_htons(linkaddr_node_addr.u16);
    RL_ASL_IP_BUF->dest.u16 = rl_asl_ip_htons(addr->u16);
    RL_ASL_IP_BUF->ipchksum = 0;
    RL_ASL_IP_BUF->ipchksum = ~rl_asl_ip_chksum();

    rl_asl_len = RL_ASL_IP_BUF->len;

    print_ip_header();

    rl_asl_ip_output(NULL);
}
/***************************************************************/
static void eventhandler(process_event_t ev, process_data_t data)
{
    // Currently, no specific event handling is implemented for handshake
    LOG_INFO("Handshake event received: %d\n", ev);
    switch (ev)
    {
    case HANDSHAKE_EVENT_SEND:
        if (!linkaddr_cmp(&parent_addr, &linkaddr_null))
        {
            ack_received = 0;
            send_handshake(&parent_addr);
            etimer_set(&resend_timer, CLOCK_SECOND / 2); // Set timer to resend handshake
        }
        break;
    case HANDSHAKE_EVENT_RESEND:
        if (!linkaddr_cmp(&parent_addr, &linkaddr_null))
        {
            send_handshake(&parent_addr);
        }
        break;
    }
}
/***************************************************************/
PROCESS_THREAD(rl_asl_handshake_process, ev, data)
{
    PROCESS_BEGIN();

    LOG_INFO("RL ASL Handshake process started\n");

    while (1)
    {
        PROCESS_YIELD();
        eventhandler(ev, data);
    }

    PROCESS_END();
}
/***************************************************************/