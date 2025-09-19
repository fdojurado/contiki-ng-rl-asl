#ifndef RL_ASL_HANDSHAKE_H
#define RL_ASL_HANDSHAKE_H

#include "contiki.h"
#include "net/linkaddr.h"


/* This function sends a handshake message to the specified address */
void send_handshake(const linkaddr_t *addr);

#endif /* RL_ASL_HANDSHAKE_H */