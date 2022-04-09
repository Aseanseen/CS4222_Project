#ifndef __DIST_UTILS_H__
#define __DIST_UTILS_H__
#include "contiki.h"
#include "constants.h"

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