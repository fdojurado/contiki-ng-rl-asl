#ifndef RL_ASL_HANDSHAKE_H
#define RL_ASL_HANDSHAKE_H

#include "contiki.h"
#include "net/linkaddr.h"

#ifndef NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK
#define NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK orchestra_callback_neighbor_updated
#endif /* NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK */
void NETSTACK_CONF_DS6_NEIGHBOR_UPDATED_CALLBACK(const linkaddr_t *, uint8_t is_added);

PROCESS_NAME(rl_asl_handshake_process);

int rl_asl_handshake_input(linkaddr_t *, linkaddr_t *);
int rl_asl_handshake_ack_input(linkaddr_t *);
void rl_asl_handshake_update_parent(const linkaddr_t *);

#endif /* RL_ASL_HANDSHAKE_H */