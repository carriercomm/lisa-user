#include "sw_private.h"
#include "sw_debug.h"

struct net_device *sw_vif_find(struct net_switch *sw, int vlan) {
	struct net_switch_vif_priv *priv;
	list_for_each_entry(priv, &sw->vif, lh) {
		if(priv->bogo_port.vlan == vlan)
			return priv->bogo_port.dev;
	}
	return NULL;
}

int sw_vif_open(struct net_device *dev) {
	netif_start_queue(dev);
	return 0;
}

int sw_vif_stop(struct net_device *dev) {
	netif_stop_queue(dev);
	return 0;
}

int sw_vif_hard_start_xmit(struct sk_buff *skb, struct net_device *dev) {
	struct net_switch_vif_priv *priv = netdev_priv(dev);
	struct skb_extra skb_e;
	unsigned long pkt_len = skb->data_len;

	skb_e.vlan = priv->bogo_port.vlan;
	skb_e.has_vlan_tag = 0;
	skb->mac.raw = skb->data;
	skb->mac_len = ETH_HLEN;
	skb_pull(skb, ETH_HLEN);
	if(sw_forward(&priv->bogo_port, skb, &skb_e)) {
		priv->stats.tx_packets++;
		priv->stats.tx_bytes += pkt_len;
	} else {
		priv->stats.tx_errors++;
	}
	return 0;
}

void sw_vif_tx_timeout(struct net_device *dev) {
}

struct net_device_stats * sw_vif_get_stats(struct net_device *dev) {
	struct net_switch_vif_priv *priv = netdev_priv(dev);
	return &priv->stats;
}

int sw_vif_set_config(struct net_device *dev, struct ifmap *map) {
	return 0;
}

int sw_vif_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd) {
	return 0;
}

int sw_vif_addif(struct net_switch *sw, int vlan) {
	char buf[9];
	struct net_device *dev;
	struct net_switch_vif_priv *priv;
	int result;
	
	if(vlan < 1 || vlan > 4095)
		return -EINVAL;
	if(sw_vif_find(sw, vlan))
		return -EEXIST;
	/* We can now safely create the new interface and this is no race
	   because this is called only from ioctl() and ioctls are
	   mutually exclusive (a semaphore in socket ioctl routine)
	 */
	sprintf(buf, "vlan%d", vlan);
	dbg("About to alloc netdev for vlan %d\n", vlan);
	dev = alloc_netdev(sizeof(struct net_switch_vif_priv), buf, ether_setup);
	if(dev == NULL)
		return -EINVAL;
	memcpy(dev->dev_addr, sw->vif_mac, ETH_ALEN);
	dev->dev_addr[ETH_ALEN - 2] ^= vlan / 0x100;
	dev->dev_addr[ETH_ALEN - 1] ^= vlan % 0x100;

	dev->open = sw_vif_open;
	dev->stop = sw_vif_stop;
	dev->set_config = sw_vif_set_config;
	dev->hard_start_xmit = sw_vif_hard_start_xmit;
	dev->do_ioctl = sw_vif_do_ioctl;
	dev->get_stats = sw_vif_get_stats;
	dev->tx_timeout = sw_vif_tx_timeout;
	dev->watchdog_timeo = HZ;
	
	priv = netdev_priv(dev);
	INIT_LIST_HEAD(&priv->bogo_port.lh); /* paranoid */
	priv->bogo_port.dev = dev;
	priv->bogo_port.sw = sw;
	priv->bogo_port.flags = 0;
	priv->bogo_port.vlan = vlan;
	priv->bogo_port.forbidden_vlans = NULL;
	list_add_tail(&priv->lh, &sw->vif);
	if ((result = register_netdev(dev))) {
		dbg("vif: error %i registering netdevice %s\n", 
				result, dev->name);
	}
	else {
		dbg("vif: successfully registered netdevice %s\n", dev->name);
	}		
	sw_vdb_add_vlan_default(sw, vlan);
	
	return 0;
}

static void __vif_delif(struct net_device *dev) {
	struct net_switch_vif_priv *priv;

	priv = netdev_priv(dev);
	list_del_rcu(&priv->lh);
	unregister_netdev(dev);
	synchronize_kernel();
	free_netdev(dev);
}

int sw_vif_delif(struct net_switch *sw, int vlan) {
	struct net_device *dev;

	if(vlan < 1 || vlan > 4095)
		return -EINVAL;
	if((dev = sw_vif_find(sw, vlan)) == NULL)
		return -ENOENT;

	__vif_delif(dev);
	return 0;
}

void sw_vif_cleanup(struct net_switch *sw) {
	struct net_switch_vif_priv *priv, *tmp;
	list_for_each_entry_safe(priv, tmp, &sw->vif, lh)
		__vif_delif(priv->bogo_port.dev);
}
