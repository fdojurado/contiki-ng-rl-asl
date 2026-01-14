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
 *         RL ASL version of Unicast Rule for Orchestra
 *
 * \author Fernando Jurado-Lasso <fdo.jurado@gmail.com>
 */

#include "contiki.h"
#include "orchestra.h"
#include "net/routing/routing.h"
#if WITH_RL_ASL_ROUTING
#include "rl-asl-routing.h"
#endif /* WITH_RL_ASL_ROUTING */
#include "net/packetbuf.h"
#include "os/sys/log.h"

#define LOG_MODULE "Orchestra RL ASL Link-Based"
#define LOG_LEVEL LOG_LEVEL_DBG

static uint16_t slotframe_handle = 0;
static uint16_t local_channel_offset;
static struct tsch_slotframe *sf_unicast;

/*---------------------------------------------------------------------------*/
static uint16_t
get_node_pair_timeslot(const linkaddr_t *from, const linkaddr_t *to)
{
  if (from != NULL && to != NULL && ORCHESTRA_UNICAST_PERIOD > 0)
  {
    return ORCHESTRA_LINKADDR_HASH2(from, to) % ORCHESTRA_UNICAST_PERIOD;
  }
  else
  {
    return 0xffff;
  }
}
/*---------------------------------------------------------------------------*/
static uint16_t
get_node_channel_offset(const linkaddr_t *addr)
{
  if (addr != NULL && ORCHESTRA_UNICAST_MAX_CHANNEL_OFFSET >= ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET)
  {
    return ORCHESTRA_LINKADDR_HASH(addr) % (ORCHESTRA_UNICAST_MAX_CHANNEL_OFFSET - ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET + 1) + ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET;
  }
  else
  {
    return 0xffff;
  }
}
/*---------------------------------------------------------------------------*/
static int
neighbor_has_uc_link(const linkaddr_t *linkaddr)
{
  if (linkaddr == NULL || linkaddr_cmp(linkaddr, &linkaddr_null))
  {
    return 0;
  }

  if (linkaddr_cmp(&orchestra_parent_linkaddr, linkaddr))
  {
    /* The node is our parent */
    return 1;
  }

  // if (nbr_table_get_from_lladdr(nbr_routes, (linkaddr_t *)linkaddr) != NULL)
  // {
  //   /* We have a route to this node;
  //    * it should have selected us as its parent and installed a link */
  //   return 1;
  // }

  return 0;
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static void
add_uc_links(const linkaddr_t *linkaddr)
{
  if (linkaddr != NULL)
  {
    uint16_t timeslot_rx = get_node_pair_timeslot(linkaddr, &linkaddr_node_addr);
    uint16_t timeslot_tx = get_node_pair_timeslot(&linkaddr_node_addr, linkaddr);

    /* Check if Tx link already exists */
    struct tsch_link *l = list_head(sf_unicast->links_list);
    int tx_exists = 0, rx_exists = 0;
    while (l != NULL) {
      if (l->timeslot == timeslot_tx && l->channel_offset == local_channel_offset &&
          (l->link_options & (LINK_OPTION_TX | LINK_OPTION_SHARED)) == (LINK_OPTION_TX | LINK_OPTION_SHARED)) {
        tx_exists = 1;
      }
      if (l->timeslot == timeslot_rx && l->channel_offset == local_channel_offset &&
          (l->link_options & LINK_OPTION_RX)) {
        rx_exists = 1;
      }
      l = list_item_next(l);
    }

    /* Add Tx link if it does not exist */
    if (!tx_exists) {
      tsch_schedule_add_link(sf_unicast, LINK_OPTION_TX | LINK_OPTION_SHARED, LINK_TYPE_NORMAL, &tsch_broadcast_address,
                             timeslot_tx, local_channel_offset, 0);
    }
    /* Add Rx link if it does not exist */
    if (!rx_exists) {
      tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX, LINK_TYPE_NORMAL, &tsch_broadcast_address,
                             timeslot_rx, local_channel_offset, 0);
    }
  }
}
/*---------------------------------------------------------------------------*/

static void
remove_unicast_link(uint16_t timeslot, uint16_t options)
{
  struct tsch_link *l = list_head(sf_unicast->links_list);
  while (l != NULL)
  {
    if (l->timeslot == timeslot && l->channel_offset == local_channel_offset && l->link_options == options)
    {
      tsch_schedule_remove_link(sf_unicast, l);
      break;
    }
    l = list_item_next(l);
  }
}
/*---------------------------------------------------------------------------*/
static void
remove_uc_links(const linkaddr_t *linkaddr)
{
  if (linkaddr != NULL)
  {
    uint16_t timeslot_rx = get_node_pair_timeslot(linkaddr, &linkaddr_node_addr);
    uint16_t timeslot_tx = get_node_pair_timeslot(&linkaddr_node_addr, linkaddr);

    remove_unicast_link(timeslot_rx, LINK_OPTION_RX);
    remove_unicast_link(timeslot_tx, LINK_OPTION_TX | LINK_OPTION_SHARED);

    /* Packets to this address were marked with this slotframe and neighbor-specific timeslot;
     * make sure they don't remain stuck in the queues after the link is removed. */
    tsch_queue_free_packets_to(linkaddr);
  }
}
/*---------------------------------------------------------------------------*/
static int
select_packet(uint16_t *slotframe, uint16_t *timeslot, uint16_t *channel_offset)
{
  /* Select data packets we have a unicast link to */
  const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  if (packetbuf_attr(PACKETBUF_ATTR_FRAME_TYPE) == FRAME802154_DATAFRAME && !orchestra_is_root_schedule_active(dest) && neighbor_has_uc_link(dest))
  {
    if (slotframe != NULL)
    {
      *slotframe = slotframe_handle;
    }
    if (timeslot != NULL)
    {
      *timeslot = get_node_pair_timeslot(&linkaddr_node_addr, dest);
    }
    /* set per-packet channel offset */
    if (channel_offset != NULL)
    {
      *channel_offset = get_node_channel_offset(dest);
    }
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
deactivate_rx_link(void)
{
  // Remove all RX links and remove RX options from TX+RX links
  struct tsch_link *l = list_head(sf_unicast->links_list);
  while (l != NULL)
  {
    // Save next link in case we remove the current one
    struct tsch_link *next = list_item_next(l);

    if (l->link_options & LINK_OPTION_RX)
    {
      if (l->link_options & LINK_OPTION_TX)
      {
        // Remove RX option from TX+RX link
        l->link_options &= ~LINK_OPTION_RX;
        LOG_DBG("Deactivated RX option from link at timeslot %u and channel offset %u\n",
                l->timeslot, l->channel_offset);
      }
      else
      {
        // Remove RX-only link, reset head since list may have changed
        tsch_schedule_remove_link(sf_unicast, l);
        LOG_DBG("Removed RX link at timeslot %u and channel offset %u\n",
                l->timeslot, l->channel_offset);
        // After removal, restart from head
        l = list_head(sf_unicast->links_list);
        continue;
      }
    }
    l = next;
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
static int
deactivate_rx_parent_link(const linkaddr_t *parent_addr)
{
  if (parent_addr == NULL || linkaddr_cmp(parent_addr, &linkaddr_null))
  {
    return 0;
  }

  uint16_t timeslot_rx = get_node_pair_timeslot(parent_addr, &linkaddr_node_addr);

  struct tsch_link *l = list_head(sf_unicast->links_list);
  while (l != NULL)
  {
    // Save next link in case we remove the current one
    struct tsch_link *next = list_item_next(l);

    if (l->timeslot == timeslot_rx && (l->link_options & LINK_OPTION_RX))
    {
      if (l->link_options & LINK_OPTION_TX)
      {
        // Remove RX option from TX+RX link
        l->link_options &= ~LINK_OPTION_RX;
        LOG_DBG("Deactivated RX option from parent link at timeslot %u and channel offset %u\n",
                l->timeslot, l->channel_offset);
      }
      else
      {
        // Remove RX-only link, reset head since list may have changed
        tsch_schedule_remove_link(sf_unicast, l);
        LOG_DBG("Removed RX parent link at timeslot %u and channel offset %u\n",
                l->timeslot, l->channel_offset);
        // After removal, restart from head
        l = list_head(sf_unicast->links_list);
        continue;
      }
    }
    l = next;
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
static void
neighbor_updated(const linkaddr_t *linkaddr, uint8_t is_added)
{
  if (is_added)
  {
    add_uc_links(linkaddr);
  }
  else
  {
    remove_uc_links(linkaddr);
  }
}
/*---------------------------------------------------------------------------*/
static void
new_time_source(const struct tsch_neighbor *old, const struct tsch_neighbor *new)
{
  if (new != old)
  {
    const linkaddr_t *old_addr = tsch_queue_get_nbr_address(old);
    const linkaddr_t *new_addr = tsch_queue_get_nbr_address(new);
    if (new_addr != NULL)
    {
      linkaddr_copy(&orchestra_parent_linkaddr, new_addr);
    }
    else
    {
      linkaddr_copy(&orchestra_parent_linkaddr, &linkaddr_null);
    }
    remove_uc_links(old_addr);
    add_uc_links(new_addr);
  }
}
/*---------------------------------------------------------------------------*/
static void
init(uint16_t sf_handle)
{
  slotframe_handle = sf_handle;
  local_channel_offset = get_node_channel_offset(&linkaddr_node_addr);
  /* Slotframe for unicast transmissions */
  sf_unicast = tsch_schedule_add_slotframe(slotframe_handle, ORCHESTRA_UNICAST_PERIOD);
}

/*---------------------------------------------------------------------------*/
struct orchestra_rule rl_asl_link_based = {
    init,
    new_time_source,
    select_packet,
    NULL,
    NULL,
    NULL,
    deactivate_rx_link,
    deactivate_rx_parent_link,
    neighbor_updated,
    NULL,
    "rl-asl link based",
    ORCHESTRA_UNICAST_PERIOD,
};
