/*
 * Configuration in project-conf.h
 * #define NETSTACK_CONF_MAC select_mac_driver
 */
#include <stdbool.h>
#include "net/mac/mac.h"
#include "net/mac/nullmac/nullmac.h"
#include "net/mac/tsch/tsch.h"


static const struct mac_driver *current_mac_driver;
/*---------------------------------------------------------------------------*/
static void
mac_init(void)
{
  /* Select if TSCH or nullmac should be used */
#ifndef SELECT_MAC_FUNCTION
  bool use_nullmac = false;
#else
  bool use_nullmac = SELECT_MAC_FUNCTION();
#endif
  if(use_nullmac) {
    current_mac_driver = &nullmac_driver;
  } else {
    current_mac_driver = &tschmac_driver;
  }
  current_mac_driver->init();
}
/*---------------------------------------------------------------------------*/
static void
mac_send(mac_callback_t sent, void *ptr)
{
  current_mac_driver->send(sent, ptr);
}
/*---------------------------------------------------------------------------*/
static void
mac_input(void)
{
  current_mac_driver->input();
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
  return current_mac_driver->on();
}
/*---------------------------------------------------------------------------*/
static int
off(void)
{
  return current_mac_driver->off();
}
/*---------------------------------------------------------------------------*/
static int
max_payload(void)
{
  return current_mac_driver->max_payload();
}
/*---------------------------------------------------------------------------*/
const struct mac_driver select_mac_driver = {
  "select-mac",
  mac_init,
  mac_send,
  mac_input,
  on,
  off,
  max_payload
};
/*---------------------------------------------------------------------------*/
