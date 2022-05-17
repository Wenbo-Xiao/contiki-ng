/**
 * \file
 *          A debug tool in order to trigger the SFD jammer more easily.
 *          
 * \author
 *          John Kanwar <johnkanwar@hotmail.com>
 */

#include "contiki.h"

#include <stdio.h>
#include "dev/radio.h"
#include "net/netstack.h"
#include "packetbuf.h"
#include "sys/timer.h"

/*---------------------------------------------------------------------------*/
PROCESS(sfd_debugger_process, "easy_interference process");
//AUTOSTART_PROCESSES(&sfd_debugger_process);

/*---------------------------------------------------------------------------*/
static radio_value_t channel_c = 26;
static int packet_cnt = 0;

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
PROCESS_THREAD(sfd_debugger_process, ev, data)
{

	static struct etimer et;
	PROCESS_BEGIN();
	
	set_channel();
	printf("%d netstack_mac_off\n", NETSTACK_MAC.off());
	printf("%d netstack_radio_on\n", NETSTACK_RADIO.on());
	etimer_set(&et, CLOCK_SECOND * 0.3);

	while (1){
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		etimer_reset(&et);
		run_transmit();

		PROCESS_PAUSE();
	}
	PROCESS_END();
}
