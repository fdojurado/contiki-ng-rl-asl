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
#include "os/sys/log.h"

#define LOG_MODULE "data-packet-generator"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_DATA_PACKET_GENERATOR

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

  rl_asl_buf_set_attr(RL_ASL_BUF_ATTR_MAX_MAC_TRANSMISSIONS, 3); // Set max MAC transmissions

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
PROCESS_THREAD(data_packet_generator_process, ev, data)
{
  static struct etimer et;

  PROCESS_BEGIN();

  etimer_set(&et, RL_ASL_DATA_PACKET_GENERATOR_TX_INTERVAL_S * CLOCK_SECOND + ((rand() % 3) - 1) * CLOCK_SECOND);

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
      LOG_DBG("Timer expired, sending data packet\n");
      send_data_packet();
      etimer_reset(&et);
    }
  }

  PROCESS_END();
}