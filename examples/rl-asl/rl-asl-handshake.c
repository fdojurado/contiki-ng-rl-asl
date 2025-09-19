#include "rl-asl-handshake.h"

/* Log configuration */
#include "os/sys/log.h"
#define LOG_MODULE "RL ASL Handshake"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_NET_PROCESSOR

/***************************************************************/
void send_handshake(const linkaddr_t *addr)
{
    LOG_INFO("Sending handshake to %02x:%02x\n", addr->u8[0], addr->u8[1]);
    // rl_asl_buf_clear();
    // RL_ASL_IP_BUF->vhl = 0x40 | 1; // Version 1, header length 1 (4 bytes)
    // RL_ASL_IP_BUF->tos = 0;
    // RL_ASL_IP_BUF->len = rl_asl_ip_htons(4); // Total length
    // RL_ASL_IP_BUF->id = 0;
    // RL_ASL_IP_BUF->offset = 0;
    // RL_ASL_IP_BUF->ttl = RL_ASL_DEFAULT_TTL;
    // RL_ASL_IP_BUF->proto = RL_ASL_PROTO_HANDSHAKE;
    // RL_ASL_IP_BUF->scr.u16 = rl_asl_ip_htons(linkaddr_node_addr.u16);
    // RL_ASL_IP_BUF->dest.u16 = rl_asl_ip_htons(addr->u16);
    // rl_asl_len = 4;
    // rl_asl_ip_output(addr);
}