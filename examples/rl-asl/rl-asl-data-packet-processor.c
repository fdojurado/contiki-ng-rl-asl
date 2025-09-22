#include "rl-asl-data-packet-processor.h"
#include "rl-asl-packets.h"
#include "rl-asl-utils.h"
#include "rl-asl-ds-nbr.h"
#include "rl-asl-net.h"
#include "rl-asl-net-processor.h"
#include "tsch.h"
#if BUILD_WITH_RL_ASL
#include "net/mac/tsch/tsch-slot-operation.h"
#endif /* BUILD_WITH_RL_ASL */
// #ifndef SAGE_MINIMAL
// #include "orchestra.h"
// #endif /* SAGE_MINIMAL */
#include "os/sys/log.h"
// #if (SAGE_ROOT || SAGE_RELAY) && !WITH_SAGE_ORCHESTRA
// #ifndef SAGE_MINIMAL
// #include "sage.h"
// #include "sage-broadcast-schedule.h"
// #endif /* SAGE_MINIMAL */
// #endif /* (SAGE_ROOT || SAGE_RELAY) && !WITH_SAGE_ORCHESTRA */

#define LOG_MODULE "rl-asl-data-packet-processor"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_DATA_PACKET_PROCESSOR

// static struct tsch_asn_t *asn = &tsch_current_asn;

/*---------------------------------------------------------------------------*/
void rl_asl_data_packet_input(const linkaddr_t *src, const uint16_t seqnum, const int8_t is_for_us)
{
    uint64_t full_asn = ((uint64_t)last_rx_asn.ms1b << 32) | last_rx_asn.ls4b;
    LOG_INFO("Processing data packet input (is_for_us=%d) at ASN %" PRIu64 "\n", is_for_us, full_asn);
#if BUILD_WITH_RL_ASL
    rl_asl_ds_nbr_update(src, seqnum, full_asn, 1);
#endif /* BUILD_WITH_RL_ASL */
}