#ifndef RL_ASL_DECISION_BUFFER_H
#define RL_ASL_DECISION_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define RL_ASL_DECISION_BUFFER_SIZE 16

typedef struct
{
    uint32_t asn_low32; // low 32 bits of the ASN when decision was made
    int state;
    int action;
    bool valid;
} rl_asl_decision_t;

extern rl_asl_decision_t rl_asl_decision_buffer[RL_ASL_DECISION_BUFFER_SIZE];

void rl_asl_decision_buffer_add(uint32_t asn_low32, int state, int action);
bool rl_asl_decision_buffer_consume(uint32_t asn_low32, int *state, int *action);
void rl_asl_decision_buffer_reset(void);

#endif /* RL_ASL_DECISION_BUFFER_H */