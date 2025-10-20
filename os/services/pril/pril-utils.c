#include "pril-utils.h"
#include "net/mac/tsch/tsch.h"
#include "orchestra.h"

/*---------------------------------------------------------------------------*/
int pril_compute_cells_from_seconds(int seconds)
{
    if (seconds <= 0)
        return 0;

    // Timeslot length in microseconds
    uint16_t slot_us = tsch_timing_us[tsch_ts_timeslot_length];

    // Convert seconds → microseconds
    uint64_t seconds_us = (uint64_t)seconds * 1000000ULL;

    // Compute how many slotframes fit in the given time
    uint64_t cells = seconds_us / (slot_us * (uint64_t)ORCHESTRA_UNICAST_PERIOD);
    return (int)cells;
}