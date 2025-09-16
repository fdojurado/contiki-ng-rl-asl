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

/**
 * \file
 *         Net processor header for the SageNet layer.
 * \author
 *         Fernando Jurado <fdo.jurado@gmail.com>
 *
 */

/**
 * \ingroup net-layer
 * \defgroup rl-asl-net-processor RL ASL Net Processor
 A network layer that does nothing. Useful for lower-layer testing and
 * for non-IPv6 scenarios.
 * @{
 */

#ifndef RL_ASL_NET_PROCESSOR_H_
#define RL_ASL_NET_PROCESSOR_H_

#include "contiki.h"
#include "net/linkaddr.h"

extern uint16_t rl_asl_len;

void rl_asl_ip_input(void);

uint8_t rl_asl_ip_output(const linkaddr_t *);

void rl_asl_ip_process(void);

void rl_asl_output();

void rl_asl_callback_joining_network(void);

PROCESS_NAME(rl_asl_net_processor_process);

#endif /* RL_ASL_NET_PROCESSOR_H_ */
       /** @} */