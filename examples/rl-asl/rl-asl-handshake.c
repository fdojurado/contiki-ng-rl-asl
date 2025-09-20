#include "rl-asl-handshake.h"
#include "rl-asl-net-processor.h"
#include "rl-asl-utils.h"
#include "rl-asl-packets.h"
#include "os/services/orchestra/orchestra.h"
#include "rl-asl-buf.h"

#include <stdlib.h>
#include <string.h>

/* Log configuration */
#include "os/sys/log.h"
#define LOG_MODULE "rl-asl-handshake"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_NET_PROCESSOR

PROCESS(rl_asl_handshake_process, "RL ASL Handshake Process");

/* Current handshake protocol version (wraps at 65535) */
static uint16_t handshake_version = 1;

/* Parent address (updated by rl_asl_handshake_update_parent) */
static linkaddr_t parent_addr;

/* Flag set when an ACK has been received for the outstanding handshake */
static uint8_t ack_received = 0;

/* Timer used to trigger resend/timeout of handshake */
static struct etimer resend_timer;

/* ack parameters structure passed to ctimer callback */
struct ack_parameters
{
    struct ctimer ack_timer;
    uint8_t version;
    linkaddr_t dest;
};
/* Single persistent ack params instance (simple model: one outstanding ACK timer) */
static struct ack_parameters ack_params;

/* Events for the handshake process */
enum
{
    HANDSHAKE_EVENT_NONE = 0,
    HANDSHAKE_EVENT_SEND,
    HANDSHAKE_EVENT_RESEND
};

/* Forward declarations */
static void do_ack(void *ptr);
static void send_ack(uint8_t version, const linkaddr_t *dest);
static void send_handshake_to_parent(const linkaddr_t *addr);
static void eventhandler(process_event_t ev, process_data_t data);

/***************************************************************/
/* ctimer callback that actually sends the ACK (calls IP output) */
static void
do_ack(void *ptr)
{
    struct ack_parameters *params = (struct ack_parameters *)ptr;
    if (params == NULL)
    {
        LOG_ERR("do_ack: params is NULL\n");
        return;
    }

    LOG_INFO("Sending handshake ACK to %02x:%02x\n", params->dest.u8[0], params->dest.u8[1]);

    /* Compose IP header for ACK */
    RL_ASL_IP_BUF->len = RL_ASL_IPH_LEN + RL_ASL_HANDSHAKE_ACKH_LEN;
    RL_ASL_IP_BUF->ttl = 0x40; /* default TTL */
    RL_ASL_IP_BUF->proto = RL_ASL_PROTO_HANDSHAKE_ACK;
    RL_ASL_IP_BUF->scr.u16 = rl_asl_ip_htons(linkaddr_node_addr.u16);
    RL_ASL_IP_BUF->dest.u16 = rl_asl_ip_htons(params->dest.u16);
    RL_ASL_IP_BUF->ipchksum = 0;
    RL_ASL_IP_BUF->ipchksum = ~rl_asl_ip_chksum();

    /* Handshake ack payload */
    RL_ASL_HANDSHAKE_ACK_BUF->version = rl_asl_ip_htons(params->version);

    rl_asl_len = RL_ASL_IP_BUF->len;

    print_ip_header();

    /* Set some MAC attribute if you want (e.g. retry limit) */
    rl_asl_buf_set_attr(RL_ASL_BUF_ATTR_MAX_MAC_TRANSMISSIONS, 3);

    /* Send directly to the destination stored in params */
    rl_asl_ip_output(&params->dest);

    LOG_INFO("Handshake ACK sent to %02x:%02x\n", params->dest.u8[0], params->dest.u8[1]);
}
/***************************************************************/
/* Schedule an ACK after a short randomized delay to avoid collisions.
 * We use the static ack_params instance so do_ack has valid storage. */
static void
send_ack(uint8_t version, const linkaddr_t *dest)
{
    if (dest == NULL)
    {
        LOG_ERR("send_ack: dest is NULL\n");
        return;
    }

    ack_params.version = version;
    linkaddr_copy(&ack_params.dest, dest);

    /* Random delay in [0.5s, 1s) (you can tune) */
    clock_time_t delay = (rand() % (CLOCK_SECOND / 2)) + (CLOCK_SECOND / 2);

    /* Cancel any existing ack_timer, then set a new one */
    ctimer_set(&ack_params.ack_timer, delay, do_ack, &ack_params);
    LOG_DBG("ACK scheduled in %lu ticks for %02x:%02x (version=%u)\n",
            (unsigned long)delay, dest->u8[0], dest->u8[1], ack_params.version);
}
/***************************************************************/
/* Called when we receive a handshake ACK packet from the network.
 * Returns 1 if the ACK is intended for this node and was accepted. */
int rl_asl_handshake_ack_input(linkaddr_t *scr)
{
    LOG_INFO("Received handshake ACK packet (from %02x:%02x)\n", scr->u8[0], scr->u8[1]);

    /* Check destination in IP header matches us */
    if (linkaddr_cmp(&RL_ASL_IP_BUF->dest, &linkaddr_node_addr))
    {
        uint16_t version = rl_asl_ip_htons(RL_ASL_HANDSHAKE_ACK_BUF->version);
        /* We expected handshake_version-1 */
        uint16_t expected = (uint16_t)(handshake_version - 1);
        if (version != expected)
        {
            LOG_WARN("Handshake ACK version mismatch: expected %u, got %u\n", expected, version);
            return 0;
        }

        LOG_INFO("Handshake ACK matches expected version %u — ACK accepted\n", version);
        ack_received = 1;

        /* Stop resend timer (post event to handshake process to do it in process context) */
        process_post(&rl_asl_handshake_process, HANDSHAKE_EVENT_RESEND, NULL);
        return 1;
    }

    LOG_DBG("Handshake ACK not addressed to this node\n");
    return 0;
}
/***************************************************************/
/* Called when we receive a handshake packet (not ACK). If it is for us
 * we reply with an ACK after a randomized delay (send_ack). */
int rl_asl_handshake_input(linkaddr_t *from, linkaddr_t *to)
{
    if (from == NULL)
    {
        LOG_ERR("rl_asl_handshake_input: from is NULL\n");
        return 0;
    }

    uint16_t version = rl_asl_ip_htons(RL_ASL_HANDSHAKE_BUF->version);
    LOG_INFO("Received handshake from %02x:%02x (version=%u)\n", from->u8[0], from->u8[1], version);

    /* Does the IP dest match us? */
    if (linkaddr_cmp(to, &linkaddr_node_addr))
    {
#ifdef NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK
        NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK(from, 1);
#endif
        /* Schedule ACK to sender after short randomized delay */
        send_ack((uint8_t)version, from);
        return 1;
    }

    LOG_DBG("Handshake packet not for us\n");
    return 0;
}
/***************************************************************/
/* Update parent address and trigger handshake send from handshake process. */
void rl_asl_handshake_update_parent(const linkaddr_t *addr)
{
    if (addr == NULL)
    {
        LOG_ERR("rl_asl_handshake_update_parent: addr is NULL\n");
        return;
    }
    linkaddr_copy(&parent_addr, addr);
    LOG_INFO("Updated parent address to %02x:%02x\n", parent_addr.u8[0], parent_addr.u8[1]);

    /* Post event to handshake process to initiate handshake. */
    process_post(&rl_asl_handshake_process, HANDSHAKE_EVENT_SEND, NULL);
}
/***************************************************************/
/* Send handshake packet to the given address (parent). */
static void
send_handshake_to_parent(const linkaddr_t *addr)
{
    if (addr == NULL)
    {
        LOG_ERR("send_handshake_to_parent: addr NULL\n");
        return;
    }

    LOG_INFO("Sending handshake to %02x:%02x (version=%u)\n", addr->u8[0], addr->u8[1], handshake_version);

    RL_ASL_IP_BUF->len = RL_ASL_IPH_LEN + RL_ASL_HANDSHAKEH_LEN;
    RL_ASL_IP_BUF->ttl = 0x40;
    RL_ASL_IP_BUF->proto = RL_ASL_PROTO_HANDSHAKE;
    RL_ASL_IP_BUF->scr.u16 = rl_asl_ip_htons(linkaddr_node_addr.u16);
    RL_ASL_IP_BUF->dest.u16 = rl_asl_ip_htons(addr->u16);
    RL_ASL_IP_BUF->ipchksum = 0;
    RL_ASL_IP_BUF->ipchksum = ~rl_asl_ip_chksum();

    RL_ASL_HANDSHAKE_BUF->version = rl_asl_ip_htons(handshake_version);

    rl_asl_len = RL_ASL_IP_BUF->len;

    print_ip_header();

    /* Optionally: notify neighbor table to add an RX link / increase priority.
     * If your platform provides a neighbor update callback, call it with the parent address.
     * Protect with an #ifdef if not available. */
#ifdef NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK
    NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK(&parent_addr, 1);
#endif

    /* Always send explicitly to the parent address */
    rl_asl_ip_output(NULL);

    LOG_INFO("Handshake sent to %02x:%02x\n", addr->u8[0], addr->u8[1]);
}
/***************************************************************/
/* Event handler executed in process context */
static void
eventhandler(process_event_t ev, process_data_t data)
{
    switch (ev)
    {
    case HANDSHAKE_EVENT_SEND:
        /* Only send if parent address is known (not null) */
        if (!linkaddr_cmp(&parent_addr, &linkaddr_null))
        {
            LOG_INFO("Initiating handshake with parent %02x:%02x\n", parent_addr.u8[0], parent_addr.u8[1]);

            /* Reset ack flag and start handshake send + retry timer */
            ack_received = 0;
            send_handshake_to_parent(&parent_addr);

            /* increment version (wrap-around mod 65536) */
            handshake_version = (uint16_t)(handshake_version + 1);

            /* Arm resend timer: if no ACK arrives within timeout, we can resend or take other action.
             * We use 1 second here (tune to your needs). */
            etimer_set(&resend_timer, CLOCK_SECOND);
        }
        else
        {
            LOG_WARN("Parent address is null — cannot send handshake\n");
        }
        break;

    case HANDSHAKE_EVENT_RESEND:
        /* Either stop timer on ACK or handle timeout (resend) */
        if (ack_received)
        {
            if (etimer_expired(&resend_timer) || etimer_expired(&resend_timer) == 0)
            {
                /* stop the timer if it was running */
                etimer_stop(&resend_timer);
            }
            LOG_INFO("Handshake ACK received — stopped resend timer\n");
            ack_received = 0; /* clear for next handshake */
        }
        else
        {
            /* Timeout without ACK -> perform resend (or backoff) */
            LOG_WARN("Handshake ACK not received — resending handshake\n");
            /* Resend handshake with possibly exponential backoff in production. */
            send_handshake_to_parent(&parent_addr);
            etimer_set(&resend_timer, CLOCK_SECOND * 2); /* next timeout longer */
        }
        break;

    case PROCESS_EVENT_TIMER:
        if (data == &resend_timer)
        {
            /* Timeout without ACK -> perform resend (or backoff) */
            LOG_WARN("Handshake ACK not received (timer event) - resending handshake\n");
            /* Resend handshake with possibly exponential backoff in production. */
            send_handshake_to_parent(&parent_addr);
            etimer_set(&resend_timer, CLOCK_SECOND * 2); /* next timeout longer */
        }
        break;

    default:
        LOG_DBG("Unknown handshake event: %d\n", (int)ev);
        break;
    }
}
/***************************************************************/
PROCESS_THREAD(rl_asl_handshake_process, ev, data)
{
    PROCESS_BEGIN();

    LOG_INFO("RL ASL Handshake process started\n");

    /* Initialize parent to null */
    linkaddr_copy(&parent_addr, &linkaddr_null);
    /* Clear ack params initially */
    memset(&ack_params, 0, sizeof(ack_params));

    while (1)
    {
        PROCESS_YIELD();
        eventhandler(ev, data);
    }

    PROCESS_END();
}
