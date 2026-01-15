/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
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
 */

#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#define LINKADDR_CONF_SIZE 2

#define RL_ASL_DS_NBR_CONF_MAX_NEIGHBOR_CACHES 50

/* Set to enable TSCH security */
#ifndef WITH_SECURITY
#define WITH_SECURITY 0
#endif /* WITH_SECURITY */

#define IEEE802154_CONF_PANID 0x81a5

#define TSCH_PACKET_CONF_EACK_WITH_SRC_ADDR 1

#define TSCH_CONF_AUTOSTART 0

#define TSCH_CALLBACK_JOINING_NETWORK rl_asl_callback_joining_network

#define RL_ASL_CONF_INIT_ENERGEST 50000L

#define RPL_MRHOF_CONF_TIME_THRESHOLD_S 30

#define TSCH_CALLBACK_PACKET_READY orchestra_callback_packet_ready
#define TSCH_CALLBACK_NEW_TIME_SOURCE orchestra_callback_new_time_source
#define NETSTACK_CONF_ROUTING_NEIGHBOR_ADDED_CALLBACK orchestra_callback_child_added
#define NETSTACK_CONF_ROUTING_NEIGHBOR_REMOVED_CALLBACK orchestra_callback_child_removed
#define TSCH_CALLBACK_ROOT_NODE_UPDATED orchestra_callback_root_node_updated
#define TSCH_CALLBACK_UC_TX_TIMESLOT orchestra_callback_uc_tx_timeslot
#define TSCH_CALLBACK_UC_RX_TIMESLOT orchestra_callback_uc_rx_timeslot

#define LOG_CONF_LEVEL_MAIN LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_RL_ASL_Q_LEARNING LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_RL_ASL_DATA_PACKET_PROCESSOR LOG_LEVEL_ERR
#define LOG_CONF_LEVEL_RL_ASL_DS_NBR LOG_LEVEL_ERR
#define LOG_CONF_LEVEL_RL_ASL_ORCHESTRA_SCHEDULE_MAP LOG_LEVEL_ERR
#define LOG_CONF_LEVEL_RL_ASL_ORCHESTRA_UNICAST LOG_LEVEL_ERR
#define LOG_CONF_LEVEL_RL_ASL_BROADCAST_SCHEDULE LOG_LEVEL_ERR
#define LOG_CONF_LEVEL_RL_ASL_DATA_PACKET_GENERATOR LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_RL_ASL LOG_LEVEL_INFO

#define LOG_CONF_LEVEL_RPL LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_IPV6 LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_6LOWPAN LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_FRAMER LOG_LEVEL_NONE
#define TSCH_LOG_CONF_PER_SLOT 1

#endif /* PROJECT_CONF_H_ */
