#ifndef __CYCLELOC_H__
#define __CYCLELOC_H__

#include "hashArray.h"
#include "constants.h"
#include "dist_utils.h"
#include <stdio.h>

static int begin_timestamp_s = 0;
static int cycle_start_timestamp_s = 0;



/*
Helper function.
Determines if the node is within 3m based on the ave RSSI of packets received in a cycle.
*/
bool is_detect_cycle(struct TokenData* dummyToken, bool (*lambda)(signed short))
{
    signed short ave_rssi;
    // did not receive any packet in the cycle
    // printf("RSSI COUNT %i\n", dummyToken->rssi_count);
    if (dummyToken->rssi_count == 0)
    {
        // did not receive
        return 0;
    }
    else
    {
        ave_rssi = dummyToken->rssi_sum / dummyToken->rssi_count;
        // printf("Ave RSSI %i\n", ave_rssi);
        // Ave RSSI values indicates detect < 3m

        // Lambda function takes in average rssi and determines whether the node is in proximity.
        return (*lambda)(ave_rssi);
    }
}

/*
    Runs at the start of every new cycle.
    Checks the state of all tokens in the hash table then counts the respective consecutive number.
    Absent 0: If consec increases to 15 then state changes to detect. // prints "Timestamp (in seconds) DETECT nodeID".
    Detect 1: If consec increases to 30 then state changes to absent. // prints "Timestamp (in seconds) ABSENT nodeID".
*/
/*
    KW:
    There are three connection states that we need to consider. 
    For a proper connection to be considered within a single unit cycle, two conditions must be fulfilled.
    1. There is at least one receiver signal received. Checked in the broadcast_recv callback.
    2. The distance condition is met (< ~3m)

    This is checked via bit-mapping. 

    For an eval-cycle to be considered as DETECTED, there must be consecutive DETECTS for each unit cycle. 
    This lemma relies on the deterministic bound of the A^2 protocol. If there is a guaranteed connection at every interval,
    then a pair of devices sufficiently proximate to one another must connect to one another at every interval.

*/
void count_consec(struct TokenData* dummyToken, int curr_timestamp_s, int start_timestamp_s)
{
    int i;
    
    STATE_CONN is_curr_detect;
    STATE_CONN is_prev_detect;

    int diff_time;
    
    int consec_same_is_detect;
    
    int tokenId;
    // Go through the hash table to find all tokens 
    for(i = 0; i<HASH_TABLE_SIZE; i++)
    {
        dummyToken = hashArray[i];
        
        if(dummyToken != NULL)
        {
            
            is_prev_detect = dummyToken->is_prev_detect;
            is_curr_detect = dummyToken->is_curr_detect;

            consec_same_is_detect = dummyToken->consec;
            tokenId = dummyToken->key;
            begin_timestamp_s = dummyToken->begin_timestamp_s;

            if (is_detect_cycle(dummyToken, is_distance_within_3m)) {
                // Write to the second bit to signify the second condition is fulfilled.
                is_curr_detect |= (1 << NOCONTACT_TOO_FAR);
            }

            printf("%d--", i);
            printf("CURR TIME %d START TIME %d BEGIN TIME %d COUNTING %d STATE %d DETECT %d\n", curr_timestamp_s, start_timestamp_s, begin_timestamp_s, consec_same_is_detect, is_prev_detect, is_curr_detect);

        	if (is_prev_detect == is_curr_detect)
        	{
                // Count the number of consecutives
                consec_same_is_detect += 1;
                // Save timestamp if first
                if (consec_same_is_detect == 1)
                {
                    dummyToken->begin_timestamp_s = start_timestamp_s;
                }

                // printf("|----- Changing from detect to absent -----|\n");
                // Need to change state
                else if (
                    (is_curr_detect == CONTACT) && 
                    (consec_same_is_detect > ABSENT_TO_DETECT_EVAL_CYCLE_TIME / UNIT_CYCLE_TIME)
                ) {
                    printf(
                        "[STATE COUNT > THRESHOLD] Was sufficiently PRESENT!\n"
                    );
        
                } 
                
                else if (
                    (is_curr_detect != CONTACT) &&
                    (consec_same_is_detect > DETECT_TO_ABSENT_EVAL_CYCLE_TIME / UNIT_CYCLE_TIME)
                ) {
                    printf(
                        "[STATE COUNT > THRESHOLD] Was sufficiently ABSENT!\n"
                    );         
                }
                        
                // printf("%i ABSENT %i\n", absent_timestamp_s, tokenId);
            
        	}

            else
            {
                // Check only if the node detected -> absent. 
                if (is_prev_detect == CONTACT) {

                    // Report how long they were connected/disconnected.
                    diff_time = curr_timestamp_s - begin_timestamp_s;
                    
                    printf(
                        "[STATE CHANGE] Was %s for ... %d s\n",
                        is_prev_detect == CONTACT ? "PRESENT" : "ABSENT",
                        diff_time
                    );
                }

                consec_same_is_detect = 0;
            }

            dummyToken->consec = consec_same_is_detect;
            
            dummyToken->is_prev_detect = is_curr_detect;
            // Always assume the node is not detected until a transmission says otherwise.
            dummyToken->is_curr_detect = NOCONTACT_DISCONNECT;
            
            dummyToken->rssi_sum = 0;
            dummyToken->rssi_count = 0;
        }
    }
}

/*
At the start of every cycle, processes the information of the last cycle.
Updates the timestamp of the start of a cycle.
*/
void process_cycle(struct TokenData* dummyToken)
{
    int curr_timestamp_s;

    curr_timestamp_s = clock_time()/CLOCK_SECOND;
    // printf("New cycle begins. Previous cycle lasted for: %i\n", curr_timestamp_s - cycle_start_timestamp_s);
    count_consec(dummyToken, curr_timestamp_s, cycle_start_timestamp_s);
    cycle_start_timestamp_s = curr_timestamp_s;
}

#endif