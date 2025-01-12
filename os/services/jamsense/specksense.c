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
#include "channel_queue.h"
#include "cfs/cfs.h"
#include "nrf52840_bitfields.h"
#include "nrf52840.h"

QUEUE(jamsense_queue);
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
#define tick_minute RTIMER_SECOND*60
/*---------------------------------------------------------------------------*/
//! Global variables

static int n_clusters;
static uint16_t classification_status = 0;

//! Global variables for RSSI scan
static int rssi_val, /*rssi_valB,*/ rle_ptr = -1, /*rle_ptrB = -1,*/
		   pre_rssi_val = 0, /*previous rssi_valB,*/
 		   cond, itr,
		   rssi_val_mod;

static unsigned rssi_levels[RSSI_SIZE];
static struct record record;
//static struct etimer jamsense_timer;

static int packet_cnt = 0;
static int sample_cnt = 0;
static uint8_t pre_measurement_channel = 0;
static uint8_t specksense_loop = 0;

#if CHANNEL_METRIC == 2
static uint16_t cidx;
static int itr_j;
static int current_channel = RADIO_CHANNEL;
#endif

rtimer_clock_t rssi_stop_time;

/*---------------------------------------------------------------------------*/
/**
 * \brief The jammer data type
 */
typedef struct jammer {
  int type;   //1 for proactive jammer; 2 for reactive jammer
  rtimer_clock_t time; //time when when jammer classified
} jammer;

//mapping from channel 10-30 to buffer 0-20
static jammer jammer_queue[CHANNEL_SEQUENCE_AMOUNT];
/*---------------------------------------------------------------------------*/
static void init_jammer_queue()
{
	int i;
	for(i = 0; i < CHANNEL_SEQUENCE_AMOUNT; i++) 
	{
    	jammer_queue[i].type = 0;
		jammer_queue[i].time = 0;
    }
}
/*---------------------------------------------------------------------------*/
void update_jammer_status(int channel, int type)
{
	channel = channel - 10;
	jammer_queue[channel].type = type;
	jammer_queue[channel].time = RTIMER_NOW();
}
/*---------------------------------------------------------------------------*/
static bool check_jammer_status(int channel)
{
	channel = channel - 10;
	if(RTIMER_NOW() - jammer_queue[channel].time > tick_minute || jammer_queue[channel].type == 0)
	{
		return true;
	}
	else
	{
		if(jammer_queue[channel].type == 1)
		{
			LOG_INFO("CH%d: PROACTIVE JAMMER RECORD EXIST\n",specksense_channel_peek());
		}
		else if (jammer_queue[channel].type == 2)
		{
			LOG_INFO("CH%d: REACTIVE JAMMER RECORD EXIST\n",specksense_channel_peek());
		}
		specksense_channel_remove();
		return false;
	}
}
/*---------------------------------------------------------------------------*/
void rssi_sampler(int sample_amount, int channel, rtimer_clock_t rssi_stop_time)
{
	/*sample_amount is the amount that is going to do, sample_cnt is the amount that already done, rle_ptr is sample amount for current loop*/
	if(sample_amount + sample_cnt > RUN_LENGTH)
	{
		sample_amount = RUN_LENGTH - sample_cnt;
	}
	//LOG_INFO("START RSSI, Sample amount: %d  \n",sample_amount);
	// sample_st = RTIMER_NOW();
	rle_ptr = 0;

	if(channel != pre_measurement_channel)
		{
			sample_cnt = 0;
			reset_kmeans();
			LOG_INFO("channel changed to %d, reset specksense!\n",channel);
		}
	pre_measurement_channel = channel;

	//int times = 0;
#if TSCH_WITH_JAMSENSE == 1
	//if(!tsch_get_lock())
	{
		
#endif
	if (NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, channel) != RADIO_RESULT_OK)
	{
		//LOG_ERR("ERROR: failed to change radio channel, RSSI Sampler failed!\n");
		sample_cnt = 0;
	}
	else
	{
		rle_ptr = rle_ptr + sample_cnt;
		/* Need to explicitly turn on for Coojamotes */
  		NETSTACK_RADIO.on();
		
		while ((rle_ptr < sample_amount + sample_cnt))
		{
			watchdog_periodic();

			/* quit rssi if doesnt have enough time */
			if((RTIMER_NOW() + 200) > rssi_stop_time)
			{
				sample_cnt = rle_ptr > 0 ? rle_ptr : 0;
				break;
			}

			if (NETSTACK_RADIO.get_value(RADIO_PARAM_RSSI, &rssi_val) != RADIO_RESULT_OK)
			{
				LOG_ERR("ERROR: failed to get RSSI value!\n");
				break;
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
#if QUICK_PROACTIVE == 1
				// Avoid energy fluctuations, may need to be fine-tuned	
				if(rssi_levels[-debug_rssi - 1] > INTERFERENCE_POWER_LEVEL_THRESHOLD && (rssi_val - pre_rssi_val) < 3 && (rssi_val - pre_rssi_val) > -3)
				{
					rssi_val = pre_rssi_val;
				}
				else
				{
					pre_rssi_val = rssi_val;
				}		
#endif
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

	}
#if TSCH_WITH_JAMSENSE == 1
	NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, tsch_current_channel);
	//tsch_release_lock();
	}
#endif

	//LOG_INFO("This is how many times the loop looped: %d \n", times);
	watchdog_start();
	sample_cnt = rle_ptr > 0 ? rle_ptr : 0;
	LOG_INFO("CH%d: RSSI sample %d\n", channel, sample_cnt);
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
	queue_init(jamsense_queue);
	init_jammer_queue();
	// etimer_set(&jamsense_timer, CLOCK_SECOND * 3);
	// process_start(&specksense, NULL);
}
/*---------------------------------------------------------------------------*/
void specksense_channel_add(unsigned int channel)
{
  queue_enqueue(jamsense_queue , channel);
}
/*---------------------------------------------------------------------------*/
void specksense_channel_remove(void)
{
  queue_dequeue(jamsense_queue);
}
/*---------------------------------------------------------------------------*/
int specksense_channel_peek(void)
{
  channel *this;
  this = queue_peek(jamsense_queue);
   if(this == NULL) {
	   //printf("CHANNEL 0 \n");
      return 0;
    } else {
		//printf("CHANNEL %d \n",((unsigned int)this->value));
      return ((unsigned int)this->value);
	  }
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
void jammer_trigger_process(void)
{
  process_start(&jammer_trigger, NULL);
}

/*---------------------------------------------------------------------------*/
PROCESS(classification, "classification process");
PROCESS_THREAD(classification, ev, data)
{
	//static rtimer_clock_t start;
	PROCESS_BEGIN();
 
        classification_status = 1;
		init_kmeans(&record, rle_ptr);
		while(n_clusters == -1)
		{
			n_clusters = kmeans(&record, rle_ptr);
			if(n_clusters == -1)
			{
				PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
			}
		}
		LOG_INFO("kmeans %d clusters!\n",n_clusters);
		specksense_loop++;
        if((RTIMER_NOW() + 4000) > rssi_stop_time)
		{
			PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
		}

		if (n_clusters > 0 )
		{
			//if find jammer, remove channel
			if(check_similarity(/*PROFILING*/ 0))
			{
				specksense_loop = 0;
				specksense_channel_remove();
			}
		}

		//if havent found jammer within 2 specksense loop, remove channel
		if(specksense_loop >= 2)
		{
			specksense_loop = 0;
			specksense_channel_remove();
		}

		sample_cnt = 0;
		n_clusters = -1;
        classification_status = 0;

	PROCESS_END();
}

void classification_process()
{
    if(classification_status == 0)
    {
        process_start(&classification, NULL);
    }
    else
    {
        process_poll(&classification);
    }
}

/*---------------------------------------------------------------------------*/
int specksense_process()
{
	//LOG_INFO("specksense!\n");
	//When there is enough samples, do kmeans. Otherwise do RSSI
	if (sample_cnt >= RUN_LENGTH)
	{		
		classification_process();
		return 1;
	}
	else
	{
#if TSCH_WITH_JAMSENSE == 1
		int channel_rssi = specksense_channel_peek();
		if (channel_rssi != 0 && check_jammer_status(channel_rssi))
		{
			rssi_sampler(SAMPLE_AMOUNT,channel_rssi,rssi_stop_time);
		}

		/*Doing Rssi when target channel is curent tsch channel*/
		// if (channel_rssi == tsch_current_channel && check_jammer_status(channel_rssi))
		// {
		// 	rssi_sampler(SAMPLE_AMOUNT,channel_rssi,rssi_stop_time);
		// }
#else
		rssi_stop_time = RTIMER_NOW() + 1000000;
		specksense_channel_add(26);
		int channel_rssi = specksense_channel_peek();
		if (channel_rssi != 0)
		{
			rssi_sampler(SAMPLE_AMOUNT,channel_rssi,rssi_stop_time);
		}
		if(0)
		{
			check_jammer_status(channel_rssi);
		}
#endif
		return 0;
	}
}