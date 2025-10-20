/*
 * Copyright (c) 2017, RISE SICS.
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
 * This file is part of the Contiki operating system.
 *
 */
#ifndef RL_ASL_BUF_H_
#define RL_ASL_BUF_H_

#include "contiki.h"
#include "rl-asl-packets.h"
#include "stdbool.h"

#define RL_ASL_MAX_MAC_TRANSMISSIONS_UNDEFINED 0

#define RL_ASL_BUF_ATTR_LLSEC_LEVEL_MAC_DEFAULT 0xffff

void rl_asl_buf_clear(void);

bool rl_asl_buf_set_len(uint16_t len);

void rl_asl_set_ip_len_field(struct rl_asl_uip_hdr *hdr, uint16_t len);

uint8_t rl_asl_get_ip_len_field(struct rl_asl_uip_hdr *hdr);

uint8_t *rl_asl_buf_get_ip_next_header(uint8_t *buffer, uint16_t size, uint8_t *protocol);

uint16_t rl_asl_buf_get_attr(uint8_t type);

int rl_asl_buf_set_attr(uint8_t type, uint16_t value);

int rl_asl_buf_set_default_attr(uint8_t type, uint16_t value);

void rl_asl_buf_clear_attr(void);

void rl_asl_buf_init(void);

enum
{
    RL_ASL_BUF_ATTR_LLSEC_LEVEL = 0,
    RL_ASL_BUF_ATTR_LLSEC_KEY_ID,
    RL_ASL_BUF_ATTR_INTERFACE_ID,
    RL_ASL_BUF_ATTR_NETWORK_ID,
#ifdef BUILD_WITH_PRIL
    RL_ASL_BUF_ATTR_PRIL_SLEEP_FLAG,
    RL_ASL_BUF_ATTR_PRIL_SEQNUM,
#endif /* BUILD_WITH_PRIL */
    RL_ASL_BUF_ATTR_PHYSICAL_NETWORK_ID,
    RL_ASL_BUF_ATTR_MAX_MAC_TRANSMISSIONS,
    RL_ASL_BUF_ATTR_MAX
};

#endif /* RL_ASL_BUF_H_ */
