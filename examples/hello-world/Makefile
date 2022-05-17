CONTIKI_PROJECT = hello-world
all: $(CONTIKI_PROJECT)

CONTIKI = ../..
#CFLAGS += -g
MAKE_MAC = MAKE_MAC_TSCH
#MAKE_MAC = MAKE_MAC_NULLMAC


include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell
MODULES += $(CONTIKI_NG_SERVICES_DIR)/jamsense
#MODULES += $(CONTIKI_NG_SERVICES_DIR)/orchestra
MODULES += examples/hello-world/jammers
include $(CONTIKI)/Makefile.include
