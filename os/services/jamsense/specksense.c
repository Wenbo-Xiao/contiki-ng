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
#include "net/mac/tsch/tsch.h"
#include "net/mac/tsch/tsch-stats.h"
#include "net/mac/tsch/tsch-slot-operation.h"

#include "lib/random.h"
#include "sys/pt.h"
#include "sys/timer.h"
#include "sys/ctimer.h"
#include "sys/rtimer.h"
#include "sys/log.h"
#define LOG_MODULE "TSCH JamSense"
#define LOG_LEVEL LOG_LEVEL_MAC

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

#if QUICK_PROACTIVE == 1
#define MAX_DURATION 500
#else
#define MAX_DURATION 1000
#endif

/*---------------------------------------------------------------------------*/
//! Global variables

static uint16_t n_clusters;

//! Global variables for RSSI scan
static int rssi_val, /*rssi_valB,*/ rle_ptr = -1, /*rle_ptrB = -1,*/
 		   cond, itr,
		   rssi_val_mod;

static unsigned rssi_levels[RSSI_SIZE];
static struct record record;
//static struct etimer jamsense_timer;

static int packet_cnt = 0;
static int sample_cnt = 0;
static uint8_t pre_measurement_channel = 0;

#if CHANNEL_METRIC == 2
static uint16_t cidx;
static int itr_j;
static int current_channel = RADIO_CHANNEL;
#endif

PROCESS(specksense, "SpeckSense");

/*---------------------------------------------------------------------------*/
void rssi_sampler(int sample_amount, int channel)
{
	/*sample_amount is the amount that is going to do, sample_cnt is the amount that already done*/
	if(sample_amount + sample_cnt > RUN_LENGTH)
	{
		sample_amount = RUN_LENGTH - sample_cnt;
	}
	//LOG_INFO("START RSSI, Sample amount: %d  \n",sample_amount);
	// sample_st = RTIMER_NOW();
	rle_ptr = -1;
	

	record.rssi_rle[0][1] = 0;
	record.rssi_rle[0][0] = 0;

	#if !MAC_CONF_WITH_TSCH
	channel = 26;
	#endif

	if(channel != pre_measurement_channel)
		{
			sample_cnt = 0;
			reset_kmeans();
			//LOG_INFO("channel changed to %d, reset specksense!\n",channel);
		}
	pre_measurement_channel = channel;

	int times = 0;
	//watchdog_periodic();

	if (NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, channel) != RADIO_RESULT_OK)
	{
		//LOG_ERR("ERROR: failed to change radio channel, RSSI Sampler failed!\n");
		sample_cnt = 0;
	}
#if MAC_CONF_WITH_TSCH
	else if (tsch_get_lock())
#else
	else
#endif
	{
		rle_ptr = rle_ptr + sample_cnt;
		/* Need to explicitly turn on for Coojamotes */
  		NETSTACK_RADIO.on();
		
		while ((rle_ptr < sample_amount + sample_cnt))
		{
			times++;
			/*Get RSSI value*/

			/*Start time*/
			if (NETSTACK_RADIO.get_value(RADIO_PARAM_RSSI, &rssi_val) != RADIO_RESULT_OK)
			{
				//LOG_ERR("ERROR: failed to get RSSI value!\n");
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
				// LOG_DBG(" rle_ptr: %d  level: %d  duratuion: %d \n",rle_ptr,record.rssi_rle[rle_ptr][0],	record.rssi_rle[rle_ptr][1]);
			}
			else
			{ /*I think a problem might be that it loops here without printing anything for a very long amount of time. */
				// if (rle_ptr == 498){}
			}
		}
#if MAC_CONF_WITH_TSCH
		tsch_release_lock();
#endif
	}
	//LOG_INFO("This is how many times the loop looped: %d \n", times);
	//watchdog_start();
	sample_cnt = rle_ptr;
	//LOG_INFO(" rle_ptr %d\n", rle_ptr);
}
/*---------------------------------------------------------------------------*/

void init_power_levels()
{
	LOG_INFO("POWER_LEVELS: %d", POWER_LEVELS);
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
	// etimer_set(&jamsense_timer, CLOCK_SECOND * 3);
	// process_start(&specksense, NULL);
}
/*---------------------------------------------------------------------------*/
static void set_channel(void)
{
	static uint8_t measurement_channel = TSCH_STATS_FIRST_CHANNEL;
	printf("Set channel\n");
	if (NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, measurement_channel) != RADIO_RESULT_OK)
	{
		printf("ERROR: failed to change radio channel\n");
	}
}
/*---------------------------------------------------------------------------*/
static void run_transmit(void)
{
	NETSTACK_RADIO.prepare(packetbuf_hdrptr(), packetbuf_totlen()); // 0 == OK
	NETSTACK_RADIO.transmit(32);	// 0 == OK
	printf("Send packet size: %d, nr: %d \n",32,packet_cnt);
	packet_cnt++;
}
/*---------------------------------------------------------------------------*/
PROCESS(jammer_trigger, "jammer trigger process");
PROCESS_THREAD(jammer_trigger, ev, data)
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
PROCESS_THREAD(specksense, ev, data)
{
	//static rtimer_clock_t start;
	PROCESS_BEGIN();
	while(1)
	{
		PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
    	// etimer_reset(&jamsense_timer);
		LOG_INFO("specksense!\n");
		//get current channel
		//

		// NETSTACK_RADIO.on();
		// watchdog_start();


		//

		// if (sample_cnt >= RUN_LENGTH)
		// {		
		// 	n_clusters = kmeans(&record, rle_ptr);
		// 	LOG_INFO("Got %d clusters!\n",n_clusters);
		// 	if (n_clusters > 0 )
		// 	{
		// 		check_similarity(/*PROFILING*/ 0);
		// 	}
		// 	sample_cnt = 0;
		// }
		}
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
void specksense_process()
{
	//process_poll(&specksense);
	//LOG_INFO("specksense!\n");
	if (sample_cnt >= RUN_LENGTH)
	{		
		n_clusters = kmeans(&record, rle_ptr);
		LOG_INFO("Got %d clusters!\n",n_clusters);
		if (n_clusters > 0 )
		{
			check_similarity(/*PROFILING*/ 0);
		}
		sample_cnt = 0;
	}

}
/*---------------------------------------------------------------------------*/
void jammer_trigger_process(void)
{
  process_start(&jammer_trigger, NULL);
}


