#ifndef PRIL_H
#define PRIL_H

#include "contiki.h"
#include "net/mac/tsch/tsch.h"
#include "net/linkaddr.h"

/* Called when a data packet has been sent */
void pril_packet_sent(int mac_status);
/* Called when a data packet is received */
void pril_data_packet_input(const linkaddr_t *src, int16_t seqnum, bool is_for_us);
/* Check whether to skip RX for a given link */
void pril_check_skip_rx(const struct tsch_link *link, bool *skip_rx);
/* Check whether to transmit for a given link */
void pril_check_skip_tx(const struct tsch_link *link, bool *skip_tx);
/* Update PRIL state machines on slot tick for a given link */
void pril_slot_tick_for_link(const struct tsch_link *link);
/* Attach sleep IE if this is the last packet before sleep */
void pril_attach_sleep_if_last(const struct tsch_link *link, const struct tsch_packet *current_packet);

#define CHECK_SKIP_RX(link, skip_rx) pril_check_skip_rx(link, skip_rx)
#define CHECK_SKIP_TX(link, skip_tx) pril_check_skip_tx(link, skip_tx)
#define SLOT_TICK_FOR_LINK(link) pril_slot_tick_for_link(link)
#define ATTACH_SLEEP_IF_LAST(link, packet) pril_attach_sleep_if_last(link, packet)

#endif /* PRIL_H */