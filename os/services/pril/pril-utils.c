#include "pril-utils.h"
#include "net/mac/tsch/tsch.h"
#include "orchestra.h"

/*---------------------------------------------------------------------------*/
int pril_compute_cells_from_seconds(uint32_t seconds)
{
    if (seconds == 0)
        return 0;
    // timeslot length in microseconds
    uint32_t slot_us = tsch_timing[tsch_ts_timeslot_length];
    // slotframe length (number of timeslots per slotframe) - obtain from your schedule
    uint16_t slotframe_len = ORCHESTRA_UNICAST_PERIOD;
    // slotframe duration in microseconds
    uint64_t slotframe_us = (uint64_t)slot_us * slotframe_len;
    if (slotframe_us == 0)
        return 0;
    uint64_t seconds_us = (uint64_t)seconds * 1000000ULL;
    // number of slotframes in seconds
    int cells = (int)(seconds_us / slotframe_us);
    if (cells < 1)
        cells = 1; // minimum of 1 cell
    return cells;
}