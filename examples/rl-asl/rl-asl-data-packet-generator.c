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

#ifdef BUILD_WITH_PRIL
#include "pril-utils.h"
#include "pril-nbr.h"
#endif /* BUILD_WITH_PRIL */

#include "os/sys/log.h"

#define LOG_MODULE "data-packet-generator"
#define LOG_LEVEL LOG_LEVEL_DBG

/* Traffic pattern selection */
#define TRAFFIC_PATTERN_BASELINE 1
#define TRAFFIC_PATTERN_HETEROGENEOUS 2
#define TRAFFIC_PATTERN_SPARSE 3
#define TRAFFIC_PATTERN_CONCURRENT 4
#define TRAFFIC_PATTERN_PERIODIC 5

#ifndef TRAFFIC_PATTERN
#define TRAFFIC_PATTERN TRAFFIC_PATTERN_BASELINE
#endif

/* TX interval in seconds (used for baseline pattern) */
#ifndef RL_ASL_DATA_PACKET_GENERATOR_TX_INTERVAL_S
#define RL_ASL_DATA_PACKET_GENERATOR_TX_INTERVAL_S 13
#endif

static struct tsch_asn_t *asn = &tsch_current_asn;

static int16_t seqnum = 0; // Sequence number for data packets

static int8_t num_acked = 0; // Number of ACKs received

PROCESS(data_packet_generator_process, "Transmission Process");

// Forward declarations
static int get_tx_interval(void);

/*---------------------------------------------------------------------------*/
void data_packet_generator_ack_received(const struct tsch_packet *packet, int mac_status)
{
  // Inspect the seqnum of the ACKed packet
  if (packet == NULL || packet->qb == NULL)
    return;

  if (mac_status != MAC_TX_OK)
    return; // Only count successful ACKs

  int16_t seq = rl_asl_buf_get_attr(RL_ASL_BUF_ATTR_PRIL_SEQNUM);
  if (seq == seqnum - 1) // ACK for the last sent packet
    num_acked++;
}
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

#ifdef BUILD_WITH_PRIL
  pril_nbr_t *nbr = pril_nbr_get(nxthop);
  // For PRIL, set sleep_ie and timing_ie
  // we need to calculate the next asn based on the current asn and the tx interval
  int tx_interval = get_tx_interval();
  if (num_acked >= 3)
  {
    RL_ASL_DATA_BUF->sleep_end = rl_asl_ip_htons(pril_compute_cells_from_seconds(tx_interval));
    if (nbr != NULL)
    {
      int Tmin_cells = pril_compute_cells_from_seconds(tx_interval);
      if (nbr->sleep_end > 0)
      {
        nbr->new_sleep_end = Tmin_cells;
        LOG_INFO("Updating neighbor %02x:%02x new_sleep_end to %d\n",
                 nxthop->u8[0], nxthop->u8[1], nbr->new_sleep_end);
      }
      else
      {
        nbr->sleep_end = Tmin_cells;
        LOG_INFO("Updating neighbor %02x:%02x sleep_end to %d\n",
                 nxthop->u8[0], nxthop->u8[1], nbr->sleep_end);
      }
      rl_asl_buf_set_attr(RL_ASL_BUF_ATTR_PRIL_SLEEP_FLAG, 1);
      LOG_DBG("Set PRIL sleep_end to %d cells for neighbor %02x:%02x\n",
              rl_asl_ip_htons(RL_ASL_DATA_BUF->sleep_end),
              nxthop->u8[0], nxthop->u8[1]);
    }
  }
  else
  {
    RL_ASL_DATA_BUF->sleep_end = rl_asl_ip_htons(0); // No sleep initially
  }
  RL_ASL_DATA_BUF->timing_T_s = (uint8_t)tx_interval;
  if (rl_asl_buf_set_attr(RL_ASL_BUF_ATTR_PRIL_SEQNUM, seqnum))
  {
    LOG_DBG("Set RL_ASL_BUF_ATTR_PRIL_SEQNUM to %d\n", seqnum);
  }
  else
  {
    LOG_WARN("Failed to set RL_ASL_BUF_ATTR_PRIL_SEQNUM\n");
  }
#endif /* BUILD_WITH_PRIL */

  if (rl_asl_buf_set_attr(RL_ASL_BUF_ATTR_MAX_MAC_TRANSMISSIONS, 4))
  {
    LOG_DBG("Set RL_ASL_BUF_ATTR_MAX_MAC_TRANSMISSIONS to 4\n");
  }
  else
  {
    LOG_WARN("Failed to set RL_ASL_BUF_ATTR_MAX_MAC_TRANSMISSIONS\n");
  }

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
  case 3:
  case 11:
  case 15:
  case 19:
    return 17;
  case 4:
  case 12:
  case 16:
  case 20:
    return 30;
  case 5:
  case 6:
  case 13:
  case 17:
  case 21:
    return 50;
  case 14:
  case 18:
  case 22:
    return 73;
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
#elif TRAFFIC_PATTERN == TRAFFIC_PATTERN_PERIODIC
  switch (linkaddr_node_addr.u8[0])
  {
  case 3:
  case 11:
  case 15:
  case 19:
    return 17; // Flow 1
  case 4:
  case 12:
  case 16:
  case 20:
    return 19; // Flow 2
  case 5:
  case 13:
  case 17:
  case 21:
    return 23; // Flow 3
  case 14:
  case 18:
  case 22:
    return 29; // Flow 4
  default:
    return 0; // Non-senders stay silent
  }
#else
  return RL_ASL_DATA_PACKET_GENERATOR_TX_INTERVAL_S; // Default interval
#endif
}

/*---------------------------------------------------------------------------*/
#if TRAFFIC_PATTERN != TRAFFIC_PATTERN_PERIODIC
/* Helper to compute tx interval + jitter */
static clock_time_t
compute_tx_with_jitter(int tx_interval)
{
  // ±5% jitter
  double jitter_range = tx_interval * 0.05; // seconds
  double jitter_sec = ((double)rand() / RAND_MAX * 2.0 * jitter_range) - jitter_range;
  clock_time_t jitter = (clock_time_t)(jitter_sec * CLOCK_SECOND);

  return (clock_time_t)(tx_interval * CLOCK_SECOND + jitter);
}
#endif /* TRAFFIC_PATTERN != TRAFFIC_PATTERN_PERIODIC */
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(data_packet_generator_process, ev, data)
{
  static struct etimer et;
  static struct etimer assoc_check_timer;
  static int tx_interval;
  static bool first_start_done = false;

  PROCESS_BEGIN();

  tx_interval = get_tx_interval();

  if (tx_interval > 0)
  {
    LOG_INFO("Node %02x:%02x will send every ~%d seconds after TSCH association\n",
             linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], tx_interval);
  }
  else
  {
    LOG_INFO("Node %02x:%02x is not a sender in this traffic pattern\n",
             linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
  }

  LOG_INFO("Waiting for TSCH association before starting transmissions...\n");

  /* Set a periodic timer to check association */
  etimer_set(&assoc_check_timer, CLOCK_SECOND);

  while (1)
  {
    PROCESS_WAIT_EVENT();

    /* Periodically check association status */
    if (etimer_expired(&assoc_check_timer))
    {
      etimer_reset(&assoc_check_timer);

      if (!first_start_done && tsch_is_associated)
      {
        LOG_INFO("TSCH association complete, starting first TX timer (delay = %d s)\n", tx_interval);
        etimer_set(&et, tx_interval * CLOCK_SECOND);
        first_start_done = true;
      }
      continue;
    }

    /* If et fired, handle TX; but only when et was actually started (first_start_done) */
    if (first_start_done && etimer_expired(&et))
    {

      /* If TSCH lost association meanwhile, cancel sending and go back to waiting */
      if (!tsch_is_associated)
      {
        LOG_WARN("TSCH not associated at TX time — postponing transmissions\n");
        /* stop current TX timer and wait for re-association */
        first_start_done = false;
        etimer_stop(&et);
        /* ensure assoc checker is running immediately */
        etimer_set(&assoc_check_timer, CLOCK_SECOND);
        continue;
      }

#if BUILD_WITH_RL_ASL
      if (orchestra_parent_knows_us == 0)
      {
        LOG_WARN("Parent does not know us yet, skipping data packet\n");
        /* schedule next try in tx_interval (or jittered) */
#if TRAFFIC_PATTERN != TRAFFIC_PATTERN_PERIODIC
        etimer_set(&et, compute_tx_with_jitter(tx_interval));
#else
        etimer_reset(&et);
#endif
        continue;
      }
#endif

      LOG_DBG("Timer expired, sending data packet (first_start_done=%d, tsch associated=%d)\n",
              first_start_done, tsch_is_associated);
      send_data_packet();

#if TRAFFIC_PATTERN != TRAFFIC_PATTERN_PERIODIC
      etimer_set(&et, compute_tx_with_jitter(tx_interval));
#else
      etimer_reset(&et);
#endif
    }
  }
  PROCESS_END();
}
