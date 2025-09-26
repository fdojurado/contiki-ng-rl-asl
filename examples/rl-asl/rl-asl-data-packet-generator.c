#include "rl-asl-data-packet-generator.h"
#include "net/netstack.h"
#include "rl-asl-net-processor.h"
#include "rl-asl-buf.h"
#include "rl-asl-utils.h"
#include "rl-asl-packets.h"
#include "tsch.h"
#include "net/routing/routing.h"
#include "rl-asl-routing.h"
#include "net/linkaddr.h"
#include "stdlib.h"
#if BUILD_WITH_RL_ASL
#include "orchestra.h"
#endif /* BUILD_WITH_RL_ASL */
#include "os/sys/log.h"

#define LOG_MODULE "data-packet-generator"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_DATA_PACKET_GENERATOR

/* Traffic pattern selection */
#define TRAFFIC_PATTERN_BASELINE 1
#define TRAFFIC_PATTERN_HETEROGENEOUS 2
#define TRAFFIC_PATTERN_SPARSE 3
#define TRAFFIC_PATTERN_CONCURRENT 4

#ifndef TRAFFIC_PATTERN
#define TRAFFIC_PATTERN TRAFFIC_PATTERN_BASELINE
#endif

/* TX interval in seconds (used for baseline pattern) */
#ifndef RL_ASL_DATA_PACKET_GENERATOR_TX_INTERVAL_S
#define RL_ASL_DATA_PACKET_GENERATOR_TX_INTERVAL_S 13
#endif

static struct tsch_asn_t *asn = &tsch_current_asn;

static int seqnum = 0; // Sequence number for data packets

PROCESS(data_packet_generator_process, "Transmission Process");
/*---------------------------------------------------------------------------*/
void send_data_packet(void)
{
  uint64_t full_asn = ((uint64_t)asn->ms1b << 32) | asn->ls4b;
  const linkaddr_t *nxthop;
  linkaddr_t *dest = &root_node_addr; // Use the root node address as destination
  nxthop = NETSTACK_ROUTING.nexthop(&linkaddr_node_addr, dest);
  if (nxthop == NULL)
  {
    LOG_ERR("No next hop found for %02x:%02x\n", dest->u8[0], dest->u8[1]);
    return;
  }
  RL_ASL_IP_BUF->len = RL_ASL_IPH_LEN + RL_ASL_DATAH_LEN;
  RL_ASL_IP_BUF->proto = RL_ASL_PROTO_DATA;
  RL_ASL_IP_BUF->ttl = 0x40; // Set a default TTL
  rl_asl_len = RL_ASL_IP_BUF->len;
  // print node address
  RL_ASL_IP_BUF->scr.u16 = rl_asl_ip_htons(linkaddr_node_addr.u16);
  RL_ASL_IP_BUF->dest.u16 = rl_asl_ip_htons(dest->u16);
  RL_ASL_IP_BUF->ipchksum = 0; // Clear checksum before calculating
  RL_ASL_IP_BUF->ipchksum = ~rl_asl_ip_chksum();

  print_ip_header();

  RL_ASL_DATA_BUF->payload_len = 0; // No payload for now
  RL_ASL_DATA_BUF->seqnum = rl_asl_ip_htons(seqnum);

  rl_asl_buf_set_attr(RL_ASL_BUF_ATTR_MAX_MAC_TRANSMISSIONS, 4); // Set max MAC transmissions

  RL_ASL_DATA_BUF->datachksum = 0; // Clear checksum before calculating
  RL_ASL_DATA_BUF->datachksum = ~rl_asl_data_chksum();

  print_data_header();

  print_raw_buffer((uint8_t *)RL_ASL_IP_BUF, rl_asl_len);

  LOG_INFO("Sending data packet with sequence number %d and ASN %" PRIu64 "\n",
           seqnum, full_asn);

  rl_asl_ip_output(nxthop);

  seqnum++;
}
/*---------------------------------------------------------------------------*/
/* TX intervals in seconds for patterns */
static int get_tx_interval(void)
{
#if TRAFFIC_PATTERN == TRAFFIC_PATTERN_BASELINE
  /* Every node sends every ~13s */
  return 13;

#elif TRAFFIC_PATTERN == TRAFFIC_PATTERN_HETEROGENEOUS
  /* Assign per-node periods: 5s, 13s, 30s */
  switch (linkaddr_node_addr.u8[0])
  { // last byte of node addr
  case 1:
  case 2:
    return 17;
  case 3:
  case 4:
    return 30;
  case 5:
  case 6:
    return 50;
  default:
    return 13; // fallback
  }

#elif TRAFFIC_PATTERN == TRAFFIC_PATTERN_SPARSE
  /* Assign sparse nodes to 60s or 120s */
  if (linkaddr_node_addr.u8[0] % 2 == 0)
  {
    return 60;
  }
  else
  {
    return 73;
  }

#elif TRAFFIC_PATTERN == TRAFFIC_PATTERN_CONCURRENT
  /* For concurrent flows, only selected nodes send */
  switch (linkaddr_node_addr.u8[0])
  {
  case 2:
    return 13; // Flow 1
  case 5:
    return 13; // Flow 2
  case 7:
    return 13; // Flow 3
  default:
    return 0; // Non-senders stay silent
  }
#endif
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(data_packet_generator_process, ev, data)
{
  static struct etimer et;

  PROCESS_BEGIN();

  int tx_interval = get_tx_interval();
  if (tx_interval > 0)
  {
    LOG_INFO("Node %02x:%02x sending every ~%d seconds\n",
             linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], tx_interval);
    etimer_set(&et, tx_interval * CLOCK_SECOND + ((rand() % 3) - 1) * CLOCK_SECOND);
  }
  else
  {
    LOG_INFO("Node %02x:%02x is not a sender in this traffic pattern\n",
             linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
  }

  LOG_INFO("TSCH is associated, starting transmission process\n");

  while (1)
  {
    PROCESS_WAIT_EVENT();
    if (!tsch_is_associated)
    {
      if (etimer_expired(&et))
        etimer_reset(&et);
    }
    else if (etimer_expired(&et))
    {
#if BUILD_WITH_RL_ASL
      if (orchestra_parent_knows_us == 0)
      {
        LOG_WARN("Parent does not know us yet, skipping data packet\n");
        etimer_reset(&et);
        continue;
      }
#endif
      LOG_DBG("Timer expired, sending data packet\n");
      send_data_packet();
      etimer_reset(&et);
    }
  }

  PROCESS_END();
}