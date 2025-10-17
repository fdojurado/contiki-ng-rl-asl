#ifndef PRIL_H
#define PRIL_H

#include "contiki.h"
#include "net/mac/tsch/tsch.h"
#include "net/linkaddr.h"

/* Called when a data packet is received */
void pril_data_packet_input(const linkaddr_t *src);
/* Check whether to skip RX for a given link */
void pril_check_skip_rx(const struct tsch_link *link, bool *skip_rx);
/* Check whether to transmit for a given link */
void pril_check_skip_tx(const struct tsch_link *link, bool *skip_tx, struct tsch_packet *current_packet);

#define CHECK_SKIP_RX(link, skip_rx) pril_check_skip_rx(link, skip_rx)
#define CHECK_SKIP_TX(link, skip_tx, current_packet) pril_check_skip_tx(link, skip_tx, current_packet)

#endif /* PRIL_H */