#include <stdbool.h>
//#define NETSTACK_CONF_MAC select_mac_driver

#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_WARN
#define TSCH_LOG_CONF_PER_SLOT                     0
//#define TSCH_CONF_CCA_ENABLED                      1

extern bool jammer_node(void);
#define SELECT_MAC_FUNCTION jammer_node
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE	   TSCH_HOPPING_SEQUENCE_1_1
