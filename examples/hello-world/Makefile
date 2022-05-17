CONTIKI_PROJECT = hello-world
all: $(CONTIKI_PROJECT)

PROJECT_SOURCEFILES += ble_dtm.c
PROJECT_SOURCEFILES += ble_dtm_hw_nrf52.c
PROJECT_SOURCEFILES += constant_jammer.c

CONTIKI = ../..
#CFLAGS += -g
MAKE_MAC = MAKE_MAC_TSCH
#MAKE_MAC = MAKE_MAC_NULLMAC

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell
MODULES += $(CONTIKI_NG_SERVICES_DIR)/jamsense
#MODULES += $(CONTIKI_NG_SERVICES_DIR)/orchestra
include $(CONTIKI)/Makefile.include
