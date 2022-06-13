#include "contiki.h"
#include "sys/node-id.h"
#include <stdbool.h>
#include <stdint.h>
bool jammer_node(void)
{
  if (node_id == 39153)
	    return true;
  else 
	    return false;
}
