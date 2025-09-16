#ifndef RL_ASL_UTILS_H_
#define RL_ASL_UTILS_H_

#include "contiki.h"

void rl_asl_init(void);

double ASNToTime(uint64_t asn_val);

uint64_t timeToASN(double time_s_d);

uint64_t ticksToASN(clock_time_t ticks);

uint16_t chksum(uint16_t sum, const uint8_t *data, uint16_t len);

uint16_t rl_asl_ip_htons(uint16_t val);

uint32_t rl_asl_ip_ntohl(uint32_t val);

uint64_t rl_asl_ip_htonl64(uint64_t val);

uint64_t rl_asl_ip_ntohl64(uint64_t val);

uint16_t rl_asl_ip_chksum(void);

uint16_t rl_asl_data_chksum(void);

uint16_t rl_asl_bc_schedule_chksum(void);

void print_ip_header();

void print_data_header();

void print_bc_schedule_header();

// print raw buffer data
void print_raw_buffer(const uint8_t *buffer, uint16_t len);

bool rl_asl_update_ttl(void);

#endif /* RL_ASL_UTILS_H_ */