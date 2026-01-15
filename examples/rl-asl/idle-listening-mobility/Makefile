CONTIKI_PROJECT = idle-listening udp-server
all: $(CONTIKI_PROJECT)

CONTIKI = ../../..

MAKE_MAC = MAKE_MAC_TSCH
MAKE_ROUTING ?= MAKE_ROUTING_OTHER
MAKE_NET ?= MAKE_NET_IPV6

CFLAGS += -DBUILD_WITH_IDLE_LISTENING=1

MAKE_WITH_RL_ASL ?= 0

MAKE_WITH_RL_ASL_TRAIN ?= 1

MAKE_WITH_RL_ASL_TOPOLOGY_A ?= 1

MAKE_WITH_LINK_BASED_ORCHESTRA ?= 0

ifeq ($(MAKE_WITH_RL_ASL),1)
MAKE_WITH_RL_ASL_MODEL ?= ../models/topology-a
# Convert MODEL to an include path
MODEL_INCLUDE += -I$(MAKE_WITH_RL_ASL_MODEL)
CFLAGS += $(MODEL_INCLUDE) -DRL_ASL_MODEL_SCENARIO=\"$(MAKE_WITH_RL_ASL_MODEL)\"
# print 
$(info MAKE_WITH_RL_ASL_MODEL is $(MAKE_WITH_RL_ASL_MODEL))
endif


ifeq ($(MAKE_WITH_RL_ASL_TOPOLOGY_A),1)
  CFLAGS += -DTOPOLOGY_A=1
else
  CFLAGS += -DTOPOLOGY_A=0
endif

MAKE_WITH_TRAFFIC_PATTERN ?= 3

CFLAGS += -DTRAFFIC_PATTERN=$(MAKE_WITH_TRAFFIC_PATTERN)

ifeq ($(MAKE_WITH_RL_ASL),1)
  MODULES += $(CONTIKI_NG_SERVICES_DIR)/rl-asl
  ifeq ($(MAKE_WITH_RL_ASL_TRAIN),1)
    CFLAGS += -DRL_ASL_CONF_MODE_TRAIN=1
    CFLAGS += -DRL_ASL_CONF_MODE_EVAL=0
  else
    CFLAGS += -DRL_ASL_CONF_MODE_TRAIN=0
    CFLAGS += -DRL_ASL_CONF_MODE_EVAL=1
  endif
endif

ORCHESTRA_EXTRA_RULES = &unicast_per_neighbor_rpl_ns
ifeq ($(MAKE_WITH_LINK_BASED_ORCHESTRA),1)
 ORCHESTRA_EXTRA_RULES = &unicast_per_neighbor_link_based_static
 ifeq ($(MAKE_WITH_RL_ASL),1)
  ORCHESTRA_EXTRA_RULES = &rl_asl_link_based
 endif
 ifeq ($(MAKE_WITH_PRIL),1)
  ORCHESTRA_EXTRA_RULES = &pril_unicast_link_based
 endif
else
  ifeq ($(MAKE_WITH_RL_ASL),1)
  ORCHESTRA_EXTRA_RULES = &unicast_rl_asl
  endif
endif
# pass the Orchestra rules to the compiler
CFLAGS += -DORCHESTRA_CONF_RULES="{&eb_per_time_source,$(ORCHESTRA_EXTRA_RULES),&default_common}"
MODULES += $(CONTIKI_NG_SERVICES_DIR)/orchestra

CFLAGS += -DSLIP_ARCH_CONF_ENABLE=0 -DWITH_SLIP=0

MODULES_SOURCES_EXCLUDES += slip.c

CONTIKI_SOURCES_EXCLUDES_COOJA += slip-arch.c

PROJECT_SOURCEFILES += rl-asl-handshake.c rl-asl-ds-nbr.c

ifeq (${ROOT}, 1)
 CFLAGS += -DROOT
else ifeq (${RELAY}, 1)
 MODULES += $(CONTIKI_NG_SERVICES_DIR)/simple-energest
 CFLAGS += -DRELAY
 PROJECT_SOURCEFILES += rl-asl-data-packet-processor.c rl-asl-ds-nbr.c
else ifeq (${LEAF}, 1)
 MODULES += $(CONTIKI_NG_SERVICES_DIR)/simple-energest
 PROJECT_SOURCEFILES += rl-asl-data-packet-generator.c rl-asl-data-packet-processor.c rl-asl-ds-nbr.c
 CFLAGS += -DLEAF
endif

include $(CONTIKI)/Makefile.dir-variables
include $(CONTIKI)/Makefile.include

# Run headless example:
# ./gradlew run --args='--no-gui ../../examples/sage-project/orchestra/topology-a.csc --logdir=../../examples/sage-project/orchestra/ --random-seed=1234' --stacktrace
