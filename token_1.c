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
static long BEACON_INTERVAL_FREQ_SCALED = TOTAL_SLOTS_LEN  * 1000 / LATENCY_BOUND_S;
#define WAKE_TIME RTIMER_SECOND * 1000 / BEACON_INTERVAL_FREQ_SCALED
#define SLEEP_SLOT RTIMER_SECOND * 1000 / BEACON_INTERVAL_FREQ_SCALED
/*---------------------------------------------------------------------------*/
static int send_arr[SEND_ARR_LEN];
static int send_index = 0;
static int curr_pos = 0;
static int state_flag = 0;
/*---------------------------------------------------------------------------*/
signed short rssi_sum;
static int rssi_count;
static int consec = 0;
static int detect_timestamp_s;
static int absent_timestamp_s;
/*---------------------------------------------------------------------------*/
static void count_consec(int, int);
/*---------------------------------------------------------------------------*/
PROCESS(cc2650_nbr_discovery_process, "cc2650 neighbour discovery process");
AUTOSTART_PROCESSES(&cc2650_nbr_discovery_process);
/*---------------------------------------------------------------------------*/
static void count_consec(int is_detect_cycle, int timestamp_s)
{
    printf("COUNTING %i STATE %i DETECT %i\n", consec, state_flag, is_detect_cycle);
	/* Detect mode */
	if(state_flag)
	{
		if (!is_detect_cycle)
		{
			// Count the number of consecutives
			consec += 1;
			// Save timestamp if first
			if (consec == 1)
			{
				absent_timestamp_s = timestamp_s;
			}
			else if (consec == DETECT_TO_ABSENT)
			{
                printf("|----- Changing from detect to absent -----|\n");
				// Need to change state
				consec = 0;
				state_flag = 0;
				printf("%i ABSENT %i\n", absent_timestamp_s, TOKEN_2_ADDR);
			}
		}
		else
		{
			consec = 0;
		}
	}
	/* Absent mode */
	else
	{
		if (is_detect_cycle)
		{
			// Count the number of consecutives
			consec += 1;
			// Save timestamp if first
			if (consec == 1)
			{
				detect_timestamp_s = timestamp_s;
			}
			else if (consec == ABSENT_TO_DETECT)
			{
                printf("|----- Changing from absent to detect -----|\n");
				// Need to change state
				consec = 0;
				state_flag = 1;
				printf("%i DETECT %i\n", detect_timestamp_s, TOKEN_2_ADDR);
			}
		}
		else
		{
			consec = 0;
		}
	}
}

static void recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
	/* Choice of message or data packet*/
	// Message
	// char message[50];
	// strcpy(message,(char *)packetbuf_dataptr());
	// message[packetbuf_datalen()]='\0';
	// curr_timestamp = clock_time(); 
	// printf("Timestamp: %3lu.%03lu Received packet from node %lu\n", curr_timestamp / CLOCK_SECOND, ((curr_timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND, ); // Need to add in some id
	// printf("RSSI: %d\n", (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI));

	// Data packet struct
	printf("RECEIVING\n");
	memcpy(&received_packet, packetbuf_dataptr(), sizeof(data_packet_struct));
	curr_timestamp = clock_time();
	
	// rssi_arr[rssi_index] = (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);
	rssi_sum += (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);
	rssi_count += 1;

	printf(
		"Timestamp: %3lu.%03lu Received packet from node id: %lu RSSI: %d\n", 
		curr_timestamp / CLOCK_SECOND, 
		((curr_timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND, 
		received_packet.src_id, 
		(signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI)
		);
}

static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;
/*---------------------------------------------------------------------------*/
void set_active_slots(int *buf, int row_num, int col_num)
{
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

int is_detect_cycle()
{
	int ave_rssi;
	// did not receive any packet in the cycle
    printf("RSSI COUNT %i\n", rssi_count);
	if (rssi_count == 0)
	{
		// did not receive
		return 0;
	}
	else
	{
		ave_rssi = rssi_sum / rssi_count;
        printf("Ave RSSI %i\n", ave_rssi);
		// Ave RSSI values indicates detect < 3m
		return ave_rssi > RSSI_THRESHOLD_3M;
	}
}

char sender_scheduler(struct rtimer *t, void *ptr)
{
    static uint16_t i = 0;
    static int NumSleep = 0;
    int prev_timestamp_s = 0;

    linkaddr_t addr;
    PT_BEGIN(&pt);

    curr_timestamp = clock_time();
    // int curr_timestamp_s = curr_timestamp / CLOCK_SECOND;
    prev_timestamp_s = curr_timestamp / CLOCK_SECOND - LATENCY_BOUND_S;

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
        printf("|----- send_slot = %d, curr_slot = %d -----|\n", send_arr[send_index], curr_pos);
        /* Awake mode */
        if (send_arr[send_index] == curr_pos)
        {
            printf("SENDING\n");

            for (i = 0; i < NUM_SEND; i++)
            { // #define NUM_SEND 2 (in defs_and_types.h)
                packetbuf_copyfrom(&data_packet, (int)sizeof(data_packet_struct));

				// Given a node id, automatically determine the address via extracting the higher and lower bytes.
				addr.u8[0] = TOKEN_2_ADDR >> 8;
				addr.u8[1] = TOKEN_2_ADDR & 0xFF;

				if(!linkaddr_cmp(&addr, &linkaddr_node_addr))
				{
					unicast_send(&uc, &addr);
				}
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
            	count_consec(is_detect_cycle(), prev_timestamp_s);
        		rssi_sum = 0;
				rssi_count = 0;
            }
        }
        /* Sleep mode */
        else
        {
            printf("SLEEPING\n");

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
            printf("Sleep for %d slots \n", NumSleep);

            // NumSleep should be a constant or static int
            for (i = 0; i < NumSleep; i++)
            {
                rtimer_set(t, RTIMER_TIME(t) + SLEEP_SLOT, 1, (rtimer_callback_t)sender_scheduler, ptr);
                PT_YIELD(&pt);
                // Increment curr pos for every sleep slot
                curr_pos = (curr_pos == TOTAL_SLOTS_LEN - 1) ? 0 : curr_pos + 1;
                if (curr_pos == 0)
	            {
	            	count_consec(is_detect_cycle(), prev_timestamp_s);
	            	rssi_sum = 0;
	            	rssi_count = 0;
	            }
            }
        }
    }
    PT_END(&pt);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(cc2650_nbr_discovery_process, ev, data)
{
    PROCESS_EXITHANDLER(unicast_close(&uc);)

    PROCESS_BEGIN();

    random_init(54222);

	#ifdef TMOTE_SKY
    powertrace_start(CLOCK_SECOND * 5);
	#endif

    unicast_open(&uc, 146, &unicast_callbacks);

	// for serial port
	#if !WITH_UIP && !WITH_UIP6
    uart1_set_input(serial_line_input_byte);
    serial_line_init();
	#endif

    printf("CC2650 neighbour discovery\n");
    printf("Node %d will be sending packet of size %d Bytes\n", node_id, (int)sizeof(data_packet_struct));

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

    // Start sender in one millisecond.
    rtimer_set(&rt, RTIMER_NOW() + (RTIMER_SECOND / 1000), 1, (rtimer_callback_t)sender_scheduler, NULL);

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/