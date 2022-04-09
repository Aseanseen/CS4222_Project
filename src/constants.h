#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

#define ABSENT_TO_DETECT_EVAL_CYCLE_TIME    15
#define DETECT_TO_ABSENT_EVAL_CYCLE_TIME    30
#define UNIT_CYCLE_TIME                     3
#define MIN_NUM_TRIALS                      ABSENT_TO_DETECT_EVAL_CYCLE_TIME / UNIT_CYCLE_TIME

/* Quantity is varied to choose the minimal power consumption. */

#define N 4
#define TOTAL_SLOTS_LEN N * N
#define SEND_ARR_LEN 2 * N - 1

#define LATENCY_BOUND_S UNIT_CYCLE_TIME
// Factor is scaled by 1000. Inverse of the desired slot period, 
// which is 10s / total number of slots give by n^2
#define BEACON_INTERVAL_FREQ_SCALED     ((long)TOTAL_SLOTS_LEN  * 1000 / LATENCY_BOUND_S)
#define WAKE_TIME                       RTIMER_SECOND * 1000 / BEACON_INTERVAL_FREQ_SCALED
#define SLEEP_SLOT                      RTIMER_SECOND * 1000 / BEACON_INTERVAL_FREQ_SCALED

#define RSSI_THRESHOLD_3M -45

#define ABSENT_TO_DETECT 15
#define DETECT_TO_ABSENT 30
#define NUM_SEND 2

typedef struct {
  unsigned long src_id;
  unsigned long timestamp;
  unsigned long seq;
} data_packet_struct;

#endif
