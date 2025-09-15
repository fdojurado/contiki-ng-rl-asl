CONTIKI_PROJECT = idle-listening
all: $(CONTIKI_PROJECT)

CONTIKI = ../../..

MAKE_MAC = MAKE_MAC_TSCH
MAKE_ROUTING = MAKE_ROUTING_OTHER
MAKE_NET = MAKE_NET_OTHER

MAKE_WITH_ORCHESTRA ?= 1

MAKE_WITH_RL_ASL ?= 0

ifeq ($(MAKE_WITH_ORCHESTRA),1)
 ORCHESTRA_EXTRA_RULES = &unicast_per_neighbor_rpl_ns

 MODULES += $(CONTIKI_NG_SERVICES_DIR)/orchestra

 # pass the Orchestra rules to the compiler
CFLAGS += -DORCHESTRA_CONF_RULES="{&eb_per_time_source,$(ORCHESTRA_EXTRA_RULES),&default_common}"
endif

ifeq ($(MAKE_WITH_RL_ASL),1)
  MODULES += $(CONTIKI_NG_SERVICES_DIR)/rl-asl
endif


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

# Run headless example:
# ./gradlew run --args='--no-gui ../../examples/sage-project/orchestra/topology-a.csc --logdir=../../examples/sage-project/orchestra/ --random-seed=1234' --stacktrace
