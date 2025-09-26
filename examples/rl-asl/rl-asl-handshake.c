#include "rl-asl-handshake.h"
#include "rl-asl-net-processor.h"
#include "rl-asl-utils.h"
#include "rl-asl-packets.h"
#include "os/services/orchestra/orchestra.h"
#include "rl-asl-buf.h"
#include "rl-asl-conf.h"
#include "net/routing/routing.h"
#include "rl-asl-ds-nbr.h"

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

/* retry/backoff parameters */
static uint8_t retry_count = 0;
static const uint8_t max_retries = 4;

/* ack parameters structure passed to ctimer callback */
struct ack_parameters
{
    struct ctimer ack_timer;
    uint16_t version;
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
static void send_ack(uint16_t version, const linkaddr_t *dest);
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

    LOG_INFO("Sending handshake ACK to %02x:%02x (version=%u)\n",
             params->dest.u8[0], params->dest.u8[1], params->version);

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
    rl_asl_buf_set_attr(RL_ASL_BUF_ATTR_MAX_MAC_TRANSMISSIONS, 4);

    /* Send explicitly to the destination */
    rl_asl_ip_output(&params->dest);

    LOG_INFO("Handshake ACK sent to %02x:%02x\n", params->dest.u8[0], params->dest.u8[1]);
}
/***************************************************************/
/* Schedule an ACK after a short randomized delay to avoid collisions.
 * We use the static ack_params instance so do_ack has valid storage. */
static void
send_ack(uint16_t version, const linkaddr_t *dest)
{
    if (dest == NULL)
    {
        LOG_ERR("send_ack: dest is NULL\n");
        return;
    }

    /* Cancel any existing ack ctimer first to avoid clobbering params */
    ctimer_stop(&ack_params.ack_timer);

    ack_params.version = version;
    linkaddr_copy(&ack_params.dest, dest);

    ctimer_set(&ack_params.ack_timer, 0, do_ack, &ack_params);

    LOG_DBG("ACK scheduled in %lu ticks for %02x:%02x (version=%u)\n",
            (unsigned long)0, dest->u8[0], dest->u8[1], ack_params.version);
}
/***************************************************************/
/* Called when we receive a handshake ACK packet from the network.
 * Returns 1 if the ACK is intended for this node and was accepted. */
int rl_asl_handshake_ack_input(linkaddr_t *scr, linkaddr_t *dest)
{
    LOG_INFO("Received handshake ACK packet (from %02x:%02x)\n", scr->u8[0], scr->u8[1]);

    /* Check destination in IP header matches us */
    if (linkaddr_cmp(dest, &linkaddr_node_addr))
    {
        uint16_t version = rl_asl_ip_htons(RL_ASL_HANDSHAKE_ACK_BUF->version);
        /* We expected handshake_version-1 */
        uint16_t expected = (uint16_t)(handshake_version);
        if (version != expected)
        {
            LOG_WARN("Handshake ACK version mismatch: expected %u, got %u\n", expected, version);
            return 0;
        }

        LOG_INFO("Handshake ACK matches expected version %u — ACK accepted\n", version);
        ack_received = 1;

        /* Stop resend etimer and any outstanding ctimer for ACK scheduling */
        if (etimer_expired(&resend_timer) == 0)
        {
            etimer_stop(&resend_timer);
        }
        ctimer_stop(&ack_params.ack_timer);

        /* Notify handshake process to handle success (in process context) */
        process_post(&rl_asl_handshake_process, HANDSHAKE_EVENT_RESEND, NULL);

        /* reset retry count for next handshake round */
        retry_count = 0;

        /* We need to evaluate if we have children if we don't have any children
         * we can switch to leaf mode */
        if (NETSTACK_ROUTING.is_in_leaf_mode())
        {
            TSCH_CALLBACK_DEACTIVATE_RX_LINK();
        }

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

        rl_asl_ds_nbr_update(from, 0, 0, 1); // Add/update neighbor with seqno 0 and ASN 0

        TSCH_CALLBACK_ACTIVATE_RX_LINK();

        /* Schedule ACK to sender after short randomized delay */
        send_ack(version, from);
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

    LOG_INFO("Sending handshake to %02x:%02x (version=%u), retry=%u\n",
             addr->u8[0], addr->u8[1], handshake_version, (unsigned)retry_count);

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

#ifdef NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK
    NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK(&parent_addr, 1);
#endif

    TSCH_CALLBACK_ACTIVATE_RX_LINK();

    /* Send broadcast to parent (we assume parent is always reachable) */
    rl_asl_ip_output(NULL);

    LOG_INFO("Handshake send attempted to %02x:%02x\n", addr->u8[0], addr->u8[1]);
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
            retry_count = 0;
            handshake_version = (uint16_t)(handshake_version + 1); /* bump version for this round */
            send_handshake_to_parent(&parent_addr);

            /* Arm resend timer: if no ACK arrives within timeout, we will handle resend.
             * initial timeout = 1 second (tunable) */
            etimer_set(&resend_timer, CLOCK_SECOND);
        }
        else
        {
            LOG_WARN("Parent address is null — cannot send handshake\n");
        }
        break;

    case HANDSHAKE_EVENT_RESEND:
        /* Called when handshake succeeded (ACK) or when process_post requested a resend action.
           We'll use ack_received to disambiguate. */
        if (ack_received)
        {
            /* ACK arrived, ensure timers are stopped */
            if (etimer_expired(&resend_timer) == 0)
                etimer_stop(&resend_timer);

            ctimer_stop(&ack_params.ack_timer);
            LOG_INFO("Handshake ACK received — stopped resend timer\n");
            ack_received = 0;
            retry_count = 0;
        }
        else
        {
            /* This path may be used to force a resend from other parts of code */
            if (retry_count < max_retries)
            {
                retry_count++;
                LOG_WARN("Forced resend (retry %u) for handshake\n", (unsigned)retry_count);
                handshake_version = (uint16_t)(handshake_version + 1);
                send_handshake_to_parent(&parent_addr);
                /* backoff: double timeout each retry (capped) */
                etimer_set(&resend_timer, CLOCK_SECOND * (1 << (retry_count > 4 ? 4 : retry_count)));
            }
            else
            {
                LOG_WARN("Max handshake retries reached (%u); giving up this round\n", (unsigned)max_retries);
                retry_count = 0;
            }
        }
        break;

    case PROCESS_EVENT_TIMER:
        if (data == &resend_timer)
        {
            /* This is the resend timeout firing */
            if (ack_received)
            {
                /* If ack was already processed we simply stop timer */
                etimer_stop(&resend_timer);
                ctimer_stop(&ack_params.ack_timer);
                ack_received = 0;
                retry_count = 0;
                LOG_INFO("Resend timer fired but ack_received was already set. Cleaning up.\n");
            }
            else
            {
                /* Timeout without ACK -> perform resend (or give up after retries) */
                if (retry_count < max_retries)
                {
                    retry_count++;
                    LOG_WARN("Handshake timeout — resending handshake (retry %u)\n", (unsigned)retry_count);
                    handshake_version = (uint16_t)(handshake_version + 1);
                    send_handshake_to_parent(&parent_addr);
                    etimer_set(&resend_timer, CLOCK_SECOND * (1 << (retry_count > 4 ? 4 : retry_count)));
                }
                else
                {
                    LOG_ERR("Handshake timeout — max retries reached (%u). Giving up.\n", (unsigned)max_retries);
                    retry_count = 0;
                }
            }
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

    /* Clear ack params initially and ctimer not armed */
    memset(&ack_params, 0, sizeof(ack_params));
    ctimer_stop(&ack_params.ack_timer);

    while (1)
    {
        PROCESS_YIELD();
        eventhandler(ev, data);
    }

    PROCESS_END();
}
