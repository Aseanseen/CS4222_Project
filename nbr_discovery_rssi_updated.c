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
#include <math.h>
#ifdef TMOTE_SKYS
#include "powertrace.h"
#endif
/*---------------------------------------------------------------------------*/
// sender timer
static struct rtimer rt;
static struct pt pt;
/*---------------------------------------------------------------------------*/
static data_packet_struct received_packet;
static data_packet_struct data_packet;
unsigned long curr_timestamp;
/*---------------------------------------------------------------------------*/
// Limit transmission time to just 10 seconds.

// Quantity is varied to choose the minimal power consumption.
#define N 10
#define TOTAL_SLOTS_LEN N * N
#define SEND_ARR_LEN 2 * N - 1
/*---------------------------------------------------------------------------*/
#define LATENCY_BOUND_S 3
// Factor is scaled by 1000. Inverse of the desired slot period, 
// which is 10s / total number of slots give by n^2
static long BEACON_INTERVAL_FREQ_SCALED = TOTAL_SLOTS_LEN  * 1000 / LATENCY_BOUND_S;
#define WAKE_TIME RTIMER_SECOND * 1000 / BEACON_INTERVAL_FREQ_SCALED
#define SLEEP_SLOT RTIMER_SECOND * 1000 / BEACON_INTERVAL_FREQ_SCALED
/*---------------------------------------------------------------------------*/
static int send_arr[SEND_ARR_LEN];
static int send_index = 0;
static int curr_pos = 0;
unsigned long last_receive, receive_delay, max_receive_delay;
/*---------------------------------------------------------------------------*/
// Formula for measuring distance using RSSI = 10^((MEASURED_POWER - RSSI) / ENVIRON_FACTOR)
#define ENVIRON_FACTOR 22.0 // Free space = 2 (after multiply by 10)
#define MEASURED_POWER -71
#define ERROR_MARGIN 0.5 // Error margin of 0.5
/*---------------------------------------------------------------------------*/
PROCESS(cc2650_nbr_discovery_process, "cc2650 neighbour discovery process");
AUTOSTART_PROCESSES(&cc2650_nbr_discovery_process);
/*---------------------------------------------------------------------------*/

/* 
    Prints decimals up to 3 s.f 
    */
static void 
print_float(float val) {
    printf("%d.", (int)val);
    printf("%d", (int)(val*10)%10);
    printf("%d", (int)(val*100)%10);
    printf("%d", (int)(val*1000)%10);
}

/* 
    Returns whether the transmitting node is within 3m away by RSSI estimation. 
*/
static bool
is_distance_within_3m(signed short rssi) {
    // Estimate distance
    int numerator = MEASURED_POWER - rssi;
    float exp = (float) numerator / ENVIRON_FACTOR;
    float dist = powf(10, exp);
    printf("Estimated distance: ");
    print_float(dist);

    // Check if distance is within error margin
    return dist + ERROR_MARGIN < 3 || dist - ERROR_MARGIN < 3;
}
/*---------------------------------------------------------------------------*/
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
    leds_on(LEDS_GREEN);
    memcpy(&received_packet, packetbuf_dataptr(), sizeof(data_packet_struct));

    // printf("Send seq# %lu  @ %8lu  %3lu.%03lu\n", data_packet.seq, curr_timestamp, curr_timestamp / CLOCK_SECOND, ((curr_timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND);

    // printf("Received packet from node %lu with sequence number %lu and timestamp %3lu.%03lu\n", received_packet.src_id, received_packet.seq, received_packet.timestamp / CLOCK_SECOND, ((received_packet.timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND);
    
    // // print RSSI
    // printf("RSSI: %d\n", (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI));
    signed short rssi = (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);

    // CHECK IF CLOSE PROXIMITY
    bool is_close = is_distance_within_3m(rssi);
    if (is_close)  printf("==> Device in close proximity!!\n");
    else  printf("XXX Not within +-3m\n");

    /* Code to get the max receive delay */
    if (last_receive == 0)
    {
        last_receive = received_packet.timestamp;
    }
    else
    {
        receive_delay = received_packet.timestamp - last_receive;
        last_receive = received_packet.timestamp;
        max_receive_delay = receive_delay > max_receive_delay ? receive_delay : max_receive_delay;
    }
    // printf("Max receive delay: %3lu.%03lu\n", max_receive_delay / CLOCK_SECOND, ((max_receive_delay % CLOCK_SECOND) * 1000) / CLOCK_SECOND);
    leds_off(LEDS_GREEN);
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;
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

char sender_scheduler(struct rtimer *t, void *ptr)
{
    static uint16_t i = 0;
    static int NumSleep = 0;
    PT_BEGIN(&pt);

    curr_timestamp = clock_time();
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
        if (send_arr[send_index] == curr_pos)
        {
            /* Awake mode */
            // printf("SENDING\n");
            // radio on
            NETSTACK_RADIO.on();

            // SET RADIO TRANSMIT POWER
            //NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, TX_POWER);

            for (i = 0; i < NUM_SEND; i++)
            { // #define NUM_SEND 2 (in defs_and_types.h)
                leds_on(LEDS_RED);

                data_packet.seq++;
                curr_timestamp = clock_time();
                data_packet.timestamp = curr_timestamp;

                // printf("Send seq# %lu  @ %8lu ticks   %3lu.%03lu\n", data_packet.seq, curr_timestamp, curr_timestamp / CLOCK_SECOND, ((curr_timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND);

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
            // printf(" Sleep for %d slots \n", NumSleep);

            // NumSleep should be a constant or static int
            for (i = 0; i < NumSleep; i++)
            {
                rtimer_set(t, RTIMER_TIME(t) + SLEEP_SLOT, 1, (rtimer_callback_t)sender_scheduler, ptr);
                PT_YIELD(&pt);
                // Increment curr pos for every sleep slot
                curr_pos = (curr_pos == TOTAL_SLOTS_LEN - 1) ? 0 : curr_pos + 1;
            }
            leds_off(LEDS_BLUE);
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
    int i;
    // printf("CC2650 neighbour discovery\n");
    // printf("Node %d will be sending packet of size %d Bytes\n", node_id, (int)sizeof(data_packet_struct));

    // radio off
    NETSTACK_RADIO.off();

    // initialize data packet
    data_packet.src_id = node_id;
    data_packet.seq = 0;

    // Choose slots to be active
    // We choose row_num and col_num randomly at the beginning of runtime
    // And generate the slots in arr [0, n*n-1] to be active.
    int row_num = random_rand() % N;
    int col_num = random_rand() % N;
    // int row_num = 0;
    // int col_num = 0;
    set_active_slots(send_arr, row_num, col_num);

    // // prints the slots whereby the device will be active.
    // printf("Send Arr: ");
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