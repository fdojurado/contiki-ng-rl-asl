#include "rl-asl.h"
#include <stdlib.h>
#include "rl-asl-conf.h"

/* log */
#include "sys/log.h"
#define LOG_MODULE "rl-asl"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL

void rl_asl_check_skip_rx(const struct tsch_link *link, bool *skip_rx)
{
    uint16_t sf_handle = link->slotframe_handle;
    if (sf_handle != RL_ASL_UNICAST_SLOTFRAME_HANDLE)
    {
        *skip_rx = false;
        return;
    }

    if (tsch_is_associated == 0)
    {
        *skip_rx = false;
        return;
    }

    // /Every ten consecutive slots, skip the RX slot if the link has RX option
    // if ((link->link_options & LINK_OPTION_RX) && ((counter % 100) == 0))
    // {
    //     *skip_rx = false;
    // }
    // else
    // {
    //     *skip_rx = true;
    // }
    // counter++;
}
