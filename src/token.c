#include "contiki.h"
#include "dev/leds.h"
#include <stdio.h>
#include "core/net/rime/rime.h"
#include "dev/serial-line.h"
#include "dev/uart1.h"
#include "node-id.h"
#include "net/netstack.h"
#include "random.h"

#include "constants.h"
#include "hashArray.h"
#include "dist_utils.h"
#include "cycle_logic.h"
#include "quorum_utils.h"

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

/*---------------------------------------------------------------------------*/
static int send_arr[SEND_ARR_LEN];
static int send_index = 0;
static int curr_pos = 0;
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
PROCESS(cc2650_nbr_discovery_process, "cc2650 neighbour discovery process");
AUTOSTART_PROCESSES(&cc2650_nbr_discovery_process);

MEMB(tmp, struct TokenData, 5);
/*---------------------------------------------------------------------------*/

struct TokenData* dummyToken;
/*---------------------------------------------------------------------------*/


// Insert token data at key


/*---------------------------------------------------------------------------*/


/*
Collects the RSSI when packet is received
*/
static void 
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
	leds_on(LEDS_GREEN);
    // Data packet struct
	memcpy(&received_packet, packetbuf_dataptr(), sizeof(data_packet_struct));
	curr_timestamp = clock_time();

    // Find the token in hash table
    dummyToken = hashArray_search(received_packet.src_id);

    // First entry of token.
    // When a new node has been discovered,
    // Store it as a key in the hash array.
	if (dummyToken == NULL)
    {
        dummyToken = hashArray_insert(tmp, received_packet.src_id,0,0,0,true);
    }

    // Accumulate the rssi value.
    dummyToken->rssi_sum += (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);
    dummyToken->rssi_count += 1;
    dummyToken->is_curr_detect |= (1 << NOCONTACT_DISCONNECT);
    leds_off(LEDS_GREEN);
	/*
    printf(
		"Timestamp: %3lu.%03lu Received packet from node id: %lu RSSI: %d\n", 
		curr_timestamp / CLOCK_SECOND, 
		((curr_timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND, 
		received_packet.src_id, 
		(signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI)
		);
        */
    
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;

void 
print_time_s() {
    curr_timestamp = clock_time();
    printf(
		"Timestamp: %3lu.%03lu\n", 
		curr_timestamp / CLOCK_SECOND, 
		((curr_timestamp % CLOCK_SECOND)*1000) / CLOCK_SECOND
    );
}

/*
    Determines the packets to be sent based on the sleep and awake modes.
    The sender scheduler interprets the generate sequence once every N * N slots, all t < LATENCY_BOUND_S.
*/
char sender_scheduler(struct rtimer *t, void *ptr)
{
    static uint16_t i = 0;
    static int NumSleep = 0;
    PT_BEGIN(&pt);

    // // printf("Start clock %lu ticks, timestamp %3lu.%03lu\n", curr_timestamp, curr_timestamp / CLOCK_SECOND, ((curr_timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND);

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
                leds_on(LEDS_RED);

                packetbuf_copyfrom(&data_packet, (int)sizeof(data_packet_struct));
				broadcast_send(&broadcast);
                leds_off(LEDS_RED);

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
                process_cycle(dummyToken);
            }
        }
        /* Sleep mode */
        else
        {
            // printf("SLEEPING\n");
            leds_on(LEDS_BLUE);
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
                    process_cycle(dummyToken);
                    
	            }
                leds_off(LEDS_BLUE);
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

    // printf("CC2650 neighbour discovery\n");
    // printf("Node %d will be sending packet of size %d Bytes\n", node_id, (int)sizeof(data_packet_struct));

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
    
    // // prints the slots whereby the device will be active.
    // printf("Send Arr: ");
    int i;
    for (i = 0; i < SEND_ARR_LEN; i++)
    {
        // printf("%d ", send_arr[i]);
    }
    // printf("\n");

    // // prints parameter data.
    long time = 1000 * 1000 / BEACON_INTERVAL_FREQ_SCALED;
    int s = time/1000;
    int ms1 = (time % 1000)*10/1000;
    int ms2 = ((time % 1000)*100/1000)%10;
    int ms3 = ((time % 1000)*1000/1000)%10;
    // printf("\
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