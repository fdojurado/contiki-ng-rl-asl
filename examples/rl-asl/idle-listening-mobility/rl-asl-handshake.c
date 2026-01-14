#include "rl-asl-handshake.h"
#include "net/ipv6/uip-icmp6.h"
#include "rl-asl-ds-nbr.h"
// #include "rl-asl-utils.h"
#include "rl-asl-packets.h"
#include "os/services/orchestra/orchestra.h"
// #include "rl-asl-buf.h"
#if (LEAF || RELAY) && BUILD_WITH_RL_ASL
#include "rl-asl-conf.h"
#endif /* LEAF || RELAY && BUILD_WITH_RL_ASL */
#include "net/routing/routing.h"
#include "rl-asl-ds-nbr.h"

#include <stdlib.h>
#include <string.h>

/* Contiki timing/random helpers */
#include "lib/random.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"

/* Log configuration */
#include "os/sys/log.h"
#define LOG_MODULE "rl-asl-handshake"
#define LOG_LEVEL LOG_LEVEL_DBG

PROCESS(rl_asl_handshake_process, "RL ASL Handshake Process");

uip_ipaddr_t mcast_addr;

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
    uip_ipaddr_t dest;
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
static void send_ack(uint16_t version, const uip_ipaddr_t *dest);
static void send_handshake_to_parent(const linkaddr_t *addr);
static void eventhandler(process_event_t ev, process_data_t data);

/* Compute backoff (in clock ticks) for a given retry_count.
 * base = CLOCK_SECOND (1s), exponential backoff 2^retry (capped at 2^4),
 * and add up to +50% random jitter to desynchronize retries. */
static clock_time_t
compute_backoff_ticks(uint8_t retry)
{
    uint8_t capped = (retry > 4) ? 4 : retry;
    /* base backoff */
    clock_time_t base = CLOCK_SECOND * (1u << capped);

    /* jitter up to 50% of base */
    uint32_t jitter_max = (uint32_t)base / 2u;
    uint32_t r = random_rand(); /* 0..65535 */
    uint32_t jitter = (jitter_max == 0) ? 0 : (r % (jitter_max + 1));

    return base + (clock_time_t)jitter;
}
/*---------------------------------------------------------------------------*/
#define ICMP6_RL_ASL ICMP6_PRIV_EXP_100
#define CODE_HS 0x00
#define CODE_HS_ACK 0x01
static void handshake_input(void);
static void rl_asl_handshake_ack_input(void);
/*---------------------------------------------------------------------------*/
UIP_ICMP6_HANDLER(hs_handler, ICMP6_RL_ASL, CODE_HS, handshake_input);
UIP_ICMP6_HANDLER(hs_ack_handler, ICMP6_RL_ASL, CODE_HS_ACK, rl_asl_handshake_ack_input);
/*---------------------------------------------------------------------------*/
static void process_handshake(handshake_t *hs)
{
    LOG_INFO("processing handshake version %u from ", hs->version);
    LOG_INFO_6ADDR(&hs->ipaddr);
    LOG_INFO_("\n");

    /* ALWAYS update neighbor knowledge */
    if (!rpl_icmp6_update_nbr_table(&hs->ipaddr, NBR_TABLE_REASON_RPL_DIO, NULL))
    {
        LOG_ERR("IPv6 cache full, dropping DIO\n");
        return;
    }
}
/*---------------------------------------------------------------------------*/
static void
handshake_input(void)
{
    unsigned char *buffer;
    linkaddr_t dest_addr;
    handshake_t last_handshake;

    uip_ipaddr_t src;
    uip_ipaddr_copy(&src, &UIP_IP_BUF->srcipaddr);

    LOG_INFO("Received handshake packet from ");
    LOG_INFO_6ADDR(&src);
    LOG_INFO_("\n");

    buffer = UIP_ICMP_PAYLOAD;
    if (buffer[0] != RL_ASL_PROTO_HANDSHAKE)
    {
        LOG_WARN("Handshake packet with invalid protocol identifier: %u\n", buffer[0]);
        uipbuf_clear();
        return;
    }

    // extract dest address
    memcpy(dest_addr.u8, &buffer[1], LINKADDR_SIZE);

    // is this my address?
    if (!linkaddr_cmp(&dest_addr, &linkaddr_node_addr))
    {
        LOG_WARN("Handshake packet not intended for this node (ignoring)");
        LOG_WARN_LLADDR(&dest_addr);
        LOG_WARN_("\n");
        uipbuf_clear();
        return;
    }

    last_handshake.version = (buffer[1 + LINKADDR_SIZE] << 8) | buffer[2 + LINKADDR_SIZE];
    uip_ipaddr_copy(&last_handshake.ipaddr, &UIP_IP_BUF->srcipaddr);

    process_handshake(&last_handshake);

    const uip_lladdr_t *lladdr = uip_ds6_nbr_lladdr_from_ipaddr(&src);
    if (lladdr == NULL)
    {
        LOG_WARN("Could not find link-layer address for neighbor ");
        LOG_WARN_6ADDR(&src);
        LOG_WARN_("\n");
        uipbuf_clear();
        return;
    }

    linkaddr_copy(&last_handshake.lladdr, (const linkaddr_t *)lladdr);

    LOG_INFO("Handshake packet accepted for this node (version=%u) from ", last_handshake.version);
    LOG_INFO_6ADDR(&src);
    LOG_INFO_(", dest addr=");
    LOG_INFO_LLADDR(&dest_addr);
    LOG_INFO_(", src addr=");
    LOG_INFO_LLADDR(&last_handshake.lladdr);
    LOG_INFO_(", node IP=");
    LOG_INFO_6ADDR(&last_handshake.ipaddr);
    LOG_INFO_("\n");

#ifdef NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK
    NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK((const linkaddr_t *)lladdr, 1);
#endif

#if (LEAF || RELAY) && BUILD_WITH_RL_ASL
    rl_asl_ds_nbr_update((const linkaddr_t *)lladdr, 0, 0, 1); // Add/update neighbor with seqno 0 and ASN 0
#endif                                                         /* LEAF || RELAY && BUILD_WITH_RL_ASL */

#if (LEAF || RELAY) && BUILD_WITH_RL_ASL
    TSCH_CALLBACK_ACTIVATE_RX_LINK();
#endif /* LEAF || RELAY && BUILD_WITH_RL_ASL */

    send_ack(last_handshake.version, &src);

    uipbuf_clear();
}
/*---------------------------------------------------------------------------*/
static void
rl_asl_handshake_ack_input(void)
{
    LOG_INFO("Received handshake ACK packet\n");

#if (ROOT) && BUILD_WITH_RL_ASL
    return;
#endif /* ROOT && BUILD_WITH_RL_ASL */

    unsigned char *buffer;
    uint16_t version;
    uip_ipaddr_t scr;

    uip_ipaddr_copy(&scr, &UIP_IP_BUF->srcipaddr);

    buffer = UIP_ICMP_PAYLOAD;
    if (buffer[0] != RL_ASL_PROTO_HANDSHAKE_ACK)
    {
        LOG_WARN("Handshake ACK packet with invalid protocol identifier: %u\n", buffer[0]);
        uipbuf_clear();
        return;
    }
    version = (buffer[1 + LINKADDR_SIZE] << 8) | buffer[2 + LINKADDR_SIZE];
    LOG_INFO("Handshake ACK version %u received\n", version);

    uint16_t expected = handshake_version;
    if (version != expected)
    {
        LOG_WARN("Unexpected handshake ACK version %u (expected %u)\n", version, expected);
        uipbuf_clear();
        return;
    }

    if (!rpl_icmp6_update_nbr_table(&scr, NBR_TABLE_REASON_RPL_DIO, NULL))
    {
        LOG_ERR("IPv6 cache full, dropping DIO\n");
        uipbuf_clear();
        return;
    }

    LOG_INFO("Handshake ACK matches expected version %u\n", expected);
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

    const uip_lladdr_t *lladdr = uip_ds6_nbr_lladdr_from_ipaddr(&scr);
    if (lladdr == NULL)
    {
        LOG_WARN("Could not find link-layer address for neighbor ");
        LOG_WARN_6ADDR(&scr);
        LOG_WARN_("\n");
        uipbuf_clear();
        return;
    }

    TSCH_CALLBACK_DEACTIVATE_RX_PARENT_LINK((const linkaddr_t *)lladdr);

    uipbuf_clear();
}
/*---------------------------------------------------------------------------*/
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

    LOG_INFO("Sending handshake ACK version: %u to ", params->version);
    LOG_INFO_6ADDR(&params->dest);
    LOG_INFO_("\n");

    unsigned char *buffer;
    buffer = UIP_ICMP_PAYLOAD;
    buffer[0] = RL_ASL_PROTO_HANDSHAKE_ACK;
    buffer[1] = (params->version >> 8) & 0xFF;
    buffer[2] = params->version & 0xFF;

    uip_icmp6_send(&params->dest, ICMP6_RL_ASL, CODE_HS_ACK, 3);
}
/***************************************************************/
/* Schedule an ACK after a short randomized delay to avoid collisions.
 * We use the static ack_params instance so do_ack has valid storage. */
static void
send_ack(uint16_t version, const uip_ipaddr_t *dest)
{
    if (dest == NULL)
    {
        LOG_ERR("send_ack: dest is NULL\n");
        return;
    }

    /* Cancel any existing ack ctimer first to avoid clobbering params */
    ctimer_stop(&ack_params.ack_timer);

    ack_params.version = version;
    uip_ipaddr_copy(&ack_params.dest, dest);

    /* Small randomized delay window to avoid ACK collisions.
     * Use up to 1/4 second random delay (tunable). */
    clock_time_t max_ack_jitter = CLOCK_SECOND / 4u;
    if (max_ack_jitter == 0)
    {
        /* If CLOCK_SECOND is small (unlikely), fall back to 1 tick */
        ctimer_set(&ack_params.ack_timer, 1, do_ack, &ack_params);
        LOG_DBG("ACK scheduled in %lu ticks (fallback 1) for ",
                (unsigned long)1);
        LOG_DBG_6ADDR(dest);
        LOG_DBG_(", version=%u\n", ack_params.version);
    }
    else
    {
        uint32_t r = random_rand();
        clock_time_t delay = (clock_time_t)(r % (max_ack_jitter + 1));
        ctimer_set(&ack_params.ack_timer, delay, do_ack, &ack_params);
        LOG_DBG("ACK scheduled in %lu ticks for ",
                (unsigned long)delay);
        LOG_DBG_6ADDR(dest);
        LOG_DBG_(", version=%u\n", ack_params.version);
    }
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
send_handshake_to_parent(const linkaddr_t *dest)
{
    unsigned char *buffer;

    if (dest == NULL)
    {
        LOG_ERR("send_handshake_to_parent: dest NULL\n");
        return;
    }

    LOG_INFO("Sending handshake to %02x:%02x (version=%u), retry=%u\n",
             dest->u8[0], dest->u8[1], handshake_version, (unsigned)retry_count);

    buffer = UIP_ICMP_PAYLOAD;
    buffer[0] = RL_ASL_PROTO_HANDSHAKE;
    // copy dest address
    memcpy(&buffer[1], dest->u8, LINKADDR_SIZE);
    // copy version
    buffer[1 + LINKADDR_SIZE] = (handshake_version >> 8) & 0xFF;
    buffer[2 + LINKADDR_SIZE] = handshake_version & 0xFF;

#ifdef NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK
    NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK(dest, 1);
#endif

    TSCH_CALLBACK_ACTIVATE_RX_LINK();

    LOG_INFO("Handshake send attempted to %02x:%02x\n", dest->u8[0], dest->u8[1]);

    uip_icmp6_send(&mcast_addr, ICMP6_RL_ASL, CODE_HS, 1 + LINKADDR_SIZE + 2);
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
             * initial timeout = randomized around 1 second (compute_backoff_ticks with retry_count=0) */
            etimer_set(&resend_timer, compute_backoff_ticks(0));
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
                /* backoff: exponential with jitter */
                etimer_set(&resend_timer, compute_backoff_ticks(retry_count));
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
                    etimer_set(&resend_timer, compute_backoff_ticks(retry_count));
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

    uip_ip6addr(&mcast_addr, 0xff02, 0, 0, 0, 0, 0, 0, 0x1a);

    uip_icmp6_register_input_handler(&hs_handler);
    uip_icmp6_register_input_handler(&hs_ack_handler);

    rl_asl_ds_nbr_init();

    /* Seed PRNG to make randomized backoff effective.
     * Good seeds include node address + clock time. */
    random_init((unsigned short)(linkaddr_node_addr.u8[0] + linkaddr_node_addr.u8[1] + (unsigned)clock_time()));

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
