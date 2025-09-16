#include "rl-asl.h"
#include <stdlib.h> 

/* log */
#include "sys/log.h"
#define LOG_MODULE "rl-asl"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL

static uint64_t counter = 0;

void rl_asl_check_skip_rx(const struct tsch_link *link, bool *skip_rx)
{
    // /Every ten consecutive slots, skip the RX slot if the link has RX option
    if ((link->link_options & LINK_OPTION_RX) && ((counter % 100) == 0))
    {
        *skip_rx = false;
    }
    else
    {
        *skip_rx = true;
    }
    counter++;
}
