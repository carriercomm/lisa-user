/*
 *    This file is part of Linux Multilayer Switch.
 *
 *    Linux Multilayer Switch is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License as published
 *    by the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Linux Multilayer Switch is distributed in the hope that it will be 
 *    useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Linux Multilayer Switch; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <linux/sockios.h>
#include <linux/net_switch.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <assert.h>

#define ETH_ALEN 6

unsigned char *parse_hw_addr(char *mac) {
	unsigned char *buf = calloc(sizeof(unsigned char), ETH_ALEN+1);
	unsigned short a0, a1, a2;
	int i;
	
	if (sscanf(mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
					buf, buf+1, buf+2, buf+3, buf+4, buf+5) == ETH_ALEN) {
		printf("Linux mac format detected: ");		
	}
	else if (sscanf(mac, "%hx.%hx.%hx", &a0, &a1, &a2) == ETH_ALEN/2) {
		printf("Cisco mac format detected: ");
		buf[0] = a0 / 0x100;
		buf[1] = a0 % 0x100;
		buf[2] = a1 / 0x100;
		buf[3] = a1 % 0x100;
		buf[4] = a2 / 0x100;
		buf[5] = a2 % 0x100;
	}
	else {
		printf("Bad mac format makes me very unhappy.\n");
		exit(1);
	}
	for (i=0; i<ETH_ALEN; i++) {
		printf("0x%x ", buf[i]);
	}
	printf("\n");
	return buf;
}

void usage() {
	printf("Usage: swctl [command] [args]\n\n"
		"Command can be any of:\n"
		"  add interface_name\tAdds an interface to switch.\n"
		"  del interface_name\tRemoves an iterface from switch\n"
		"  addvlan vlan_no vlan_name\tAdds a vlan to the vlan database\n"
		"  delvlan vlan_no\tDeletes a vlan from the vlan database\n"
		"  chvlan vlan_no new_vlan_name\tRenames vlan_no to new_vlan_name\n"
		"  addportvlan interface_name vlan_no\tAdds vlan to allowed vlans of interface (trunk mode)\n"
		"  delportvlan interface_name vlan_no\tRemoves vlan from allowed vlans of interface (trunk mode)\n"
		"  settrunk interface_name flag\tPuts interface in trunk (flag=1) or non-trunk (flag=0) mode\n"
		"  setportvlan interface_name vlan_no\tAdd interface in vlan vlan_no (non-trunk mode)\n"
		"  clearportmac interface_name\tClears fdb entries for interface\n"
		"  setagetime ns\tSets aging interval (in seconds) for fdb entries\n"
		"  macstatic interface_name vlan_no hw_addr\tAdds a static mac to interface in vlan vlan_no\n\n"
	);
}

int main(int argc, char **argv) {
	int sock;
	int status;
	struct net_switch_ioctl_arg user_arg;

	if (argc < 2) {
		usage();
		return 0;
	}
	
	sock = socket(PF_PACKET, SOCK_RAW, 0);
	if (sock == -1) {
		perror("socket");
		return 0;
	}	

	if(!strcmp(argv[1], "add")) {
		if (argc < 3) {
			usage();
			return 0;
		}
		user_arg.cmd = SWCFG_ADDIF;
		user_arg.name = argv[2];
		status = ioctl(sock, SIOCSWCFG, &user_arg);
		if(status)
			perror("add failed");
		return 0;
	}

	if(!strcmp(argv[1], "del")) {
		if (argc < 3) {
			usage();
			return 0;
		}
		user_arg.cmd = SWCFG_DELIF;
		user_arg.name = argv[2];
		status = ioctl(sock, SIOCSWCFG, &user_arg);
		if(status)
			perror("del failed");
		return 0;
	}

	if (!strcmp(argv[1], "addvlan")) {
		if (argc < 4) {
			usage();
			return 0;
		}
		user_arg.cmd = SWCFG_ADDVLAN;
		user_arg.vlan = atoi(argv[2]);
		user_arg.name = strdup(argv[3]);
		status = ioctl(sock, SIOCSWCFG, &user_arg);
		if (status)
			perror("addvlan failed");
		return 0;	
	}

	if (!strcmp(argv[1], "delvlan")) {
		if (argc < 3) {
			usage();
			return 0;
		}
		user_arg.cmd = SWCFG_DELVLAN;
		user_arg.vlan = atoi(argv[2]);
		user_arg.name = NULL;
		status = ioctl(sock, SIOCSWCFG, &user_arg);
		if (status)
			perror("delvlan failed");
		return 0;
	}

	if (!strcmp(argv[1], "chvlan")) {
		if (argc < 4) {
			usage();
			return 0;
		}
		user_arg.cmd = SWCFG_RENAMEVLAN;
		user_arg.vlan = atoi(argv[2]);
		user_arg.name = strdup(argv[3]);
		status = ioctl(sock, SIOCSWCFG, &user_arg);
		if (status)
			perror("chvlan failed");
		return 0;	
	}

	if (!strcmp(argv[1], "addportvlan")) {
		if (argc < 4) {
			usage();
			return 0;
		}
		user_arg.cmd = SWCFG_ADDVLANPORT;
		user_arg.name = strdup(argv[2]);
		user_arg.vlan = atoi(argv[3]);
		status = ioctl(sock, SIOCSWCFG, &user_arg);
		if (status)
			perror("addportvlan failed");
		return 0;	
	}

	if (!strcmp(argv[1], "delportvlan")) {
		if (argc < 4) {
			usage();
			return 0;
		}
		user_arg.cmd = SWCFG_DELVLANPORT;
		user_arg.name = strdup(argv[2]);
		user_arg.vlan = atoi(argv[3]);
		status = ioctl(sock, SIOCSWCFG, &user_arg);
		if (status)
			perror("delportvlan failed");
		return 0;
	}
	
	if (!strcmp(argv[1], "settrunk")) {
		if (argc < 4) {
			usage();
			return 0;
		}
		user_arg.cmd = SWCFG_SETTRUNK;
		user_arg.name = strdup(argv[2]);
		user_arg.vlan = atoi(argv[3]);
		status = ioctl(sock, SIOCSWCFG, &user_arg);
		if (status)
			perror("settrunk failed");
		return 0;	
	}

	if (!strcmp(argv[1], "setportvlan")) {
		if (argc < 4) {
			usage();
			return 0;
		}
		user_arg.cmd = SWCFG_SETPORTVLAN;
		user_arg.name = strdup(argv[2]);
		user_arg.vlan = atoi(argv[3]);
		status = ioctl(sock, SIOCSWCFG, &user_arg);
		if (status)
			perror("setportvlan failed");
		return 0;
	}

	if (!strcmp(argv[1], "clearportmac")) {
		if (argc < 3) {
			usage();
			return 0;
		}
		user_arg.cmd = SWCFG_CLEARMACINT;
		user_arg.name = strdup(argv[2]);
		status = ioctl(sock, SIOCSWCFG, &user_arg);
		if (status)
			perror("clearportmac failed");
		return 0;	
	}

	if (!strcmp(argv[1], "setagetime")) {
		if (argc < 3) {
			usage();
			return 0;
		}
		user_arg.cmd = SWCFG_SETAGETIME;
		user_arg.ts.tv_sec = atoi(argv[2]);
		user_arg.ts.tv_nsec = 0;
		status = ioctl(sock, SIOCSWCFG, &user_arg);
		if (status)
			perror("setagetime failed");
		return 0;
	}

	if (!strcmp(argv[1], "macstatic")) {
		if (argc < 5) {
			usage();
			return 0;
		}
		user_arg.cmd = SWCFG_MACSTATIC;
		user_arg.name = strdup(argv[2]);
		user_arg.vlan = atoi(argv[3]);
		user_arg.mac = parse_hw_addr(argv[4]);
		status = ioctl(sock, SIOCSWCFG, &user_arg);
		if (status)
			perror("macstatic failed");
		return 0;
	}

	/* first command line arg invalid ... */
	usage();

	return 0;
}
