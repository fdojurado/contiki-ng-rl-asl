#ifndef RL_ASL_H
#define RL_ASL_H

#include "contiki.h"
#include "net/mac/tsch/tsch.h"

void rl_asl_on_slot_outcome(uint32_t asn_low32, bool packet_received);
void rl_asl_check_skip_rx(const struct tsch_link *link, bool *skip_rx);

#define CHECK_SKIP_RX(link, skip_rx) rl_asl_check_skip_rx(link, skip_rx)

#endif /* RL_ASL_H */
