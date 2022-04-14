#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

#define ABSENT_TO_DETECT_S                  15
#define DETECT_TO_ABSENT_S                  30
#define UNIT_CYCLE_TIME_S                   1

/* Quantity is varied to choose the minimal power consumption. */
#define N 2
#define TOTAL_SLOTS_LEN N * N
#define SEND_ARR_LEN 2 * N - 1
#define NUM_SEND 2

#define LATENCY_BOUND_S UNIT_CYCLE_TIME_S
#define BEACON_INTERVAL_FREQ_SCALED  (float)(TOTAL_SLOTS_LEN  * 1000 / LATENCY_BOUND_S)
#define WAKE_TIME RTIMER_SECOND * 1000 / BEACON_INTERVAL_FREQ_SCALED
#define SLEEP_SLOT RTIMER_SECOND * 1000 / BEACON_INTERVAL_FREQ_SCALED

#define RSSI_THRESHOLD_3M -45
#define ENVIRON_FACTOR 22.0 // Free space = 2 (after multiply by 10)
#define MEASURED_POWER -71
#define ERROR_MARGIN 0.5 // Error margin of 0.5
#define DISTANCE_THRESHOLD 1

#endif
