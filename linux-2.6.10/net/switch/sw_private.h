#ifndef _SW_PRIVATE_H
#define _SW_PRIVATE_H

#include <linux/netdevice.h>
#include <linux/list.h>
#include <asm/semaphore.h>

struct net_switch {
	/* List of all ports in the switch */
	struct list_head ports;

	/* We don't want an interface to be added or removed "twice". This
	   would mess up the usage counter, the promiscuity counter and many
	   other things.
	 */
	struct semaphore adddelif_mutex;
};

struct net_switch_port {
	struct list_head lh;
	struct net_device *dev;
};

#endif
