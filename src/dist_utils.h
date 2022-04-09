#ifndef __DIST_UTILS_H__
#define __DIST_UTILS_H__
#include "contiki.h"

#define ENVIRON_FACTOR 22.0 // Free space = 2 (after multiply by 10)
#define MEASURED_POWER -71
#define ERROR_MARGIN 0.5 // Error margin of 0.5

#define DISTANCE_THRESHOLD 1
/* 
    Prints decimals up to 3 s.f 
*/
void 
print_float(float val) {
    printf("%d.", (int)val);
    printf("%d", (int)(val*10)%10);
    printf("%d", (int)(val*100)%10);
    printf("%d", (int)(val*1000)%10);
}



/* 
    Returns whether the transmitting node is within 3m away by RSSI estimation. 
*/
bool
is_distance_within_3m(signed short rssi) {
    // Estimate distance
    int numerator = MEASURED_POWER - rssi;
    float exp = (float) numerator / ENVIRON_FACTOR;
    float dist = powf(10, exp);
    printf("Estimated distance: ");
    print_float(dist);
    printf("\n");

    // Check if distance is within error margin
    return dist + ERROR_MARGIN < DISTANCE_THRESHOLD || dist - ERROR_MARGIN < DISTANCE_THRESHOLD;
}

#endif