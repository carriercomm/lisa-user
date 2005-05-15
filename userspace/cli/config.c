#include <linux/net_switch.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "command.h"
#include "climain.h"
#include "config_if.h"

#include <errno.h>
extern int errno;
char hostname_default[] = "Switch\0";

static void cmd_end(FILE *out, char **argv) {
	cmd_root = &command_root_main;
	/* FIXME scoatere binding ^Z */
}

static void cmd_hostname(FILE *out, char **argv) {
	char *arg = *argv;
	sethostname(arg, strlen(arg));
}

static void cmd_nohostname(FILE *out, char **argv) {
	sethostname(hostname_default, strlen(hostname_default));
}

static void cmd_int_eth(FILE *out, char **argv) {
	char *arg = *argv;
	struct net_switch_ioctl_arg ioctl_arg;
	char if_name[IFNAMSIZ];
	
	ioctl_arg.cmd = SWCFG_ADDIF;
	ioctl_arg.if_name = if_name_eth(arg);
	do {
		if(!ioctl(sock_fd, SIOCSWCFG, &ioctl_arg))
			break;
		if(errno == ENODEV) {
			fprintf(out, "Command rejected: device %s does not exist\n",
					if_name);
			return;
		}
	} while(0);

	cmd_root = &command_root_config_if_eth;
	strcpy(sel_eth, if_name);
}

static void cmd_no_int_eth(FILE *out, char **argv) {
	char *arg = argv[1]; /* argv[0] is "no" */
	struct net_switch_ioctl_arg ioctl_arg;
	
	ioctl_arg.cmd = SWCFG_DELIF;
	ioctl_arg.if_name = if_name_eth(arg);
	ioctl(sock_fd, SIOCSWCFG, &ioctl_arg);
}

static void cmd_int_vlan(FILE *out, char **argv) {
	char *arg = *argv;
	struct net_switch_ioctl_arg ioctl_arg;
	int vlan = parse_vlan(arg);

	ioctl_arg.cmd = SWCFG_ADDVIF;
	ioctl_arg.vlan = vlan;
	ioctl(sock_fd, SIOCSWCFG, &ioctl_arg);

	sprintf(sel_vlan, "vlan%d", vlan);
	cmd_root = &command_root_config_if_vlan;
}

static void cmd_no_int_vlan(FILE *out, char **argv) {
	fprintf(out, "FIXME\n");
}

static void cmd_macstatic(FILE *out, char **argv) {
	struct net_switch_ioctl_arg ioctl_arg;
	int status;
	unsigned char mac[ETH_ALEN];

	ioctl_arg.cmd = SWCFG_MACSTATIC;
	if(!strcmp(argv[0], "no")) {
		ioctl_arg.cmd = SWCFG_DELMACSTATIC;
		argv++;
	}
	status = parse_mac(argv[0], mac);
	assert(!status);
	ioctl_arg.ext.mac = mac;
	ioctl_arg.vlan = parse_vlan(argv[1]);
	ioctl_arg.if_name = if_name_eth(argv[2]);
	status = ioctl(sock_fd, SIOCSWCFG, &ioctl_arg);
	if(ioctl_arg.cmd == SWCFG_DELMACSTATIC && status == -1) {
		fprintf(out, "MAC address could not be removed\n"
				"Address not found\n\n");
		fflush(out);
	}
}

int valid_host(char *arg, char lookahead) {
	return 1;
}

int valid_no(char *arg, char lookahead) {
	return !strcmp(arg, "no");
}

static sw_command_t sh_no_int_eth[] = {
	{eth_range,				1,	valid_eth,	cmd_no_int_eth,		RUN,		"Ethernet interface number",					NULL},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh_int_eth[] = {
	{eth_range,				1,	valid_eth,	cmd_int_eth,		RUN,		"Ethernet interface number",					NULL},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh_no_int_vlan[] = {
	{vlan_range,			1,	valid_vlan,	cmd_no_int_vlan,	RUN,		"Vlan interface number",						NULL},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh_int_vlan[] = {
	{vlan_range,			1,	valid_vlan,	cmd_int_vlan,		RUN,		"Vlan interface number",						NULL},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh_no_int[] = {
	{"ethernet",			1,	NULL,		NULL,				0,			"Ethernet IEEE 802.3",							sh_no_int_eth},
	{"vlan",				1,	NULL,		NULL,				0,			"LMS Vlans",									sh_no_int_vlan},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

sw_command_t sh_conf_int[] = {
	{"ethernet",			1,	NULL,		NULL,				0,			 "Ethernet IEEE 802.3",							sh_int_eth},
	{"vlan",				1,	NULL,		NULL,				0,			 "LMS Vlans",									sh_int_vlan},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh_macstatic_ifp[] = {
	{eth_range,				1,	valid_eth,	cmd_macstatic,		RUN|PTCNT,	"Ethernet interface number",					NULL},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh_macstatic_if[] = {
	{"ethernet",			1,	NULL,		NULL,				0,			"Ethernet IEEE 802.3",							sh_macstatic_ifp},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh_macstatic_i[] = {
	{"interface",			1,	NULL,		NULL,				0,			"interface",									sh_macstatic_if},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh_macstatic_vp[] = {
	{vlan_range,			1,	valid_vlan,	NULL,				PTCNT,		"VLAN id of mac address table",					sh_macstatic_i},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh_macstatic_v[] = {
	{"vlan",				1,	NULL,		NULL,				0,			"VLAN keyword",									sh_macstatic_vp},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh_macstatic[] = {
	{"H.H.H",				1,	valid_mac,	NULL,				PTCNT,		"48 bit mac address",							sh_macstatic_v},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh_nomac[] = {
	{"aging-time",			1,	NULL,		NULL,				0,			"Set MAC address table entry maximum age",		NULL},
	{"static",				1,	NULL,		NULL,				0,			"static keyword",								sh_macstatic},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh_mac[] = {
	{"aging-time",			1,	NULL,		NULL,				0,			"Set MAC address table entry maximum age",		NULL},
	{"static",				1,	NULL,		NULL,				0,			"static keyword",								sh_macstatic},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh_hostname[] = {
	{"WORD",				1,	valid_host,	cmd_hostname,		RUN|PTCNT,	"This system's network name",					NULL},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh_no[] = {
	{"hostname",			1,	NULL,		cmd_nohostname,		RUN,		"Set system's network name",					NULL},
	{"interface",			1,	NULL,		NULL,				0,			"Select an interface to configure",				sh_no_int},
	{"mac-address-table",	1,	NULL,		NULL,				0,			"Configure the MAC address table",				sh_nomac},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

static sw_command_t sh[] = {
	{"enable",				1,	NULL,		NULL,				0,			"Modify enable password parameters",			NULL},
	{"end",					1,	NULL,		cmd_end,			RUN,		"Exit from configure mode",						NULL},
	{"exit",				1,	NULL,		cmd_end,			RUN,		"Exit from configure mode",						NULL},
	{"hostname",			1,	NULL,		NULL,				0,			"Set system's network name",					sh_hostname},
	{"interface",			1,	NULL,		NULL,				0,			"Select an interface to configure",				sh_conf_int},
	{"mac-address-table",	1,	NULL,		NULL,				0,			"Configure the MAC address table",				sh_mac},
	{"no",					1,	valid_no,	NULL,				PTCNT|CMPL, "Negate a command or set its defaults",			sh_no},
	{NULL,					0,	NULL,		NULL,				0,			NULL,											NULL}
};

sw_command_root_t command_root_config = 				{"%s(config)%c",			sh};
