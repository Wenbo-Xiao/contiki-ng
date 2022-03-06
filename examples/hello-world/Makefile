CONTIKI_PROJECT = hello-world
all: $(CONTIKI_PROJECT)

CONTIKI = ../..

MAKE_MAC = MAKE_MAC_TSCH

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell
MODULES += $(CONTIKI_NG_SERVICES_DIR)/jamsense
MODULES += $(CONTIKI_NG_SERVICES_DIR)/orchestra
include $(CONTIKI)/Makefile.include

