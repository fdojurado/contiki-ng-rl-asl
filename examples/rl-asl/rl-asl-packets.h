/*
 * Copyright (c) 2016, George Oikonomou - http://www.spd.gr
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RL_ASL_PACKETS_H_
#define RL_ASL_PACKETS_H_

#include "contiki.h"
#include "net/linkaddr.h"

#define RL_ASL_BUFSIZE 100

typedef union
{
    uint32_t u32[(RL_ASL_BUFSIZE + 3) / 4];
    uint8_t u8[RL_ASL_BUFSIZE];
} rl_asl_buf_t;

extern rl_asl_buf_t rl_asl_aligned_buf;

#define rl_asl_buf (rl_asl_aligned_buf.u8)

/* Header sizes. */
#define RL_ASL_IPH_LEN 10
#define RL_ASL_DATAH_LEN sizeof(struct rl_asl_data_hdr)

#define RL_ASL_IP_BUF ((struct rl_asl_uip_hdr *)rl_asl_buf)
#define RL_ASL_IP_PAYLOAD(ext) ((unsigned char *)rl_asl_buf + RL_ASL_IPH_LEN + (ext))

#define RL_ASL_DATA_BUF ((struct rl_asl_data_hdr *)RL_ASL_IP_PAYLOAD(0))
#define RL_ASL_DATA_PAYLOAD_PTR ((uint8_t *)RL_ASL_DATA_BUF + RL_ASL_DATAH_LEN)

// Lets define the packet structure
struct rl_asl_uip_hdr
{
    uint8_t len; // Length of the packet
    uint8_t ttl; // Time to live
    uint8_t proto;
    uint8_t paddng;
    linkaddr_t scr, dest;
    int16_t ipchksum;
};

struct rl_asl_data_hdr
{
    uint8_t payload_len; // Length of the payload
    uint16_t seqnum;      // Sequence number
    int16_t datachksum;  // Checksum for the data
} __attribute__((packed));

#define RL_ASL_PROTO_DATA 0x01

#define RL_ASL_LINK_MTU 1000

#endif /* RL_ASL_PACKETS_H_ */