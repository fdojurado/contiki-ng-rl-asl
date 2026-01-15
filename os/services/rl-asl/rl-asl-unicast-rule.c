/*
 * Copyright (c) 2016, Inria.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/**
 * \file
 *         Orchestra: a slotframe dedicated to unicast data transmission. Designed primarily
 *         for RPL non-storing mode but would work with any mode-of-operation.
 *         Uses uIPv6 DS6 neighhbor tables to obtain knowledge of the children.
 *         Only supports the receiver based mode, and works as follows:
 *           Nodes listen at a timeslot defined as hash(MAC) % ORCHESTRA_SB_UNICAST_PERIOD
 *           Nodes transmit at: for any neighbor, hash(nbr.MAC) % ORCHESTRA_SB_UNICAST_PERIOD
 *
 * \author Simon Duquennoy <simon.duquennoy@inria.fr>
 *         Atis Elsts <atis.elsts@edi.lv>
 */

#include "contiki.h"
#include "orchestra.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/packetbuf.h"
#include "rl-asl-handshake.h"

static uint16_t slotframe_handle = 0;
static struct tsch_slotframe *sf_unicast;

/*---------------------------------------------------------------------------*/
static uint16_t
get_node_timeslot(const linkaddr_t *addr)
{
  if(addr != NULL && ORCHESTRA_UNICAST_PERIOD > 0) {
    return ORCHESTRA_LINKADDR_HASH(addr) % ORCHESTRA_UNICAST_PERIOD;
  } else {
    return 0xffff;
  }
}
/*---------------------------------------------------------------------------*/
static uint16_t
get_node_channel_offset(const linkaddr_t *addr)
{
  if(addr != NULL && ORCHESTRA_UNICAST_MAX_CHANNEL_OFFSET >= ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET) {
    return ORCHESTRA_LINKADDR_HASH(addr) % (ORCHESTRA_UNICAST_MAX_CHANNEL_OFFSET - ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET + 1)
        + ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET;
  } else {
    return 0xffff;
  }
}
/*---------------------------------------------------------------------------*/
static int
activate_rx_link(void)
{
  if(sf_unicast != NULL) {
    /* Add a shared Rx link at our own timeslot */
    uint16_t timeslot = get_node_timeslot(&linkaddr_node_addr);
    tsch_schedule_add_link(sf_unicast,
        LINK_OPTION_SHARED | LINK_OPTION_RX,
        LINK_TYPE_NORMAL, &tsch_broadcast_address,
        timeslot, get_node_channel_offset(&linkaddr_node_addr), 1);
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
deactivate_rx_link(void)
{
  if(sf_unicast != NULL) {
    uint16_t timeslot = get_node_timeslot(&linkaddr_node_addr);
    uint16_t channel_offset = get_node_channel_offset(&linkaddr_node_addr);
    /* Remove the Rx link at our own timeslot */
    tsch_schedule_remove_link_by_offsets(sf_unicast, timeslot, channel_offset);
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
add_uc_link(const linkaddr_t *linkaddr)
{
  if(linkaddr != NULL) {
    /* Let's first check if we already have a link to this neighbor */
    uint16_t timeslot = get_node_timeslot(linkaddr);
    if(tsch_schedule_get_link_by_offsets(sf_unicast, timeslot, 0) != NULL) {
      /* We already have a link, no need to add it again */
      return;
    }
    /* Add a Tx link to the neighbor; do not replace any existing links
     * at that cell. The channel offset here does not matter:
     * select_packet() always sets the right channel offset per packet. */
    tsch_schedule_add_link(sf_unicast,
        LINK_OPTION_SHARED | LINK_OPTION_TX,
        LINK_TYPE_NORMAL, &tsch_broadcast_address,
        timeslot, 0, 0);
  }
}
/*---------------------------------------------------------------------------*/
static void
remove_uc_link(const linkaddr_t *linkaddr)
{
  if(linkaddr != NULL) {
    uint16_t timeslot = get_node_timeslot(linkaddr);
    tsch_schedule_remove_link_by_offsets(sf_unicast, timeslot, 0);
    tsch_queue_free_packets_to(linkaddr);
  }
}
/*---------------------------------------------------------------------------*/
static void
neighbor_updated(const linkaddr_t *linkaddr, uint8_t is_added)
{
  if(is_added) {
    add_uc_link(linkaddr);
  } else {
    remove_uc_link(linkaddr);
  }
}
/*---------------------------------------------------------------------------*/
static int
select_packet(uint16_t *slotframe, uint16_t *timeslot, uint16_t *channel_offset)
{
  /* Select data packets we have a unicast link to */
  const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  if(packetbuf_attr(PACKETBUF_ATTR_FRAME_TYPE) == FRAME802154_DATAFRAME
     && !orchestra_is_root_schedule_active(dest)
     && !linkaddr_cmp(dest, &linkaddr_null)) {
    if(slotframe != NULL) {
      *slotframe = slotframe_handle;
    }
    if(timeslot != NULL) {
      *timeslot = get_node_timeslot(dest);
    }
    /* set per-packet channel offset */
    if(channel_offset != NULL) {
      *channel_offset = get_node_channel_offset(dest);
    }
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
new_time_source(const struct tsch_neighbor *old, const struct tsch_neighbor *new)
{
  if(new != old) {
    const linkaddr_t *old_addr = tsch_queue_get_nbr_address(old);
    const linkaddr_t *new_addr = tsch_queue_get_nbr_address(new);
    if(new_addr != NULL) {
      linkaddr_copy(&orchestra_parent_linkaddr, new_addr);
    } else {
      linkaddr_copy(&orchestra_parent_linkaddr, &linkaddr_null);
    }
    remove_uc_link(old_addr);
    add_uc_link(new_addr);
    /* Here we need to make sure that our parent knows that we exist, so we send
        a broadcast packet that will cause the parent to add us as its child */
    if(new_addr != NULL) {
      rl_asl_handshake_update_parent(new_addr);    
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
init(uint16_t sf_handle)
{
  slotframe_handle = sf_handle;
  /* Slotframe for unicast transmissions */
  sf_unicast = tsch_schedule_add_slotframe(slotframe_handle, ORCHESTRA_UNICAST_PERIOD);
  /* We dont initialize any links here; they will be added when we get to
     know our neighbors through the neighbor callback */
}
/*---------------------------------------------------------------------------*/
struct orchestra_rule unicast_rl_asl = {
  init,
  new_time_source,
  select_packet,
  NULL,
  NULL,
  activate_rx_link,
  deactivate_rx_link,
  NULL,
  neighbor_updated,
  NULL,
  "unicast RL ASL", // Updated description
  ORCHESTRA_UNICAST_PERIOD,
};
