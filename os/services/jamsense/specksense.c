/**
 * \file
 *          A Contiki application that detects multiple
 *          concurrent channel activity in the 2.4 GHz band.
 *          The application works on low-power sensor node platforms
 *          that feature the cc2420 radio from Texas Instruments.
 *
 * \author
 *          Venkatraman Iyer <iyer.venkat9@gmail.com>
 */

#include "contiki.h"
#include <math.h>
#include <stdio.h> /* For printf() */
#include <stdlib.h>
#include "net/netstack.h"
#include "net/packetbuf.h"

#include "lib/random.h"
#include "sys/pt.h"
#include "sys/timer.h"
#include "sys/ctimer.h"
#include "sys/rtimer.h"
#include "sys/log.h"

#include "dev/eeprom.h"
#include "dev/radio.h"
#include "dev/watchdog.h"
#include "dev/leds.h"
#include "dev/leds.h"
#include "dev/etc/rgb-led/rgb-led.h"
#include "dev/button-hal.h"
#include "dev/serial-line.h"
#include "specksense.h"
#include "kmeans.h"
#include "cfs/cfs.h"
#include "nrf52840_bitfields.h"
#include "nrf52840.h"


/*---------------------------------------------------------------------------*/

/*Jamming detection, could be placed in a makefile*/
#if J_D == 1
#define RSSI_SIZE 140
#define POWER_LEVELS 20
#else
#define RSSI_SIZE 120
#define POWER_LEVELS 16
#endif
#define MAX_DURATION 1000

/*---------------------------------------------------------------------------*/
//! Global variables

static int print_counter = 0;

static uint16_t n_clusters;

//! Global variables for RSSI scan
static int rssi_val, /*rssi_valB,*/ rle_ptr = -1, /*rle_ptrB = -1,*/
		   step_count = 1, cond, itr,
		   n_samples, max_samples, rssi_val_mod;

static unsigned rssi_levels[RSSI_SIZE];
static rtimer_clock_t sample_st, sample_end;
static struct record record;

static radio_value_t channel_c = 26;
static int packet_cnt = 0;

#if CHANNEL_METRIC == 2
static uint16_t cidx;
static int itr_j;
static int current_channel = RADIO_CHANNEL;
#endif

static void print_rssi_rle()
{
	int i;
	printf("RSSI");
	for (i = 0; i <= rle_ptr; i++)
		printf(" (%d,%d)", record.rssi_rle[i][0], record.rssi_rle[i][1]);
	printf("\nrle_ptr:%d,%d,%d\n", rle_ptr, max_samples, n_samples);
}

/*---------------------------------------------------------------------------*/
void rssi_sampler(int time_window_ms)
{
	// sample_st = RTIMER_NOW();
	max_samples = time_window_ms * 20;
	step_count = 1;
	rle_ptr = -1;
	record.sequence_num = 0;
	int globalCounter = 0;
	print_counter = 0;

	printf("\n START \n");

	// for (int i = 0; i < (RUN_LENGTH); i++)
	// {
	// 	// printf("Old values: [0]: %d  [1]: %d \n", record.rssi_rle[i][0],record.rssi_rle[i][1]);

	// 	record.rssi_rle[i][0] = 0;
	// 	record.rssi_rle[i][1] = 0;
	// }

	record.sequence_num++;
	rle_ptr = -1;
	record.rssi_rle[0][1] = 0;
	record.rssi_rle[0][0] = 0;
	n_samples = max_samples * 10;
	//   watchdog_stop();
	print_counter = 0;
	int times = 0;
	watchdog_periodic();

	while ((rle_ptr < RUN_LENGTH))
	{
		times++;
		/*Get RSSI value*/

		/*Start time*/
		if (NETSTACK_RADIO.get_value(RADIO_PARAM_RSSI, &rssi_val) != RADIO_RESULT_OK)
		{
			printf(" ff");
		}

		rssi_val -= 45; /* compensation offset */
		int16_t debug_rssi = rssi_val;

		/*If power level is <= 2 set it to power level 1*/
		if (debug_rssi <= -132)
		{
			debug_rssi = -139;
		}

		/*Power level most be higher than one */
		if (rssi_levels[-debug_rssi - 1] > 1)
		{
			n_samples = n_samples - 1;
			cond = 0x01 & ((record.rssi_rle[rle_ptr][0] != rssi_levels[-rssi_val - 1]) | (record.rssi_rle[rle_ptr][1] == 32767));

			/*Max_duration achieved, move to next value*/
			if (record.rssi_rle[rle_ptr][1] >= MAX_DURATION)
			{
				cond = 1; /*Jump to next value*/
			}

			/*Increase rle_ptr when new powerlevel starts recording.*/
			rle_ptr = rle_ptr + cond;
			rssi_val_mod = -rssi_val - 1;

			/*Check out of bounds*/
			if (rssi_val_mod >= 140)
			{
				rssi_val_mod = 139;
				// printf("out of loop I guess\n");
			}

			/*Create 2D vector*/
			record.rssi_rle[rle_ptr][0] = rssi_levels[rssi_val_mod];
			record.rssi_rle[rle_ptr][1] = (record.rssi_rle[rle_ptr][1]) * (1 - cond) + 1;
		}
		else
		{ /*I think a problem might be that it loops here without printing anything for a very long amount of time. */
			if (rle_ptr == 498){}
		}
	}
	printf("This is how many times the loop looped: %d \n", times);
	watchdog_start();

	// sample_end = RTIMER_NOW();
	if (rle_ptr < RUN_LENGTH)
		rle_ptr++;

	printf("\nNumber of sampels %d : rle_ptr %d\n", globalCounter, rle_ptr);
	printf(" \n");
	printf(" \n");
}
/*---------------------------------------------------------------------------*/

static int values_read = 0;
/*---------------------------------------------------------------------------*/
static void rssi_sampler_old(int time_window_ms)
{

	sample_st = RTIMER_NOW();
	max_samples = time_window_ms * 20;
	step_count = 1;
	rle_ptr = -1;
	record.sequence_num = 0;
	int times = 0;

	record.sequence_num++;
	rle_ptr = -1;
	record.rssi_rle[0][1] = 0;
	record.rssi_rle[0][0] = 0;
	n_samples = max_samples * 10;

	while ((rle_ptr < RUN_LENGTH) && (n_samples))
	{
		if (NETSTACK_RADIO.get_value(RADIO_PARAM_RSSI, &rssi_val) != RADIO_RESULT_OK)
		{
			printf(" ff");
		}

		values_read += 1;
		if (rssi_levels[-rssi_val] > 2)
		{

			n_samples = n_samples - 1;

			rssi_val = ((signed char)rssi_val);
			cond = 0x01 & ((record.rssi_rle[rle_ptr][0] != rssi_levels[-rssi_val - 1]) | (record.rssi_rle[rle_ptr][1] == 32767));

			rle_ptr = rle_ptr + cond;
			record.rssi_rle[rle_ptr][0] = rssi_levels[-rssi_val - 1];
			record.rssi_rle[rle_ptr][1] = (record.rssi_rle[rle_ptr][1]) * (1 - cond) + 1;
		}
	}

	printf("This is how many times the loop looped: %d \n", times);
	watchdog_start();
	step_count++;

	if (rle_ptr < RUN_LENGTH)
	{
		rle_ptr++;
	}

	// printf("\nNumber of sampels %d : rle_ptr %d\n", globalCounter, rle_ptr);
	printf(" \n");

	sample_end = RTIMER_NOW();
}

void init_power_levels()
{
	printf("POWER_LEVELS: %d", POWER_LEVELS);
#if POWER_LEVELS == 2
	for (itr = 0; itr < 120; itr++)
		if (itr < 90)
			rssi_levels[itr] = 2;
		else
			rssi_levels[itr] = 1;
#elif POWER_LEVELS == 4
	for (itr = 0; itr < 120; itr++)
		if (itr < 30)
			rssi_levels[itr] = 4;
		else if (itr >= 30 && itr < 60)
			rssi_levels[itr] = 3;
		else if (itr >= 60 && itr < 90)
			rssi_levels[itr] = 2;
		else
			rssi_levels[itr] = 1;
#elif POWER_LEVELS == 8
	for (itr = 0; itr < 120; itr++)
		if (itr < 14)
			rssi_levels[itr] = 8;
		else if (itr >= 12 && itr < 25)
			rssi_levels[itr] = 7;
		else if (itr >= 25 && itr < 38)
			rssi_levels[itr] = 6;
		else if (itr >= 38 && itr < 51)
			rssi_levels[itr] = 5;
		else if (itr >= 51 && itr < 64)
			rssi_levels[itr] = 4;
		else if (itr >= 64 && itr < 77)
			rssi_levels[itr] = 3;
		else if (itr >= 77 && itr < 90)
			rssi_levels[itr] = 2;
		else
			rssi_levels[itr] = 1;
#elif POWER_LEVELS == 16
	for (itr = 0; itr < 120; itr++)
		if (itr < 6)
			rssi_levels[itr] = 16;
		else if (itr >= 6 && itr < 12)
			rssi_levels[itr] = 15;
		else if (itr >= 12 && itr < 18)
			rssi_levels[itr] = 14;
		else if (itr >= 18 && itr < 24)
			rssi_levels[itr] = 13;
		else if (itr >= 24 && itr < 30)
			rssi_levels[itr] = 12;
		else if (itr >= 30 && itr < 36)
			rssi_levels[itr] = 11;
		else if (itr >= 36 && itr < 42)
			rssi_levels[itr] = 10;
		else if (itr >= 42 && itr < 48)
			rssi_levels[itr] = 9;
		else if (itr >= 48 && itr < 54)
			rssi_levels[itr] = 8;
		else if (itr >= 54 && itr < 60)
			rssi_levels[itr] = 7;
		else if (itr >= 60 && itr < 66)
			rssi_levels[itr] = 6;
		else if (itr >= 66 && itr < 72)
			rssi_levels[itr] = 5;
		else if (itr >= 72 && itr < 78)
			rssi_levels[itr] = 4;
		else if (itr >= 78 && itr < 84)
			rssi_levels[itr] = 3;
		else if (itr >= 84 && itr < 90)
			rssi_levels[itr] = 2;
		else
			rssi_levels[itr] = 1;
#elif POWER_LEVELS == 120
	int i_c = 140 - 1;
	for (itr = 1; itr <= 140; itr++)
	{
		rssi_levels[i_c] = itr;
		i_c--;
	}
#elif POWER_LEVELS == 20 /*Power level is read in the opposite way. */
	for (itr = 0; itr < 140; itr++)
		if (/*itr >= 60 &&*/ itr < 64)
			rssi_levels[itr] = 20;
		else if (itr >= 64 && itr < 68)
			rssi_levels[itr] = 19;
		else if (itr >= 68 && itr < 72)
			rssi_levels[itr] = 18;
		else if (itr >= 72 && itr < 76)
			rssi_levels[itr] = 17;
		else if (itr >= 76 && itr < 80)
			rssi_levels[itr] = 16;
		else if (itr >= 80 && itr < 84)
			rssi_levels[itr] = 15;
		else if (itr >= 84 && itr < 88)
			rssi_levels[itr] = 14;
		else if (itr >= 88 && itr < 92)
			rssi_levels[itr] = 13;
		else if (itr >= 92 && itr < 96)
			rssi_levels[itr] = 12;
		else if (itr >= 96 && itr < 100)
			rssi_levels[itr] = 11;
		else if (itr >= 100 && itr < 104)
			rssi_levels[itr] = 10;
		else if (itr >= 104 && itr < 108)
			rssi_levels[itr] = 9;
		else if (itr >= 108 && itr < 112)
			rssi_levels[itr] = 8;
		else if (itr >= 112 && itr < 116)
			rssi_levels[itr] = 7;
		else if (itr >= 116 && itr < 120)
			rssi_levels[itr] = 6;
		else if (itr >= 120 && itr < 124)
			rssi_levels[itr] = 5;
		else if (itr >= 124 && itr < 128)
			rssi_levels[itr] = 4;
		else if (itr >= 128 && itr < 132)
			rssi_levels[itr] = 3;
		else if (itr >= 132 && itr < 136)
			rssi_levels[itr] = 2;
		else if (itr >= 136 && itr < 140)
			rssi_levels[itr] = 1;
		else
			rssi_levels[itr] = 1; // Will never happen

#else
#error "Power levels should be one of the following values: 2, 4, 8, 16 or 120"
#endif
}
/*---------------------------------------------------------------------------*/
static void
set_channel(void)
{
	printf("Set channel\n");
	if (NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, channel_c) != RADIO_RESULT_OK)
	{
		printf("ERROR: failed to change radio channel\n");
	}
}
/*---------------------------------------------------------------------------*/
static void
run_transmit(void)
{
	NETSTACK_RADIO.prepare(packetbuf_hdrptr(), packetbuf_totlen()); // 0 == OK
	NETSTACK_RADIO.transmit(32);	// 0 == OK
	printf("Send packet size: %d, nr: %d \n",32,packet_cnt);
	packet_cnt++;
}
/*---------------------------------------------------------------------------*/
PROCESS(jammer_trigger_process, "jammer trigger process");
PROCESS_THREAD(jammer_trigger_process, ev, data)
{
	static struct etimer et,et_period;
	PROCESS_BEGIN();
	set_channel();
	NETSTACK_MAC.off();
	NETSTACK_RADIO.on();
	//frequncy to send jammer trigger msg
	etimer_set(&et, CLOCK_SECOND * 0.1);
	//period to send msg
	etimer_set(&et_period, CLOCK_SECOND * 2);

	while (1){
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		etimer_reset(&et);
		run_transmit();
		if(etimer_expired(&et_period)){
			break;
		}
		PROCESS_PAUSE();
	}
	PROCESS_END();
}

/*---------------------------------------------------------------------------*/
PROCESS(specksense_process, "SpeckSense");
PROCESS_THREAD(specksense_process, ev, data)
{
	//    static struct etimer et;
	static int count, loop=0;
	int suspicion_count;
	PROCESS_BEGIN();
	NETSTACK_MAC.off();
	NETSTACK_RADIO.on();

	watchdog_start();
	init_power_levels(); // Populate the rssi quantization levels.

	if (NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, RADIO_CHANNEL) != RADIO_RESULT_OK)
	{
		printf("ERROR: failed to change radio channel, JamSense ends\n");
	}

	record.sequence_num = 0;
	/*
		They need to have the same format after the RSSI sampler in order to be as one process
	*/

	// printf("J_D: %d RSSI_SIZE:%d POWER_LEVELS: %d \n", J_D, RSSI_SIZE, POWER_LEVELS);
	if (J_D)
	{	
		for(count = 1;count <= INTERFERENCE_NUMBER_SAMPLES * 2;count++){
		leds_single_on(LEDS_LED1);
		rssi_sampler(TIME_WINDOW);
		if (0)
		{
			print_rssi_rle();
		}
		n_clusters = kmeans(&record, rle_ptr);
		suspicion_count = check_similarity(/*PROFILING*/ 0);
		printf("Number of cluster %d\n", n_clusters);
		leds_single_off(LEDS_LED1);
		loop++;
		printf("Number of loop %d\n", loop);
		if (suspicion_count >= 5)
		{
			break;
		}
		}			
	}
	else if (J_D != 1)
	{
		rssi_sampler_old(TIME_WINDOW);
		n_clusters = kmeans_old(&record, rle_ptr);
		// print_rssi_rle();
		check_unintentional_interference(n_clusters);
		// print_interarrival(RADIO_CHANNEL, n_clusters);
	}

		
	
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
void specksense_run(void)
{
  process_start(&specksense_process, NULL);
}
/*---------------------------------------------------------------------------*/
void jammer_trigger_run(void)
{
  process_start(&jammer_trigger_process, NULL);
}


