#ifndef RL_ASL_DATA_PACKET_PROCESSOR_H
#define RL_ASL_DATA_PACKET_PROCESSOR_H

#include "contiki.h"
#include "net/ipv6/uip.h"

void rl_asl_data_packet_input(const uip_ipaddr_t *src,
                              const uint16_t seqnum);

#endif /* RL_ASL_DATA_PACKET_PROCESSOR_H */