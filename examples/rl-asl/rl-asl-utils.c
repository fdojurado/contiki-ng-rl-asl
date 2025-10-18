#include "rl-asl-utils.h"
#include "net/ipv6/uip.h"
#include "rl-asl-buf.h"
#include "rl-asl-packets.h"
#include "rl-asl-ds-nbr.h"
#include "net/mac/tsch/tsch.h"
#include "tsch-asn.h"
#include "os/sys/log.h"
#ifdef BUILD_WITH_PRIL
#include "pril-nbr.h"
#endif /* BUILD_WITH_PRIL */

#define LOG_MODULE "rl-asl-utils"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_UTILS

/*---------------------------------------------------------------------------*/
void rl_asl_init(void)
{
    LOG_INFO("RL-ASL utilities initialized\n");
    rl_asl_buf_init();
    rl_asl_ds_nbr_init();
#ifdef BUILD_WITH_PRIL
    pril_nbr_init();
#endif /* BUILD_WITH_PRIL */
}
/*---------------------------------------------------------------------------*/
double ASNToTime(uint64_t asn_val)
{
    return (double)(asn_val * tsch_timing[tsch_ts_timeslot_length]) / 1000000.0;
}
/*---------------------------------------------------------------------------*/
uint64_t timeToASN(double time_s_d)
{
    return (uint64_t)(time_s_d * 1000000.0 / tsch_timing[tsch_ts_timeslot_length]);
}
/*---------------------------------------------------------------------------*/
uint64_t ticksToASN(clock_time_t ticks)
{
    return (uint64_t)(ticks * 1000000.0 / CLOCK_SECOND / tsch_timing[tsch_ts_timeslot_length]);
}
/*---------------------------------------------------------------------------*/
uint16_t
chksum(uint16_t sum, const uint8_t *data, uint16_t len)
{
    uint16_t t;
    const uint8_t *dataptr;
    const uint8_t *last_byte;

    dataptr = data;
    last_byte = data + len - 1;

    while (dataptr < last_byte)
    { /* At least two more bytes */
        t = (dataptr[0] << 8) + dataptr[1];
        sum += t;
        if (sum < t)
        {
            sum++; /* carry */
        }
        dataptr += 2;
    }

    if (dataptr == last_byte)
    {
        t = (dataptr[0] << 8) + 0;
        sum += t;
        if (sum < t)
        {
            sum++; /* carry */
        }
    }

    /* Return sum in host byte order. */
    return sum;
}
/*---------------------------------------------------------------------------*/
uint16_t
rl_asl_ip_htons(uint16_t val)
{
    return UIP_HTONS(val);
}
/*---------------------------------------------------------------------------*/
uint32_t
rl_asl_ip_ntohl(uint32_t val)
{
    return UIP_HTONL(val);
}
/*---------------------------------------------------------------------------*/
uint64_t
rl_asl_ip_ntohl64(uint64_t val)
{
    if (UIP_BYTE_ORDER == UIP_BIG_ENDIAN)
    {
        return val; // Already in network byte order
    }
    uint32_t hi = (uint32_t)(val >> 32);
    uint32_t lo = (uint32_t)(val & 0xFFFFFFFF);
    uint64_t swapped = ((uint64_t)UIP_HTONL(lo) << 32) | UIP_HTONL(hi);
    return swapped;
}

uint64_t
rl_asl_ip_htonl64(uint64_t val)
{
#if UIP_BYTE_ORDER == UIP_BIG_ENDIAN
    return val;
#else
    uint32_t high = UIP_HTONL((uint32_t)(val >> 32));
    uint32_t low = UIP_HTONL((uint32_t)(val & 0xFFFFFFFF));
    return ((uint64_t)low << 32) | high;
#endif
}
/*---------------------------------------------------------------------------*/
uint16_t rl_asl_ip_chksum(void)
{
    uint16_t sum;

    sum = chksum(0, rl_asl_buf, RL_ASL_IPH_LEN);
    LOG_DBG("IP checksum: %04x\n", sum);
    return (sum == 0) ? 0xffff : rl_asl_ip_htons(sum);
}
/*---------------------------------------------------------------------------*/
uint16_t rl_asl_data_chksum(void)
{
    uint16_t sum;

    sum = chksum(0, RL_ASL_IP_PAYLOAD(0), RL_ASL_DATAH_LEN);
    LOG_DBG("Data checksum: %04x\n", sum);
    return (sum == 0) ? 0xffff : rl_asl_ip_htons(sum);
}
/*---------------------------------------------------------------------------*/
uint16_t rl_asl_data_chksum_from_buffer(const uint8_t *data)
{
    uint16_t sum;

    sum = chksum(0, data, RL_ASL_DATAH_LEN);
    LOG_DBG("Data checksum from buffer: %04x\n", sum);
    return (sum == 0) ? 0xffff : rl_asl_ip_htons(sum);
}
/*---------------------------------------------------------------------------*/
void print_ip_header()
{
    LOG_DBG("IP Header:\n");
    LOG_DBG("  Length: %d\n", RL_ASL_IP_BUF->len);
    LOG_DBG("  TTL: %d\n", RL_ASL_IP_BUF->ttl);
    LOG_DBG("  Protocol: %d\n", RL_ASL_IP_BUF->proto);
    LOG_DBG("  Checksum: %04x\n", RL_ASL_IP_BUF->ipchksum);
    LOG_DBG("  Source: ");
    LOG_DBG_LLADDR(&RL_ASL_IP_BUF->scr);
    LOG_DBG_("\n");
    LOG_DBG("  Destination: ");
    LOG_DBG_LLADDR(&RL_ASL_IP_BUF->dest);
    LOG_DBG_("\n");
}
/*---------------------------------------------------------------------------*/
void print_data_header()
{
    LOG_DBG("Data Header (%zu bytes):\n", sizeof(struct rl_asl_data_hdr));
    LOG_DBG("  Payload Length: %d (%zu bytes)\n", RL_ASL_DATA_BUF->payload_len, sizeof(RL_ASL_DATA_BUF->payload_len));
    LOG_DBG("  Sequence Number: %d (%zu bytes)\n", rl_asl_ip_htons(RL_ASL_DATA_BUF->seqnum), sizeof(RL_ASL_DATA_BUF->seqnum));
#ifdef BUILD_WITH_PRIL
    LOG_DBG("  Sleep End: %d (%zu bytes)\n", rl_asl_ip_htons(RL_ASL_DATA_BUF->sleep_end), sizeof(RL_ASL_DATA_BUF->sleep_end));
    LOG_DBG("  Timing T_s: %d (%zu bytes)\n", RL_ASL_DATA_BUF->timing_T_s, sizeof(RL_ASL_DATA_BUF->timing_T_s));
#endif /* BUILD_WITH_PRIL */
    LOG_DBG("  Data Checksum: %04x (%zu bytes)\n", rl_asl_ip_htons(RL_ASL_DATA_BUF->datachksum), sizeof(RL_ASL_DATA_BUF->datachksum));
    LOG_DBG("  Payload Data: ");
    for (uint16_t i = 0; i < RL_ASL_DATA_BUF->payload_len; i++)
    {
        LOG_DBG_("%02x ", RL_ASL_DATA_PAYLOAD_PTR[i]);
        if ((i + 1) % 16 == 0)
        {
            LOG_DBG_("\n");
            LOG_DBG("  Payload Data (continued): ");
        }
    }
    LOG_DBG_("\n");
}
/*---------------------------------------------------------------------------*/
void print_raw_buffer(const uint8_t *buffer, uint16_t len)
{
    LOG_DBG("Raw Buffer Data:\n");
    for (uint16_t i = 0; i < len; i++)
    {
        LOG_DBG_("%02x ", buffer[i]);
        if ((i + 1) % 16 == 0)
        {
            LOG_DBG_("\n");
        }
    }
    LOG_DBG_("\n");
}
/*---------------------------------------------------------------------------*/
bool rl_asl_update_ttl(void)
{
    if (RL_ASL_IP_BUF->ttl <= 1)
    {
        LOG_WARN("TTL expired for packet from %02x:%02x to %02x:%02x\n",
                 RL_ASL_IP_BUF->scr.u8[0], RL_ASL_IP_BUF->scr.u8[1],
                 RL_ASL_IP_BUF->dest.u8[0], RL_ASL_IP_BUF->dest.u8[1]);
        return false; // Packet should be dropped
    }
    else
    {
        RL_ASL_IP_BUF->ttl--;
        LOG_DBG("Decremented TTL, new value: %d\n", RL_ASL_IP_BUF->ttl);
        return true; // Packet can continue
    }
}
