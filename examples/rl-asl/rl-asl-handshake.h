#ifndef RL_ASL_HANDSHAKE_H
#define RL_ASL_HANDSHAKE_H

#include "contiki.h"
#include "net/linkaddr.h"

PROCESS_NAME(rl_asl_handshake_process);

void rl_asl_handshake_update_parent(const linkaddr_t *addr);

#endif /* RL_ASL_HANDSHAKE_H */