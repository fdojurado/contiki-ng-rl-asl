#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch.h"
#include <stdint.h>
#include <inttypes.h>

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY 1
#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define SEND_INTERVAL (10 * CLOCK_SECOND)

#if LEAF
static struct tsch_asn_t *asn = &tsch_current_asn;
#endif /* LEAF */

#if LEAF
static struct simple_udp_connection udp_conn;
#endif /* LEAF */

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
#if LEAF
  static struct etimer periodic_timer;
  uip_ipaddr_t dest_ipaddr;
  static char payload[3];
  static int16_t seqnum = 0;
  static uint64_t full_asn;
#endif /* LEAF */

  PROCESS_BEGIN();

  NETSTACK_MAC.on();

#if LEAF
  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, NULL);
#endif /* LEAF */

#if LEAF
  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);
#endif /* LEAF */

  while (1)
  {
#if LEAF
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
#else
    PROCESS_YIELD();
#endif /* LEAF */

#if LEAF
    if (NETSTACK_ROUTING.node_is_reachable() &&
        NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr))
    {

      /* Send to DAG root */
      payload[0] = 27; // Protocol identifier
      payload[1] = (seqnum >> 8) & 0xff;
      payload[2] = seqnum & 0xff;

      full_asn = ((uint64_t)asn->ms1b << 32) | asn->ls4b;

      LOG_INFO("Sending data packet with sequence number %d and ASN %" PRIu64 "\n",
               seqnum, full_asn);
      simple_udp_sendto(&udp_conn, payload, sizeof(payload), &dest_ipaddr);
      seqnum++;
    }

    /* Add some jitter */
    etimer_set(&periodic_timer, SEND_INTERVAL - CLOCK_SECOND + (random_rand() % (2 * CLOCK_SECOND)));
#endif /* LEAF */
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
