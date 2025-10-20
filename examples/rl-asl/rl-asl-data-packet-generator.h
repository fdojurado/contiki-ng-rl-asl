#ifndef RL_ASL_DATA_PACKET_GENERATOR_H
#define RL_ASL_DATA_PACKET_GENERATOR_H

#include "contiki.h"
#include "net/mac/tsch/tsch-packet.h"

#ifdef RL_ASL_DATA_PACKET_GENERATOR_CONF_TX_INTERVAL_S
#define RL_ASL_DATA_PACKET_GENERATOR_TX_INTERVAL_S RL_ASL_DATA_PACKET_GENERATOR_CONF_TX_INTERVAL_S
#else
#define RL_ASL_DATA_PACKET_GENERATOR_TX_INTERVAL_S 13 // Default transmission interval in seconds
#endif                                                /* RL_ASL_DATA_PACKET_GENERATOR_CONF_TX_INTERVAL_S */

PROCESS_NAME(data_packet_generator_process);

void data_packet_generator_ack_received(const struct tsch_packet *packet, int mac_status);

#endif // RL_ASL_DATA_PACKET_GENERATOR_H