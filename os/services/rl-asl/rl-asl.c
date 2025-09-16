#include "rl-asl.h"
#include <stdlib.h> 

/* log */
#include "sys/log.h"
#define LOG_MODULE "rl-asl"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL

void rl_asl_check_skip_rx(const struct tsch_link *link, bool *skip_rx)
{
    // Lets flip a coin to decide whether to skip RX or not for demonstration purposes
    if (link->link_options & LINK_OPTION_RX)
    {
        *skip_rx = (rand() % 2) == 0; // 50% chance to skip RX
    }
    else
    {
        *skip_rx = false; // Do not skip RX if the link does not have RX option
    }
    LOG_DBG("Link handle: %u, RX option: %s, Skip RX: %s\n",
            link->handle,
            (link->link_options & LINK_OPTION_RX) ? "Yes" : "No",
            *skip_rx ? "Yes" : "No");
}
