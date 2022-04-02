#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

#define TOKEN_1_ADDR 0x5403
#define TOKEN_2_ADDR 0x2082

/* Quantity is varied to choose the minimal power consumption. */
#define N 3
#define TOTAL_SLOTS_LEN N * N
#define SEND_ARR_LEN 2 * N - 1

#define LATENCY_BOUND_S 1

#define RSSI_THRESHOLD_3M -45

#define ABSENT_TO_DETECT 15
#define DETECT_TO_ABSENT 30

#endif
