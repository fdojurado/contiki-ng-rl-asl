/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
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
 *         A demonstration of the Orchestra MAC protocol without any networking.
 * \author
 *         fdo.jurado@gmail.com
 */

#include "contiki.h"
#include "tsch.h"
#include "rl-asl-net-processor.h"

// #if (ROOT || RELAY) && !WITH_RL_ASL_ORCHESTRA
// #include "sage-broadcast-schedule.h"
// #include "sage.h"
// #endif /* SAGE_ROOT || SAGE_RELAY */

#ifdef LEAF
#include "rl-asl-data-packet-generator.h"
#endif /* LEAF */

#if CONTIKI_TARGET_IOTLAB
#include "platform.h"
#endif /* CONTIKI_TARGET_IOTLAB */

#include "sys/log.h"
#define LOG_MODULE "main"
#define LOG_LEVEL LOG_LEVEL_MAIN

/*---------------------------------------------------------------------------*/
PROCESS(rl_asl_idle_listening_process, "RL ASL idle listening process");
AUTOSTART_PROCESSES(&rl_asl_idle_listening_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(rl_asl_idle_listening_process, ev, data)
{

  PROCESS_BEGIN();

#ifdef ROOT
  tsch_set_coordinator(1);
#endif

  NETSTACK_MAC.on();

#if CONTIKI_TARGET_IOTLAB
  // Possible values for M3 radio 3, 2.8, 2.3, 1.8, 1.3, 0.7, 0.0, -1,
  // -2, -3, -4, -5, -7, -9, -12, -17
  // see phy.h for correct value to use
  NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, PHY_POWER_0dBm);
#endif /* CONTIKI_TARGET_IOTLAB */

  process_start(&rl_asl_net_processor_process, NULL);

#if (ROOT || RELAY)
  // process_start(&sage_broadcast_schedule_process, NULL);
  // process_start(&sage_process, NULL);
#endif /* ROOT || RELAY */

#ifdef LEAF
  LOG_INFO("Starting data packet generator process\n");
  process_start(&data_packet_generator_process, NULL);
#endif /* LEAF */

  while (1)
  {
    PROCESS_YIELD();
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
