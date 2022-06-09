/**
 * \file
 *          A proactive constant jammer that emits random bits with support 
 *          of nrf52840's direct test mode.
 *          
 * \author
 *          John Kanwar <johnkanwar@hotmail.com>
 */

#include "contiki.h"

#include <stdio.h>
#include "dev/radio.h"
#include "net/netstack.h"
#include "packetbuf.h"
#include "ble_dtm.h"
#include "sys/timer.h"
#include "boards.h"

#define BLE_CHANNEL 39  // channel 26
#define PACKET_LENGTH 254

#define CONSTANT_JAMMER DTM_PKT_PRBS9
#define DECEPTIVE_JAMMER DTM_PDU_TYPE_0X55
/*---------------------------------------------------------------------------*/
PROCESS(constant_jammer_process, "constant_jammer process");
//AUTOSTART_PROCESSES(&constant_jammer_process);
/*---------------------------------------------------------------------------*/
static int is_init = 0;
/*---------------------------------------------------------------------------*/

 

/*---------------------------------------------------------------------------*/
static uint32_t
dtm_cmd_put(int channel_input, int cmd_input, int length_input)
{
    dtm_cmd_t command_code = cmd_input;
    dtm_freq_t channel = channel_input;
    uint32_t length = length_input;
    dtm_pkt_type_t payload = CONSTANT_JAMMER; /*DTM_PDU_TYPE_0X55 Deceptive*/
    return dtm_cmd(command_code, channel, length, payload);
}
/*---------------------------------------------------------------------------*/
static void
start_jamming(void)
{
    dtm_cmd_put(BLE_CHANNEL, LE_TRANSMITTER_TEST, PACKET_LENGTH);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(constant_jammer_process, ev, data)
{
    PROCESS_BEGIN();
    printf("%d netstack_mac_off\n", NETSTACK_MAC.off());
    printf("%d netstack_radio_on\n", NETSTACK_RADIO.on());
    while (true){
        if (!is_init){
            dtm_init();
            NRF_RADIO->TXPOWER = 0x08UL;
            is_init = 1;
            start_jamming();
        }

        PROCESS_PAUSE();
    }
    PROCESS_END();
}
/*---------------------------------------------------------------------------*/

void
constant_jamming_start(void)
{
    process_start(&constant_jammer_process, NULL);
}