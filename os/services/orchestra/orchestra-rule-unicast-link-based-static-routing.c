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
#if WITH_RL_ASL_ROUTING
#include "contiki.h"
#include "orchestra.h"
#include "net/routing/routing.h"
#include "rl-asl-routing.h"
#include "net/packetbuf.h"
#include "os/sys/log.h"

#define LOG_MODULE "Orchestra Unicast Link-Based Static"
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

    /* Add Tx link */
    tsch_schedule_add_link(sf_unicast, LINK_OPTION_TX | LINK_OPTION_SHARED, LINK_TYPE_NORMAL, &tsch_broadcast_address,
                           timeslot_tx, local_channel_offset, 0);
    /* Add Rx link */
    tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX, LINK_TYPE_NORMAL, &tsch_broadcast_address,
                           timeslot_rx, local_channel_offset, 0);
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
  LOG_DBG("Initializing unicast link-based static Orchestra rule with slotframe handle %u\n", slotframe_handle);
  local_channel_offset = get_node_channel_offset(&linkaddr_node_addr);
  /* Slotframe for unicast transmissions */
  sf_unicast = tsch_schedule_add_slotframe(slotframe_handle, ORCHESTRA_UNICAST_PERIOD);
  // We need to schedule a link to each of our children and a tx link to the parent
  const linkaddr_t *parent_addr = NETSTACK_ROUTING.nexthop(&linkaddr_node_addr, &root_node_addr);
  if (parent_addr)
  {
    add_uc_links(parent_addr);
    LOG_DBG("Added Tx link for parent %02x:%02x at timeslot %u and channel offset %u\n",
            parent_addr->u8[0], parent_addr->u8[1],
            get_node_pair_timeslot(&linkaddr_node_addr, parent_addr),
            local_channel_offset);
  }

  const routing_entry_t *rt_table = routing_table;
  while (rt_table->src.u8[0] != 0 || rt_table->src.u8[1] != 0)
  {
    if (linkaddr_cmp(&rt_table->next_hop, &linkaddr_node_addr))
    {
      add_uc_links(&rt_table->src);
      LOG_DBG("Added Rx link for neighbor %02x:%02x at timeslot %u and channel offset %u\n",
              rt_table->src.u8[0], rt_table->src.u8[1],
              get_node_pair_timeslot(&linkaddr_node_addr, &rt_table->src),
              local_channel_offset);
    }
    rt_table++;
  }
}

/*---------------------------------------------------------------------------*/
struct orchestra_rule unicast_per_neighbor_link_based_static = {
    init,
    new_time_source,
    select_packet,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "unicast per neighbor link based static",
    ORCHESTRA_UNICAST_PERIOD,
};
#endif /* WITH_RL_ASL_ROUTING */
