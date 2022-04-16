#include <stdint.h>
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
#include "math.h"
/*---------------------------------------------------------------------------*/
#define COOJA_LIGHT_VAL                     12000
/*---------------------------------------------------------------------------*/
#if TMOTE_SKY
#include "powertrace.h"
#include "dev/light-sensor.h"
#define CC26XX_SENSOR_READING_ERROR        0x80000000
const struct sensors_sensor *sensor = &light_sensor;
#else
#include "board-peripherals.h"
const struct sensors_sensor *sensor = &opt_3001_sensor;

#endif
/*---------------------------------------------------------------------------*/
#define ABSENT_TO_DETECT_S                  15
#define DETECT_TO_ABSENT_S                  30
#define UNIT_CYCLE_TIME_S                   1

/* Quantity is varied to choose the minimal power consumption. */
#define N_VAL                               8
#define TOTAL_SLOTS_LEN                     N_VAL * N_VAL
#define SEND_ARR_LEN                        2 * N_VAL - 1
#define NUM_SEND                            2

static int LATENCY_BOUND_S;
static float BEACON_INTERVAL_FREQ_SCALED;
static float BEACON_INTERVAL_PERIOD_SCALED;
static float WAKE_TIME;
static float SLEEP_SLOT;

#define ENVIRON_FACTOR_IN                   22.0 // Free space = 2 (after multiply by 10)
#define MEASURED_POWER_IN                   -78
#define ENVIRON_FACTOR_OUT                  22.0 // Free space = 2 (after multiply by 10)
#define MEASURED_POWER_OUT                  -70
#define ERROR_MARGIN                        0.5 // Error margin of 0.5
#define TX_POWER                            -15 // Default: 5, Min: -18
#define DISTANCE_THRESHOLD                  3

#define LUX_THRESHOLD                       1200 // LUX threshold to determine outdoor/indoor 
#define MIN_WARM_UP_TIME_S                  (float)0.2 // Min number of seconds needed for light sensor to warm up
/*---------------------------------------------------------------------------*/
static struct rtimer rt;
static struct pt pt;
/*---------------------------------------------------------------------------*/
static data_packet_struct received_packet;
static data_packet_struct data_packet; 
unsigned long curr_timestamp;
/*---------------------------------------------------------------------------*/
static int send_arr[SEND_ARR_LEN];
static int send_index = 0;
static int curr_pos = 0;
static int environment = 0; // 0 - indoor, 1 - outdoor
/*---------------------------------------------------------------------------*/
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
int min_light_t; // Min number of slots to warm up light sensor.
int light_flag = 1; // Signal when light sensor is warmed up.

/*
    Prints float variables
*/
void 
print_float(float val) {
    printf("%d.", (int)val);
    printf("%d", (int)(val*10)%10);
    printf("%d", (int)(val*100)%10);
    printf("%d", (int)(val*1000)%10);
}

/*
    Returns whether the receiver node is indoor or outdoor
    0 - indoor, 1 - outdoor
    default to indoor
*/
int
is_outdoor(){
    int value;
    int rtr_val = 0;

    value = (*sensor).value(0);
    // Overwrite with pseudo value if COOJA
    #if TMOTE_SKY
    value = COOJA_LIGHT_VAL;
    #else 
    if(value != CC26XX_SENSOR_READING_ERROR) {
        // Check if LUX over threshold
        if ((value / 100) >= LUX_THRESHOLD) rtr_val = 1;
    }
    #endif
    // printf("OPT: Light=%d.%02d lux\n", value / 100, value % 100);
    
    return rtr_val;
}

/* 
    Returns whether the transmitting node is within 3m away by RSSI estimation. 
*/
int
is_distance_within_3m(signed short rssi) {
    // Estimate distance
    //printf("RSSI: %d\n", rssi);

    float dist = 0;
    if (environment == 0) {
        // Indoor environment
        int numerator = MEASURED_POWER_IN - rssi;
        float exp = (float) numerator / ENVIRON_FACTOR_IN;
        dist = powf(10, exp);
    } else {
        // Outdoor environment
        int numerator = MEASURED_POWER_OUT - rssi;
        float exp = (float) numerator / ENVIRON_FACTOR_OUT;
        dist = powf(10, exp);
    }
    
    //printf("Estimated distance: ");
    //print_float(dist);
    //printf("\n");

    // Check if distance is within error margin
    return (dist - ERROR_MARGIN) < DISTANCE_THRESHOLD ;
}

/*
    Helper function.
    Determines if the node is within 3m based on the ave RSSI of packets received in a cycle.
*/
int 
is_detect_cycle(struct TokenData* dummyToken)
{
    int ave_rssi;
    // did not receive any packet in the cycle
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
static void 
count_consec(int curr_timestamp_s, int start_timestamp_s)   
{
    
    // Check light setting
    environment = is_outdoor();
    //printf("\nIS OUTDOOR: %i\n", environment);
    
    int i;
    int is_detect;
    int consec;
    int state_flag;
    int tokenId;
    struct TokenData* _dummyToken;
    // Go through the hash table to find all tokens 
    for(i = 0; i<ARR_MAX_LEN; i++)
    {
        _dummyToken = tokenDataList.tk[i];   
        // map_view(&tokenDataList);
        if(_dummyToken->key != -1)
        {
            state_flag = _dummyToken->state_flag;
            consec = _dummyToken->consec;
            tokenId = _dummyToken->key;
            is_detect = is_detect_cycle(_dummyToken);
            // printf("NODE %d ", _dummyToken->key);
            // printf("CURR TIME %i START TIME %i COUNTING %i STATE %i DETECT %i\n", curr_timestamp_s, !state_flag ? _dummyToken->detect_to_absent_ts : _dummyToken->absent_to_detect_ts, consec, state_flag, is_detect);

        	/* Detect mode */
        	if(state_flag && !is_detect)
        	{
        		// Count the number of consecutives
        		consec += 1;
        		// Save timestamp if first
        		if (consec == 1)
        		{
        			_dummyToken->detect_to_absent_ts = start_timestamp_s;
        		}
        		else if (consec == DETECT_TO_ABSENT_S / UNIT_CYCLE_TIME_S)
        		{
        			// Need to change state
        			consec = 0;
        			state_flag = 0;
        			printf("%i ABSENT %i\n", _dummyToken->detect_to_absent_ts, tokenId);
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
        			_dummyToken->absent_to_detect_ts = start_timestamp_s;
        		}
        		else if (consec == ABSENT_TO_DETECT_S / UNIT_CYCLE_TIME_S)
        		{
        			// Need to change state
        			consec = 0;
        			state_flag = 1;
        			printf("%i DETECT %i\n", _dummyToken->absent_to_detect_ts, tokenId);
        		}
        	}
            else
            {
                consec = 0;
            }
            //printf(">>> CURR TIME %i START TIME %i COUNTING %i STATE %i DETECT %i\n", curr_timestamp_s, start_timestamp_s, consec, state_flag, is_detect);

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
void 
process_cycle()
{
    int curr_timestamp_s;

    curr_timestamp_s = clock_time()/CLOCK_SECOND;
    count_consec(curr_timestamp_s, cycle_start_timestamp_s);
    cycle_start_timestamp_s = curr_timestamp_s;
}

/*
Collects the RSSI when packet is received
*/
static void 
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
	// Data packet struct
	memcpy(&received_packet, packetbuf_dataptr(), sizeof(data_packet_struct));
	curr_timestamp = clock_time();

    // Find the token in hash table
    dummyToken = map_search(&tokenDataList, received_packet.src_id);

    // First entry of token
	if (dummyToken->key == -1)
    {
        map_insert(&tokenDataList, received_packet.src_id, 0, 0, 0, 0, 0, 0);
    } 
    dummyToken->rssi_sum += (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);
    dummyToken->rssi_count += 1;
    // printf("RSSI: %d\n", (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI));
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
void 
set_active_slots(int *buf, int row_num, int col_num)
{
    int temp = 0, insert_index = 0;
    int j;

    int rotation_num = N_VAL * row_num + col_num;
    for (j = 0; j < N_VAL; j++)
    {
        temp = col_num + N_VAL * j;
        if (temp < rotation_num)
        {
            buf[j] = temp;
        }
        else if (temp > rotation_num)
        {
            buf[j + N_VAL - 1] = temp;
        }
        else
        {
            insert_index = j;
        }
    }

    for (j = 0; j < N_VAL; j++)
    {
        temp = row_num * N_VAL + j;
        buf[insert_index + j] = temp;
    }
}

/*
    Handles movement along the quorum array
*/
void 
handle_next_pos(int *curr_pos) {
    // update the curr pos and the send index
    *curr_pos = (*curr_pos == TOTAL_SLOTS_LEN - 1) ? 0 : *curr_pos + 1;

    // printf("%d --- %d : %s\n", min_light_t, TOTAL_SLOTS_LEN - curr_pos, light_flag ? "yes1" : "no1");
    if (((TOTAL_SLOTS_LEN - *curr_pos) < min_light_t) && light_flag) {
        SENSORS_ACTIVATE(*sensor);
        // printf("active - 1!\n");
        light_flag = 0;
    }

    if (*curr_pos == 0)
    {
        process_cycle();
        light_flag = 1;
    }
}

/*
    Determines the packets to be sent based on the sleep and awake modes
*/
char 
sender_scheduler(struct rtimer *t, void *ptr)
{
    static uint16_t i = 0;
    static int NumSleep = 0;
    PT_BEGIN(&pt);
    
    light_flag = 1;
    while (1)
    {
        /* 
            Check if to send in the current slot.
            curr_pos refers to the time slot in an n*n slot window.
            send_index refers to the next time slot to send determined by the active slot buffer.
            If the time slot is to send,
            The curr_pos will correspond with the send index pointed at an element in the active slot buffer.
        */
        /* Awake mode */
        if (send_arr[send_index] == curr_pos)
        {
            // radio on
            NETSTACK_RADIO.on();

            // Set Radio transmission power
            # if TMOTE_SKY
            NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, 0);
            # else
            NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, TX_POWER);
            #endif
            int tx_val;
            NETSTACK_RADIO.get_value(RADIO_PARAM_TXPOWER, &tx_val);
            // printf("tx_val: %i\n", tx_val);
            for (i = 0; i < NUM_SEND; i++)
            { 
                packetbuf_copyfrom(&data_packet, (int)sizeof(data_packet_struct));
				broadcast_send(&broadcast);
                // printf("Time1: %ld\n", clock_time()/CLOCK_SECOND);
                if (i != (NUM_SEND - 1))
                {
                    rtimer_set(t, RTIMER_TIME(t) + WAKE_TIME, 1, (rtimer_callback_t)sender_scheduler, ptr);
                    PT_YIELD(&pt);
                }
            }
            
            handle_next_pos(&curr_pos);

            send_index = (send_index == SEND_ARR_LEN - 1) ? 0 : send_index + 1;        }
        /* Sleep mode */
        else
        {
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

            // NumSleep should be a constant or static int
            for (i = 0; i < NumSleep; i++)
            {
                // Warm up light sensor 1 slot before wake up
                // printf("Time2: %ld\n", clock_time()/CLOCK_SECOND);

                rtimer_set(t, RTIMER_TIME(t) + SLEEP_SLOT, 1, (rtimer_callback_t)sender_scheduler, ptr);
                PT_YIELD(&pt);

                handle_next_pos(&curr_pos);
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

    // radio off
    NETSTACK_RADIO.off();

    // initialize data packet
    data_packet.src_id = node_id;
    // Choose slots to be active
    // We choose row_num and col_num randomly at the beginning of runtime
    // And generate the slots in arr [0, n*n-1] to be active.
    int row_num = random_rand() % N_VAL;
    int col_num = random_rand() % N_VAL;

    set_active_slots(send_arr, row_num, col_num);
    
    LATENCY_BOUND_S = UNIT_CYCLE_TIME_S;
    BEACON_INTERVAL_PERIOD_SCALED = (float)LATENCY_BOUND_S / (TOTAL_SLOTS_LEN);
    BEACON_INTERVAL_FREQ_SCALED = 1 / BEACON_INTERVAL_FREQ_SCALED;
    WAKE_TIME = RTIMER_SECOND * BEACON_INTERVAL_PERIOD_SCALED;
    SLEEP_SLOT = RTIMER_SECOND * BEACON_INTERVAL_PERIOD_SCALED;
    min_light_t = (int)(MIN_WARM_UP_TIME_S / ((float)BEACON_INTERVAL_PERIOD_SCALED)) + 1;  // Min number of slots to warm up light sensor.

    // Prints parameter data.
    
    int n_val = N_VAL;
    int total_slots_len = TOTAL_SLOTS_LEN;
    int latency_bound_s = LATENCY_BOUND_S;
    
    printf("\
    Row num: %d\n\
    Col num: %d\n\
    N_VAL: %d\n\
    Total Slots Len: %d\n\
    Max Time s: %d\n\ 
    Min Light Time: %d\n",
    row_num, col_num, n_val, total_slots_len, latency_bound_s, min_light_t);
    
    printf("\nBeacon interval period: ");
    print_float(BEACON_INTERVAL_PERIOD_SCALED);

    printf("\nWake Time: ");
    print_float(WAKE_TIME);

    printf("\nSleep Slot: ");
    print_float(SLEEP_SLOT);
    

    // Initialise memory for the TokenData Hashtable.
    memb_init(&tmp);
    map_init(tmp, &tokenDataList);

    // Start sender in one millisecond.
    rtimer_set(&rt, RTIMER_NOW() + (RTIMER_SECOND / 1000), 1, (rtimer_callback_t)sender_scheduler, NULL);

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/