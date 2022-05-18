CONTIKI_PROJECT = hello-world
all: $(CONTIKI_PROJECT)

PROJECT_SOURCEFILES += constant_jammer.c random_jammer.c sfd_jammer.c sfd_debugger.c 
PROJECT_SOURCEFILES += select-mac.c jammer_node.c
 
CONTIKI = ../..
#CFLAGS += -g
MAKE_MAC = MAKE_MAC_TSCH
#MAKE_MAC = MAKE_MAC_NULLMAC

MODULES += $(CONTIKI_NG_MAC_DIR)/nullmac
MODULES += $(CONTIKI_NG_MAC_DIR)/tsch

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell
MODULES += $(CONTIKI_NG_SERVICES_DIR)/jamsense
#MODULES += $(CONTIKI_NG_SERVICES_DIR)/orchestra
include $(CONTIKI)/Makefile.include
