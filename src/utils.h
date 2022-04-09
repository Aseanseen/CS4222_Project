#include "contiki.h"

void 
print_time_s(unsigned long curr_timestamp) {
    printf(
		"Timestamp: %3lu.%03lu\n", 
		curr_timestamp / CLOCK_SECOND, 
		((curr_timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND
    );
}