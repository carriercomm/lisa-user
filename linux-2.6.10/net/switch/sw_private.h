#ifndef _SW_PRIVATE_H
#define _SW_PRIVATE_H

#include <linux/netdevice.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <asm/semaphore.h>
#include <asm/atomic.h>

#define SW_HASH_SIZE_BITS 12
#define SW_HASH_SIZE (1 << SW_HASH_SIZE_BITS)

/* Hash bucket */
struct net_switch_bucket {
	/*
		List of fdb_entries
	*/
	struct list_head entries;

	/* To avoid adding a fdb_entry twice we protect each bucket
	   with a rwlock. Since each bucket has its own rwlock, this
	   doesn't lead to a bottleneck.
	 */
	rwlock_t lock;
};

#define SW_MAX_VLAN_NAME	32

struct net_switch_vdb_entry {
	char name[SW_MAX_VLAN_NAME];
	struct list_head ports;
};

struct net_switch {
	/* List of all ports in the switch */
	struct list_head ports;

	/* To avoid deadlocks and brain damage, we have one global mutex for
	   all configuration tasks.
	 */
	struct semaphore cfg_mutex;
	
	/* Switch forwarding database (hashtable) */
	struct net_switch_bucket fdb[SW_HASH_SIZE];

	/* Vlan database */
	struct net_switch_vdb_entry * volatile vdb[4096];
    /* Cache of link structures */
    kmem_cache_t *vdb_cache;
};

struct net_switch_port {
	/* Linking with other ports in a list */
	struct list_head lh;

	/* Physical device associated with this port */
	struct net_device *dev;

	/* Pointer to the switch owning this port */
	struct net_switch *sw;

	unsigned int flags;
	int vlan;

	/* Bitmap of forbidden vlans for trunk ports.
	   512 * 8 bits = 4096 bits => 4096 vlans
	 */
	unsigned char forbidden_vlans[512];
};

struct net_switch_vdb_link {
	struct list_head lh;
	struct net_switch_port *port;
};

#define SW_PFL_DISABLED     0x01
#define SW_PFL_TRUNK		0x02

/* Hash Entry */
struct net_switch_fdb_entry {
	struct list_head lh;
	unsigned char mac[ETH_ALEN];
	int vlan;
	struct net_switch_port *port;
	unsigned long stamp;
};

struct skb_extra {
	int vlan;
	int has_vlan_tag;
};

#define sw_allow_vlan(bitmap, vlan) (bitmap)[(vlan) / 8] |= (1 << ((vlan) % 8))
#define sw_forbid_vlan(bitmap, vlan) (bitmap)[(vlan) / 8] &= ~(1 << ((vlan) % 8))

extern struct net_switch sw;

/* sw_fdb.c */
extern void sw_fdb_init(struct net_switch *sw);
extern void fdb_cleanup_port(struct net_switch_port *);
extern void fdb_learn(unsigned char *mac, struct net_switch_port *port, int vlan);
extern void sw_fdb_exit(struct net_switch *sw);

/* sw_proc.c */
extern int init_switch_proc(void);
extern void cleanup_switch_proc(void);

/* sw_vdb.c */
extern void sw_vdb_add_vlan(struct net_switch *sw, int vlan, char *name);
extern void sw_vdb_del_vlan(struct net_switch *sw, int vlan);
extern void sw_vdb_set_vlan_name(struct net_switch *sw, int vlan, char *name);
extern void __init sw_vdb_init(struct net_switch *sw);
extern void __exit sw_vdb_exit(struct net_switch *sw);
extern void sw_vdb_add_port(int vlan, struct net_switch_port *port);
extern void sw_vdb_del_port(int vlan, struct net_switch_port *port);

#endif
