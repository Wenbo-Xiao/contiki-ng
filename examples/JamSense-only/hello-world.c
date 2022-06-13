/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         A very simple Contiki application showing how Contiki programs look
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"
#include "sys/node-id.h"
#include "dev/watchdog.h"
#include "dev/button-hal.h"
#include <stdio.h> /* For printf() */
#include "services/jamsense/specksense.h"
#include "net/netstack.h"
#include "os/net/mac/tsch/tsch.h"
#include "constant_jammer.h"
#include "random_jammer.h"
#include "sfd_jammer.h"
//#include "sfd_debugger.h"
#include "jammer_node.h"
/*---------------------------------------------------------------------------*/
PROCESS(hello_world_process, "Hello world process");
AUTOSTART_PROCESSES(&hello_world_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(hello_world_process, ev, data)
{
  static rtimer_clock_t start;
  static int  loop=0;
  static struct etimer timer;
  PROCESS_BEGIN();
  // PROCESS_WAIT_EVENT_UNTIL(ev == button_hal_press_event);
    
  // printf("\nnode id %d \n",node_id);
  
  if (node_id == 00000)
  {
    NETSTACK_ROUTING.root_start();
  }
  else if (node_id == 00000)
  {
    printf("\nnode %d as constant jammer \n",node_id);
    constant_jamming_start();
  }
  else if (node_id == 00000)
  {
    printf("\nnode %d as random jammer \n",node_id);
    random_jamming_start();
  }
  // else if (node_id == 00000)
  // {
  //   printf("\nnode %d as sfd jammer \n",node_id);
  //   sfd_jamming_start();
  // }
  // else if (node_id == 00000)
  // {
  //   printf("\nnode %d as sfd debugger \n",node_id);
  //   sfd_debugger_start();
  // }
  else
  {
//For testing jamsense

  
  /* Setup a periodic timer that expires after 10 seconds. */
  etimer_set(&timer, CLOCK_SECOND * 1);


  while(1) 
    {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
      etimer_reset(&timer);
      
      watchdog_periodic();

      start = RTIMER_NOW();
      specksense_process();
      printf("rssi_sampler time %lu \n",RTIMER_NOW() - start);
      start = RTIMER_NOW();
      specksense_process();
      printf("classification time %lu \n",RTIMER_NOW() - start);
      loop++;
      printf("loop : %d \n",loop);
      if(loop >= 5000) break;
    }
  
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
