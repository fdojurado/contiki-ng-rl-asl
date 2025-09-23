CONTIKI_PROJECT = idle-listening
all: $(CONTIKI_PROJECT)

CONTIKI = ../../..

MAKE_MAC = MAKE_MAC_TSCH
MAKE_ROUTING = MAKE_ROUTING_OTHER
MAKE_NET = MAKE_NET_OTHER

MAKE_WITH_ORCHESTRA ?= 1

MAKE_WITH_RL_ASL ?= 0

MAKE_WITH_RL_ASL_TRAIN ?= 1

MAKE_WITH_RL_ASL_TOPOLOGY_A ?= 1

ifeq ($(MAKE_WITH_RL_ASL_TOPOLOGY_A),1)
  CFLAGS += -DTOPOLOGY_A=1
else
  CFLAGS += -DTOPOLOGY_A=0
endif

# CFLAGS += -DNETSTACK_CONF_NETWORK=rl_asl_net_driver
# CFLAGS += -DNETSTACK_CONF_ROUTING=rl_asl_routing_driver
# Lets define a flag that we can use in the code to check if we are using rl_asl_net_driver and rl_asl_routing_driver
MAKE_WITH_RL_ASL_NET ?= 1
ifeq ($(MAKE_WITH_RL_ASL_NET),1)
  CFLAGS += -DWITH_RL_ASL_NET=1
  CFLAGS += -DNETSTACK_CONF_NETWORK=rl_asl_net_driver
  CFLAGS += -DNETSTACK_CONF_ROUTING=rl_asl_routing_driver
endif

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

ifeq ($(MAKE_WITH_ORCHESTRA),1)
 ORCHESTRA_EXTRA_RULES = &unicast_per_neighbor_rpl_ns
 ifeq ($(MAKE_WITH_RL_ASL),1)
  ORCHESTRA_EXTRA_RULES = &unicast_rl_asl
 endif
 # pass the Orchestra rules to the compiler
 CFLAGS += -DORCHESTRA_CONF_RULES="{&eb_per_time_source,$(ORCHESTRA_EXTRA_RULES),&default_common}"
 MODULES += $(CONTIKI_NG_SERVICES_DIR)/orchestra
endif

CFLAGS += -DSLIP_ARCH_CONF_ENABLE=0 -DWITH_SLIP=0

MODULES_SOURCES_EXCLUDES += slip.c

CONTIKI_SOURCES_EXCLUDES_COOJA += slip-arch.c

PROJECT_SOURCEFILES += ../rl-asl-buf.c ../rl-asl-utils.c ../rl-asl-ds-nbr.c ../rl-asl-data-packet-processor.c ../rl-asl-data-packet-generator.c ../rl-asl-net-processor.c ../rl-asl-net.c ../rl-asl-routing.c
# PROJECT_SOURCEFILES += ../rl-asl-buf.c ../rl-asl-data-packet-generator.c ../rl-asl-net-processor.c ../rl-asl-net.c ../rl-asl-packets.c ../rl-asl-routing.c
CFLAGS += -I../../rl-asl

ifeq (${ROOT}, 1)
 CFLAGS += -DROOT
else ifeq (${RELAY}, 1)
 MODULES += $(CONTIKI_NG_SERVICES_DIR)/simple-energest
 CFLAGS += -DRELAY
 ifeq ($(MAKE_WITH_RL_ASL),1)
  PROJECT_SOURCEFILES += ../rl-asl-handshake.c
 endif
else ifeq (${LEAF}, 1)
 MODULES += $(CONTIKI_NG_SERVICES_DIR)/simple-energest
 CFLAGS += -DLEAF
 ifeq ($(MAKE_WITH_RL_ASL),1)
  PROJECT_SOURCEFILES += ../rl-asl-handshake.c
 endif
endif

include $(CONTIKI)/Makefile.dir-variables
include $(CONTIKI)/Makefile.include

# Run headless example:
# ./gradlew run --args='--no-gui ../../examples/sage-project/orchestra/topology-a.csc --logdir=../../examples/sage-project/orchestra/ --random-seed=1234' --stacktrace
