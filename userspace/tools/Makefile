TOP_DIR = ..

include $(TOP_DIR)/scripts/Makefile.variables
ifeq ($(IMPL), lisa)
CFLAGS	+= -DLiSA
INC = $(LINUX_INC) 
endif

ifeq ($(IMPL), linux)
CFLAGS 	+= -DLinux
INC =
endif

ifeq ($(IMPL), openwrt)
CFLAGS 	+= -DOpenWRT
INC =
endif


ifneq ($(MAKECMDGOALS), clean)
$(info ********* Building testing tools *********)
endif

LDFLAGS = -lcrypt -lpthread
include $(TOP_DIR)/scripts/Makefile.includes
include $(TOP_DIR)/switch/Makefile.switch
INC	+= $(SWITCH_INC) \
	  $(TOP_DIR)/include

SRCS	= filter.c login.c swctl.c swclid.c reboot.c

SIMPLE_TARGETS = filter login reboot swclid
TARGETS = $(SIMPLE_TARGETS) swctl

all: $(TARGETS)

include $(TOP_DIR)/scripts/Makefile.rules

dep: $(DEPS)

swctl: swctl.o $(TOP_DIR)/switch/libswitch.so
	$(Q) $(MAKE) -C $(TOP_DIR)/lib
	$(QUIET_LD)$(CC) -o $@ $^ ../lib/if_generic.o \
		../lib/netlink.o ../lib/util.o ../lib/mm.o -lrt $(LDFLAGS)
	
$(SIMPLE_TARGETS): % : %.o
	$(QUIET_LD)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
