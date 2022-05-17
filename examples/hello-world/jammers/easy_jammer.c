/*
*	Easy interference.
*	Transmitts packages into the air on a specific channel.
*/

/*
*	Problem:
*	Affects all the channels. 
*/

#include "contiki.h"

#include <stdio.h>
#include "dev/radio.h"
#include "net/netstack.h"
#include "packetbuf.h"

/*---------------------------------------------------------------------------*/
PROCESS(easy_interference_process, "easy_interference process");
//AUTOSTART_PROCESSES(&easy_interference_process);

/*---------------------------------------------------------------------------*/
static radio_value_t channel_c = 26;

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
	NETSTACK_RADIO.transmit(120);	// 0 == OK
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(easy_interference_process, ev, data)
{
	PROCESS_BEGIN();
	
	set_channel();
	/*RSSI:-49-49-91-49-49-93-49-49-92-49-49-92-49-49-92-49*/ //On distance
	printf("%d netstack_mac_off\n", NETSTACK_MAC.off());
	printf("%d netstack_radio_on\n", NETSTACK_RADIO.on());

	//NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, RADIO_CONST_TXPOWER_MAX); //Activating this makes the interference weaker.


	while (1)
	{
		run_transmit();
		PROCESS_PAUSE();
	}
	PROCESS_END();
}
