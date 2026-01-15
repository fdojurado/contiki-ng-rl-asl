CONTIKI_PROJECT = udp-client udp-server
all: $(CONTIKI_PROJECT)

CONTIKI = ../../..

CFLAGS += -DBUILD_WITH_IDLE_LISTENING=1

MAKE_WITH_TRAFFIC_PATTERN ?= 3

CFLAGS += -DTRAFFIC_PATTERN=$(MAKE_WITH_TRAFFIC_PATTERN)

MAKE_MAC = MAKE_MAC_TSCH
MAKE_NET ?= MAKE_NET_IPV6

ORCHESTRA_EXTRA_RULES = &unicast_per_neighbor_rpl_ns
# pass the Orchestra rules to the compiler
CFLAGS += -DORCHESTRA_CONF_RULES="{&eb_per_time_source,$(ORCHESTRA_EXTRA_RULES),&default_common}"
MODULES += $(CONTIKI_NG_SERVICES_DIR)/orchestra

CFLAGS += -DSLIP_ARCH_CONF_ENABLE=0 -DWITH_SLIP=0

MODULES_SOURCES_EXCLUDES += slip.c

CONTIKI_SOURCES_EXCLUDES_COOJA += slip-arch.c

ifeq (${ROOT}, 1)
 CFLAGS += -DROOT
else ifeq (${RELAY}, 1)
 MODULES += $(CONTIKI_NG_SERVICES_DIR)/simple-energest
 CFLAGS += -DRELAY
else ifeq (${LEAF}, 1)
 MODULES += $(CONTIKI_NG_SERVICES_DIR)/simple-energest
 CFLAGS += -DLEAF
endif

include $(CONTIKI)/Makefile.dir-variables
include $(CONTIKI)/Makefile.include
