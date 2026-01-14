#ifndef RL_ASL_HANDSHAKE_H
#define RL_ASL_HANDSHAKE_H

#include "contiki.h"
#include "net/linkaddr.h"
#include "net/ipv6/uip.h"

#define TSCH_CALLBACK_ACTIVATE_RX_LINK orchestra_callback_activate_rx_link
#define TSCH_CALLBACK_DEACTIVATE_RX_LINK orchestra_callback_deactivate_rx_link
#define TSCH_CALLBACK_DEACTIVATE_RX_PARENT_LINK orchestra_callback_deactivate_rx_parent_link

#ifndef NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK
#define NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK orchestra_callback_neighbor_updated
#endif /* NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK */
void NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK(const linkaddr_t *, uint8_t is_added);

PROCESS_NAME(rl_asl_handshake_process);

typedef struct handshake
{
    uint16_t version;
    uip_ipaddr_t ipaddr;
    linkaddr_t lladdr;
} handshake_t;

// int rl_asl_handshake_input(linkaddr_t *, linkaddr_t *);
// int rl_asl_handshake_ack_input(linkaddr_t *, linkaddr_t *);
void rl_asl_handshake_update_parent(const linkaddr_t *);

#endif /* RL_ASL_HANDSHAKE_H */