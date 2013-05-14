/*
 *    This file is part of LiSA Switch
 *
 *    LiSA Switch is free software; you can redistribute it
 *    and/or modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version 2 of the
 *    License, or (at your option) any later version.
 *
 *    LiSA Switch is distributed in the hope that it will be
 *    useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with LiSA Switch; if not, write to the Free
 *    Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *    MA  02111-1307  USA
 */

#include "linux.h"

static int add_bridge(struct linux_context *lnx_ctx, int vlan_id)
{
	int ret, bridge_sfd;
	char name[VLAN_NAME_LEN];

	sprintf(name, "vlan%d", vlan_id);

	BRIDGE_SOCK_OPEN(lnx_ctx, bridge_sfd);
	ret = ioctl(bridge_sfd, SIOCBRADDBR, name);
	BRIDGE_SOCK_CLOSE(lnx_ctx, bridge_sfd);

	return 0;
}

static int remove_bridge(struct linux_context *lnx_ctx, int vlan_id)
{
	int ret, bridge_sfd;
	char name[9];

	sprintf(name, "vlan%d", vlan_id);

	BRIDGE_SOCK_OPEN(lnx_ctx, bridge_sfd);
	ret = ioctl(bridge_sfd, SIOCBRDELBR, name);
	BRIDGE_SOCK_CLOSE(lnx_ctx, bridge_sfd);

	return ret;
	return 0;
}

static int add_bridge_if(struct linux_context *lnx_ctx, int vlan_id, int ifindex)
{
	int ret = 0, bridge_sfd;
	struct ifreq ifr;
	char br_name[VLAN_NAME_LEN];
	unsigned long args[4] = { BRCTL_ADD_IF, ifindex, 0, 0 };

	/* Build the name of the bridge */
	sprintf(br_name, "vlan%d", vlan_id);

	/* Add the interface to the bridge */
	strncpy(ifr.ifr_name, br_name, IFNAMSIZ);
	ifr.ifr_data = (char *) args;

	BRIDGE_SOCK_OPEN(lnx_ctx, bridge_sfd);
	ret = ioctl(bridge_sfd, SIOCDEVPRIVATE, &ifr);
	BRIDGE_SOCK_CLOSE(lnx_ctx, bridge_sfd);

	return ret;
}

static int remove_bridge_if(struct linux_context *lnx_ctx, int vlan_id, int ifindex)
{
	int ret = 0, bridge_sfd;
	struct ifreq ifr;
	unsigned long args[4] = { BRCTL_DEL_IF, ifindex, 0, 0 };
	char br_name[VLAN_NAME_LEN];

	/* Build the name of the bridge */
	sprintf(br_name, "vlan%d", vlan_id);

	/* Remove the interface from the bridge */
	strncpy(ifr.ifr_name, br_name, IFNAMSIZ);
	ifr.ifr_data = (char *) args;

	BRIDGE_SOCK_OPEN(lnx_ctx, bridge_sfd);
	ret = ioctl(bridge_sfd, SIOCDEVPRIVATE, &ifr);
	BRIDGE_SOCK_CLOSE(lnx_ctx, bridge_sfd);

	return ret;
}

static int __manage_vif(struct linux_context *lnx_ctx, char *if_name,
	int vlan_id, int cmd)
{
	int ret = 0, vlan_sfd;
	struct vlan_ioctl_args if_request;

	/* Create a new interface in a given VLAN */
	strcpy(if_request.device1, if_name);
	if_request.u.VID = vlan_id;
	if_request.cmd = cmd;

	VLAN_SOCK_OPEN(lnx_ctx, vlan_sfd);
	ret = ioctl(vlan_sfd, SIOCSIFVLAN, &if_request);
	VLAN_SOCK_CLOSE(lnx_ctx, vlan_sfd);

	return ret;
}

static int __add_vif(struct linux_context *lnx_ctx, char *if_name, int vlan_id)
{
	return __manage_vif(lnx_ctx, if_name, vlan_id, ADD_VLAN_CMD);
}

static int __remove_vif(struct linux_context *lnx_ctx, char *if_name)
{
	return __manage_vif(lnx_ctx, if_name, 0, DEL_VLAN_CMD);
}

static int linux_init(struct switch_operations *sw_ops)
{
	return 0;
}

static int if_add(struct switch_operations *sw_ops, int ifindex, int mode)
{
	struct if_data data;
	struct net_switch_device device;
	int ret, if_sfd;
	char if_name[IFNAMSIZE], vif_name[IFNAMSIZE];
	unsigned char vlan_bitmap[512];
	struct linux_context *lnx_ctx = SWLINUX_CTX(sw_ops);
	mm_ptr_t ptr;


	/* Get the name of the interface */
	IF_SOCK_OPEN(lnx_ctx, if_sfd);
	if_get_name(ifindex, if_sfd, if_name);
	IF_SOCK_CLOSE(lnx_ctx, if_sfd);


	/* Add the new interface to the default bridge */
	if (IF_MODE_ACCESS == mode) {
		ret = add_bridge_if(lnx_ctx, LINUX_DEFAULT_VLAN, ifindex);
		if (ret)
			return ret;
		goto create_data;
	}

	/* Create interfaces for each VLAN in the switch */
	mm_lock(mm);

	mm_list_for_each(mm, ptr, mm_ptr(mm, &SHM->vlan_data)) {
		struct vlan_data *v_data =
			mm_addr(mm, mm_list_entry(ptr, struct vlan_data, lh));

		/* Use 8021q to add a new interface */
		ret = __add_vif(lnx_ctx, if_name, v_data->vlan_id);
		if (ret) {
			mm_unlock(mm);
			goto out;
		}
	}

	mm_unlock(mm);

create_data:
	/* Create interface specific data */
	strncpy(device.name, if_name, IFNAMSIZ);
	device.ifindex = ifindex;
	device.type = IF_TYPE_SWITCHED;
	data.device = device;

	init_vlan_bitmap(vlan_bitmap);
	data.bitmap = vlan_bitmap;
	data.mode = mode;
	set_if_data(ifindex, data);

out:
	return ret;
}

static int if_remove(struct switch_operations *sw_ops, int ifindex)
{
	int ret = 0, if_sfd;
	struct linux_context *lnx_ctx = SWLINUX_CTX(sw_ops);
	char if_name[IFNAMSIZE];
	struct if_data data;

	/* Get the name of the interface */
	IF_SOCK_OPEN(lnx_ctx, if_sfd);
	if_get_name(ifindex, if_sfd, if_name);
	IF_SOCK_CLOSE(lnx_ctx, if_sfd);

	/* Get interface data */
	get_if_data(ifindex, &data);

	/* Remove the new interface from the default bridge for ACCES mode */
	if (IF_MODE_ACCESS == data.mode) {
		ret = remove_bridge_if(lnx_ctx, LINUX_DEFAULT_VLAN, ifindex);
		goto remove_data;
	}

	/* TODO Remove all the virtual interfaces for TRUNK mode */

remove_data:
	/* Remove the interface specific data */
	del_if_data(ifindex);
	return ret;
}

static int vlan_add(struct switch_operations *sw_ops, int vlan)
{
	int ret;
	struct linux_context *lnx_ctx = SWLINUX_CTX(sw_ops);

	/* Add a bridge for the new VLAN */
	ret = add_bridge(lnx_ctx, vlan);
	if (ret)
		return ret;

	/* Add new virtual interfaces for all the trunk interfaces */
	LIST_HEAD(net_devices);

	mm_lock(mm);
	mm_list_for_each(mm, ptr, mm_ptr(mm, &SHM->if_data)) {
		struct if_data *if_data =
			mm_addr(mm, mm_list_entry(ptr, struct if_data, lh));

		if (IF_MODE_ACCESS == if_data->mode)
			continue;


	}
	mm_unlock(mm);

	/* Create VLAN specific data */
	set_vlan_data(vlan, net_devices);

	return ret;
}

static int vlan_del(struct switch_operations *sw_ops, int vlan)
{
	int ret;
	struct linux_context *lnx_ctx = SWLINUX_CTX(sw_ops);

	ret = remove_bridge(lnx_ctx, vlan);

	/* TODO Remove VLAN specific data */
	return ret;
}

static int vif_add(struct switch_operations *sw_ops, int vlan, int *ifindex)
{
	int ret, vlan_sfd;
	struct linux_context *lnx_ctx = SWLINUX_CTX(sw_ops);

	VLAN_SOCK_OPEN(lnx_ctx, vlan_sfd);
	ret = 0;
	VLAN_SOCK_CLOSE(lnx_ctx, vlan_sfd);

	return ret;
}

static int vif_del(struct switch_operations *sw_ops, int vlan)
{
	int ret, vlan_sfd;
	struct linux_context *lnx_ctx = SWLINUX_CTX(sw_ops);

	VLAN_SOCK_OPEN(lnx_ctx, vlan_sfd);
	ret = 0;
	VLAN_SOCK_CLOSE(lnx_ctx, vlan_sfd);

	return ret;
}

struct linux_context lnx_ctx = {
	.sw_ops = (struct switch_operations) {
		.backend_init	= linux_init,

		.if_add		= if_add,
		.if_remove	= if_remove,

		.vlan_add	= vlan_add,
		.vlan_del	= vlan_del,
		.vif_add	= vif_add,
		.vif_del	= vif_del
	},
	.vlan_sfd	= -1,
	.bridge_sfd	= -1,
	.if_sfd		= -1
};
