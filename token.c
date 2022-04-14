#include "contiki.h"
#include "dev/leds.h"
#include <stdio.h>
#include "core/net/rime/rime.h"
#include "dev/serial-line.h"
#include "dev/uart1.h"
#include "node-id.h"
#include "defs_and_types.h"
#include "net/netstack.h"
#include "random.h"
#include "constants.h"
#ifdef TMOTE_SKY
#include "powertrace.h"
#endif

/*---------------------------------------------------------------------------*/
static struct rtimer rt;
static struct pt pt;
/*---------------------------------------------------------------------------*/
static data_packet_struct received_packet;
static data_packet_struct data_packet;
unsigned long curr_timestamp;
/*---------------------------------------------------------------------------*/
// Factor is scaled by 1000. Inverse of the desired slot period, 
// which is 10s / total number of slots give by n^2

/*---------------------------------------------------------------------------*/
static int send_arr[SEND_ARR_LEN];
static int send_index = 0;
static int curr_pos = 0;
/*---------------------------------------------------------------------------*/
static int detect_timestamp_s;
static int absent_timestamp_s;
unsigned long cycle_start_timestamp_s;
/*---------------------------------------------------------------------------*/
static void count_consec(int, int);
void set_active_slots(int *, int, int);
int is_detect_cycle(struct TokenData*);
void process_cycle();
/*---------------------------------------------------------------------------*/
PROCESS(cc2650_nbr_discovery_process, "cc2650 neighbour discovery process");
AUTOSTART_PROCESSES(&cc2650_nbr_discovery_process);
/*---------------------------------------------------------------------------*/
struct TokenData* dummyToken;
struct TokenDataList tokenDataList= {.max_size = ARR_MAX_LEN, .num_elem = 0};
/*---------------------------------------------------------------------------*/
MEMB(tmp, struct TokenData, ARR_MAX_LEN);
/*---------------------------------------------------------------------------*/

/*
    Mesures distance
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
int
is_distance_within_3m(signed short rssi) {
    // Estimate distance
    printf("RSSI: %d\n", rssi);
    int numerator = MEASURED_POWER - rssi;
    float exp = (float) numerator / ENVIRON_FACTOR;
    float dist = powf(10, exp);
    printf("Estimated distance: ");
    print_float(dist);
    printf("\n");

    // Check if distance is within error margin
    return (dist + ERROR_MARGIN) < DISTANCE_THRESHOLD ;
}

/*
Helper function.
Determines if the node is within 3m based on the ave RSSI of packets received in a cycle.
*/
int is_detect_cycle(struct TokenData* dummyToken)
{
    int ave_rssi;
    // did not receive any packet in the cycle
    // printf("RSSI COUNT %i\n", dummyToken->rssi_count);
    if (dummyToken->rssi_count == 0)
    {
        // did not receive
        return 0;
    }
    else
    {
        ave_rssi = dummyToken->rssi_sum/dummyToken->rssi_count;
        return is_distance_within_3m(ave_rssi);
    }
}

/*
Runs at the start of every new cycle.
Checks the state of all tokens in the hash table then counts the respective consecutive number.
Absent 0: If consec increases to 15 then state changes to detect. Prints "Timestamp (in seconds) DETECT nodeID".
Detect 1: If consec increases to 30 then state changes to absent. Prints "Timestamp (in seconds) ABSENT nodeID".
*/
static void count_consec(int curr_timestamp_s, int start_timestamp_s)   
{
    int i;
    int is_detect;
    int consec;
    int state_flag;
    int tokenId;
    struct TokenData* _dummyToken;
    // Go through the hash table to find all tokens 
    printf("\n----------------------------------\n");
    
    for(i = 0; i<ARR_MAX_LEN; i++)
    {
        
        _dummyToken = tokenDataList.tk[i];
        
        if(_dummyToken->key != -1)
        {
            map_view(&tokenDataList);
            state_flag = _dummyToken->state_flag;
            consec = _dummyToken->consec;
            tokenId = _dummyToken->key;
            is_detect = is_detect_cycle(_dummyToken);
            printf("NODE %d ", _dummyToken->key);
            printf("CURR TIME %i START TIME %i COUNTING %i STATE %i DETECT %i\n", curr_timestamp_s, !state_flag ? _dummyToken->detect_to_absent_ts : _dummyToken->absent_to_detect_ts, consec, state_flag, is_detect);

        	/* Detect mode */
        	if(state_flag && !is_detect)
        	{
        		// Count the number of consecutives
        		consec += 1;
        		// Save timestamp if first
        		if (consec == 1)
        		{
        			absent_timestamp_s = start_timestamp_s;
        		}
        		else if (consec == DETECT_TO_ABSENT_S / UNIT_CYCLE_TIME_S)
        		{
                    printf("|----- Changing from detect to absent -----|\n");
        			// Need to change state
        			consec = 0;
        			state_flag = 0;
        			printf("%i ABSENT %i\n", absent_timestamp_s, tokenId);
                    _dummyToken->detect_to_absent_ts = absent_timestamp_s;
                    printf("Node has been present for --- %d\n", _dummyToken->detect_to_absent_ts - _dummyToken->absent_to_detect_ts);
        		}
        	}
        	/* Absent mode */
        	else if (!state_flag && is_detect)
        	{
        		// Count the number of consecutives
        		consec += 1;
        		// Save timestamp if first
        		if (consec == 1)
        		{
        			detect_timestamp_s = start_timestamp_s;
        		}
        		else if (consec == ABSENT_TO_DETECT_S / UNIT_CYCLE_TIME_S)
        		{
                    printf("|----- Changing from absent to detect -----|\n");
        			// Need to change state
        			consec = 0;
        			state_flag = 1;
        			printf("%i DETECT %i\n", detect_timestamp_s, tokenId);
                    _dummyToken->absent_to_detect_ts = detect_timestamp_s;
        		}
        	}
            else
            {
                consec = 0;
            }
            // printf("NODE %d", _dummyToken->key);
            printf(">>> CURR TIME %i START TIME %i COUNTING %i STATE %i DETECT %i\n", curr_timestamp_s, start_timestamp_s, consec, state_flag, is_detect);

            if (!(consec || state_flag || is_detect)) {
                map_remove(&tokenDataList, _dummyToken);
            }

            _dummyToken->consec = consec;
            _dummyToken->state_flag = state_flag;
            _dummyToken->rssi_sum = 0;
            _dummyToken->rssi_count = 0;
        }
    }
}

/*
At the start of every cycle, processes the information of the last cycle.
Updates the timestamp of the start of a cycle.
*/
void process_cycle()
{
    int curr_timestamp_s;

    curr_timestamp_s = clock_time()/CLOCK_SECOND;
    // printf("New cycle begins. Previous cycle lasted for: %i\n", curr_timestamp_s - cycle_start_timestamp_s);
    count_consec(curr_timestamp_s, cycle_start_timestamp_s);
    cycle_start_timestamp_s = curr_timestamp_s;
}

/*
Collects the RSSI when packet is received
*/
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
	// Data packet struct
	// printf("RECEIVING\n");
	memcpy(&received_packet, packetbuf_dataptr(), sizeof(data_packet_struct));
	curr_timestamp = clock_time();

    // Find the token in hash table
    dummyToken = map_search(&tokenDataList, received_packet.src_id);

    // First entry of token
	if (dummyToken == NULL)
    {
        struct TokenData insertToken = {
            .key = received_packet.src_id,
            .rssi_sum = 0,
            .rssi_count = 0,
            .consec = 0,
            .state_flag = 0,
            .absent_to_detect_ts = 0,
            .detect_to_absent_ts = 0
        };

        map_insert(&tokenDataList, &insertToken);
    } 
    dummyToken->rssi_sum += (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);
    dummyToken->rssi_count += 1;

	// printf("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
    // map_view(&tokenDataList);
    // printf("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;

/*
Populates an array with time slots to remain active for.
Suppose an example array of n = 3:

0 1 2
3 4 5
6 7 8

And row number, r is 1 and col number, c is 1

Then, rotation number is the number which the row and col intersects.
It is determined by formula n * r + c = 3 * 1 + 1 = 4.

We first find all col-wise numbers excluding rot number 4, and populate the arr as such:
[1, x, x, x, 7].

We reserve 3 slots between the col num before and after rot num for the row-wise numbers 3, 4, 5.

The final array is [1, 3, 4, 5, 7]. It is always sorted and is helpful in the 
revised send scheduler algorithm to determine when to turn on and off the radio.
*/
void set_active_slots(int *buf, int row_num, int col_num)
{
    int temp = 0, insert_index = 0;
    int j;

    int rotation_num = N * row_num + col_num;
    for (j = 0; j < N; j++)
    {
        temp = col_num + N * j;
        if (temp < rotation_num)
        {
            buf[j] = temp;
        }
        else if (temp > rotation_num)
        {
            buf[j + N - 1] = temp;
        }
        else
        {
            insert_index = j;
        }
    }

    for (j = 0; j < N; j++)
    {
        temp = row_num * N + j;
        buf[insert_index + j] = temp;
    }
}

/*
Determines the packets to be sent based on the sleep and awake modes
*/
char sender_scheduler(struct rtimer *t, void *ptr)
{
    static uint16_t i = 0;
    static int NumSleep = 0;
    PT_BEGIN(&pt);

    // printf("Start clock %lu ticks, timestamp %3lu.%03lu\n", curr_timestamp, curr_timestamp / CLOCK_SECOND, ((curr_timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND);

    while (1)
    {
        /* 
            Check if to send in the current slot.
            curr_pos refers to the time slot in an n*n slot window.
            send_index refers to the next time slot to send determined by the active slot buffer.
            If the time slot is to send,
            The curr_pos will correspond with the send index pointed at an element in the active slot buffer.
        */
        // printf("|----- send_slot = %d, curr_slot = %d -----|\n", send_arr[send_index], curr_pos);
        /* Awake mode */
        if (send_arr[send_index] == curr_pos)
        {
            // printf("SENDING\n");

            // radio on
            NETSTACK_RADIO.on();

            for (i = 0; i < NUM_SEND; i++)
            { // #define NUM_SEND 2 (in defs_and_types.h)
                packetbuf_copyfrom(&data_packet, (int)sizeof(data_packet_struct));
				broadcast_send(&broadcast);

                if (i != (NUM_SEND - 1))
                {
                    rtimer_set(t, RTIMER_TIME(t) + WAKE_TIME, 1, (rtimer_callback_t)sender_scheduler, ptr);
                    PT_YIELD(&pt);
                }
            }
            // update the curr pos and the send index
            curr_pos = (curr_pos == TOTAL_SLOTS_LEN - 1) ? 0 : curr_pos + 1;
            send_index = (send_index == SEND_ARR_LEN - 1) ? 0 : send_index + 1;
            if (curr_pos == 0)
            {
                process_cycle();
            }
        }
        /* Sleep mode */
        else
        {
            // printf("SLEEPING\n");

            // radio off
            NETSTACK_RADIO.off();

            // SLEEP_SLOT cannot be too large as value will overflow,
            // to have a large sleep interval, sleep many times instead

            // Finds the number of slots that the device can sleep for
            // Looks at the difference between send arr and the curr pos
            if (send_arr[send_index] > curr_pos)
            {
                NumSleep = send_arr[send_index] - curr_pos;
            }
            else
            {
                NumSleep = TOTAL_SLOTS_LEN + send_arr[send_index] - curr_pos;
            }
            // printf("Sleep for %d slots \n", NumSleep);

            // NumSleep should be a constant or static int
            for (i = 0; i < NumSleep; i++)
            {
                rtimer_set(t, RTIMER_TIME(t) + SLEEP_SLOT, 1, (rtimer_callback_t)sender_scheduler, ptr);
                PT_YIELD(&pt);
                // Increment curr pos for every sleep slot
                curr_pos = (curr_pos == TOTAL_SLOTS_LEN - 1) ? 0 : curr_pos + 1;
                if (curr_pos == 0)
	            {
                    process_cycle();
	            }
            }
        }
    }
    PT_END(&pt);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(cc2650_nbr_discovery_process, ev, data)
{
    PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

    PROCESS_BEGIN();

    random_init(54222);

	#ifdef TMOTE_SKY
    powertrace_start(CLOCK_SECOND * 5);
	#endif

    broadcast_open(&broadcast, 129, &broadcast_call);

	// for serial port
	#if !WITH_UIP && !WITH_UIP6
    uart1_set_input(serial_line_input_byte);
    serial_line_init();
	#endif

    printf("CC2650 neighbour discovery\n");
    printf("Node %d will be sending packet of size %d Bytes\n", node_id, (int)sizeof(data_packet_struct));

    // radio off
    NETSTACK_RADIO.off();

    // initialize data packet
    data_packet.src_id = node_id;

    // Choose slots to be active
    // We choose row_num and col_num randomly at the beginning of runtime
    // And generate the slots in arr [0, n*n-1] to be active.
    int row_num = random_rand() % N;
    int col_num = random_rand() % N;

    set_active_slots(send_arr, row_num, col_num);

    // Prints the slots whereby the device will be active.
    printf("Send Arr: ");
    int i;
    for (i = 0; i < SEND_ARR_LEN; i++)
    {
        printf("%d ", send_arr[i]);
    }
    printf("\n");

    // Prints parameter data.
    long time = 1000 * 1000 / BEACON_INTERVAL_FREQ_SCALED;
    int s = time/1000;
    int ms1 = (time % 1000)*10/1000;
    int ms2 = ((time % 1000)*100/1000)%10;
    int ms3 = ((time % 1000)*1000/1000)%10;

    printf("\
        Row num: %d\n\
        Col num: %d\n\
        Wake time: %ld\n\
        Sleep slot: %ld\n\
        Beacon Interval Period: %d.%d%d%ds\n\
        N: %d\n\
        Total Slots Len: %d\n\
        Max Time s: %d\n\
    ", row_num, col_num, WAKE_TIME, SLEEP_SLOT, s, ms1, ms2, ms3, N, TOTAL_SLOTS_LEN, LATENCY_BOUND_S);


    map_init(tmp, &tokenDataList);
    
    // Start sender in one millisecond.
    rtimer_set(&rt, RTIMER_NOW() + (RTIMER_SECOND / 1000), 1, (rtimer_callback_t)sender_scheduler, NULL);

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/