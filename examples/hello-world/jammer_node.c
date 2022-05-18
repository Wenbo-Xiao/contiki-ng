#include "contiki.h"
#include "sys/node-id.h"
#include <stdbool.h>
#include <stdint.h>
bool jammer_node(void)
{
  if (node_id == 52853 || node_id == 48020 || node_id == 51502 || node_id == 25868 || node_id == 45808)
	    return true;
  else 
	    return false;
}
