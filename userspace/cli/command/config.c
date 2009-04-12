#include "swcli.h"
#include "config.h"

#include <regex.h>

extern struct menu_node menu_main;
extern struct menu_node config_main;
extern struct menu_node config_if_main;
extern struct menu_node config_vlan_main;

static char hostname_default[] = "Switch\0";

int cmd_cdp_v2(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	struct cdp_configuration cdp;
	int version = 2;

	if (!strcmp(argv[0], "no"))
		version = 1;

	shared_get_cdp(&cdp);
	if (cdp.enabled) {
		cdp.version = version;
		shared_set_cdp(&cdp);
	}

	return CLI_EX_OK;
}

int cmd_cdp_holdtime(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	struct cdp_configuration cdp;

	shared_get_cdp(&cdp);
	if (cdp.enabled) {
		cdp.holdtime = atoi(argv[2]);
		shared_set_cdp(&cdp);
	}

	return CLI_EX_OK;
}

int cmd_cdp_timer(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	struct cdp_configuration cdp;

	shared_get_cdp(&cdp);
	if (cdp.enabled) {
		cdp.timer = atoi(argv[2]);
		shared_set_cdp(&cdp);
	}

	return CLI_EX_OK;
}

int cmd_cdp_run(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	struct cdp_configuration cdp;
	int enabled = 1;

	if (!strcmp(argv[0], "no"))
		enabled = 0;
	shared_get_cdp(&cdp);
	cdp.enabled = enabled;
	shared_set_cdp(&cdp);

	return CLI_EX_OK;
}

static int _cmd_setenpw(struct cli_context *ctx, int level, char *passwd)
{
	int err;
	if ((err = shared_set_passwd(SHARED_AUTH_ENABLE, level, passwd))) {
		EX_STATUS_REASON(ctx, "%s", strerror(err));
		return CLI_EX_REJECTED;
	}
	return CLI_EX_OK;
}

int cmd_setenpw(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	char *clear_pw = argv[argc - 1],
		 *encrypted_pw;
	const char salt_base[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";
	char salt[] = "$1$....$\0";
	int i, level = SW_MAX_ENABLE;

	/* Clear text password; need to crypt() */
	srandom(time(NULL) & 0xffffffffL);
	for (i = 3; i < 7; i++)
		salt[i] = salt_base[random() % 64];
	encrypted_pw = crypt(clear_pw, salt);

	if (argc > 4)
		level = atoi(argv[3]);

	return _cmd_setenpw(ctx, level, encrypted_pw);
}

int cmd_setenpw_encrypted(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	FILE *out;
	char *encrypted_pw = argv[argc - 1];
	int status, level = SW_MAX_ENABLE;
	regex_t regex;
	regmatch_t result;

	/* Encrypted password; need to check syntax */
	status = regcomp(&regex,  "^\\$1\\$[a-zA-Z0-9\\./]{4}\\$[a-zA-Z0-9\\./]{22}$", REG_EXTENDED);
	assert(!status);
	if(regexec(&regex, *argv, 1, &result, 0)) {
		out = ctx->out_open(ctx, 0);
		fputs("ERROR: The secret you entered is not a valid encrypted secret.\n"
				"To enter an UNENCRYPTED secret, do not specify type 5 encryption.\n"
				"When you properly enter an UNENCRYPTED secret, it will be encrypted.\n\n",
				out);
		return CLI_EX_REJECTED;
	}

	if (argc > 4)
		level = atoi(argv[3]);

	return _cmd_setenpw(ctx, level, encrypted_pw);
}

int cmd_noensecret(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	int level = SW_MAX_ENABLE;

	if (argc == 5)
		level = atoi(argv[4]);

	return _cmd_setenpw(ctx, level, (char *)"\0");
}

int cmd_end(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	ctx->root = &menu_main;
	ctx->node_filter &= PRIV_FILTER(PRIV_MAX);
	return CLI_EX_OK;
}

int cmd_exit(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	ctx->root =& config_main;
	ctx->node_filter &= PRIV_FILTER(PRIV_MAX);
	return CLI_EX_OK;
}

int cmd_hostname(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	char *hostname = argv[1];

	if (!strncmp(argv[0], "no", strlen(argv[0])))
		hostname = hostname_default;

	if (sethostname(hostname, strlen(hostname))) {
		EX_STATUS_REASON(ctx, "%s", strerror(errno));
		return CLI_EX_REJECTED;
	}
	return CLI_EX_OK;
}

static int use_if_ether(struct cli_context *ctx, char *name, int index, int switchport)
{
	int status, sock_fd, ioctl_errno;
	struct swcfgreq swcfgr;
	struct swcli_context *uc = SWCLI_CTX(ctx);
#if 0
	struct cdp_session *cdp;
#endif

	SW_SOCK_OPEN(ctx, sock_fd);

	if (!index)
		index = if_get_index(name, sock_fd);

	if (!index) {
		EX_STATUS_REASON(ctx, "interface %s does not exist", name);
		SW_SOCK_CLOSE(ctx, sock_fd);
		return CLI_EX_REJECTED;
	}

	swcfgr.cmd = SWCFG_ADDIF;
	swcfgr.ifindex = index;
	swcfgr.ext.switchport = switchport;
	status = ioctl(sock_fd, SIOCSWCFG, &swcfgr);
	ioctl_errno = errno;
	SW_SOCK_CLOSE(ctx, sock_fd); /* this can overwrite ioctl errno */

	if (status) {
		switch (ioctl_errno) {
		case ENODEV:
			EX_STATUS_REASON(ctx, "interface %s does not exist", name);
			return CLI_EX_REJECTED;
		case EINVAL:
			EX_STATUS_REASON(ctx, "interface %s is a VIF", name);
			return CLI_EX_REJECTED;
		case EBUSY:
			break;
		default:
			EX_STATUS_REASON_IOCTL(ctx, ioctl_errno);
			return CLI_EX_REJECTED;
		}
	} else {
		/* Enable CDP on this interface */
		/* FIXME FIXME FIXME: we should _try_ to enable cdp
		 * and if it fails return gracefully.
		 * This will stay commented out until we fix it.
		 */
#if 0
		CDP_SESSION_OPEN(ctx, cdp);
		cdp_set_interface(cdp, index, 1);
		CDP_SESSION_CLOSE(ctx, cdp);
#endif
	}

	ctx->node_filter &= PRIV_FILTER(PRIV_MAX);
	ctx->node_filter |= IFF_SWITCHED;
	ctx->root = &config_if_main;
	uc->ifindex = index;
	uc->vlan = -1;

	return CLI_EX_OK;
}

static int use_if_vlan(struct cli_context *ctx, int vlan, int index)
{
	int status, sock_fd, ioctl_errno;
	struct swcfgreq swcfgr;
	struct swcli_context *uc = SWCLI_CTX(ctx);

	SW_SOCK_OPEN(ctx, sock_fd);

	swcfgr.cmd = SWCFG_ADDVIF;
	swcfgr.vlan = vlan;
	status = ioctl(sock_fd, SIOCSWCFG, &swcfgr);
	ioctl_errno = errno;
	SW_SOCK_CLOSE(ctx, sock_fd);

	if (status && ioctl_errno != EEXIST) {
		EX_STATUS_REASON_IOCTL(ctx, ioctl_errno);
		return CLI_EX_REJECTED;
	}

	ctx->node_filter &= PRIV_FILTER(PRIV_MAX);
	ctx->node_filter |= IFF_VIF;
	ctx->root = &config_if_main;
	uc->ifindex = swcfgr.ifindex;
	uc->vlan = vlan;

	return CLI_EX_OK;
}

static int remove_if_ether(struct cli_context *ctx, char *name, int index, int switchport)
{
	int sock_fd;
	struct swcfgreq swcfgr;
#if 0
	struct cdp_session *cdp;
#endif

	SW_SOCK_OPEN(ctx, sock_fd);

	if (!index)
		index = if_get_index(name, sock_fd);

	if (!index) {
		SW_SOCK_CLOSE(ctx, sock_fd);
		return CLI_EX_OK;
	}

	swcfgr.cmd = SWCFG_DELIF;
	swcfgr.ifindex = index;

	/* Disable CDP on this interface */
#if 0
	CDP_SESSION_OPEN(ctx, cdp);
	cdp_set_interface(cdp, index, 0);
	CDP_SESSION_CLOSE(ctx, cdp);
#endif

	ioctl(sock_fd, SIOCSWCFG, &swcfgr);
	SW_SOCK_CLOSE(ctx, sock_fd);

	return CLI_EX_OK;
}

static int remove_if_vlan(struct cli_context *ctx, int vlan, int index)
{
	int status, sock_fd, ioctl_errno;
	struct swcfgreq swcfgr;

	SW_SOCK_OPEN(ctx, sock_fd);

	swcfgr.cmd = SWCFG_DELVIF;
	swcfgr.vlan = vlan;
	status = ioctl(sock_fd, SIOCSWCFG, &swcfgr);
	ioctl_errno = errno;
	SW_SOCK_CLOSE(ctx, sock_fd);

	if (status && ioctl_errno != ENOENT) {
		EX_STATUS_REASON_IOCTL(ctx, ioctl_errno);
		return CLI_EX_REJECTED;
	}

	return CLI_EX_OK;
}

static int cmd_no_int_any(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	int status, n, sock_fd, ioctl_errno;
	struct ifreq ifr;
	struct swcfgreq swcfgr;

	status = if_parse_args(argv + 1, nodev + 1, ifr.ifr_name, &n);

	switch (status) {
	case IF_T_ETHERNET:
		return remove_if_ether(ctx, ifr.ifr_name, 0, 1);
	case IF_T_VLAN:
		return remove_if_vlan(ctx, n, 0);
	}

	if (status == IF_T_ERROR && n == -1) {
		EX_STATUS_REASON(ctx, "invalid interface name");
		return CLI_EX_REJECTED;
	}

	if (status != IF_T_NETDEV) {
		ctx->ex_status.reason = NULL;
		return CLI_EX_REJECTED;
	}

	/* try to guess what netdev name means */

	SW_SOCK_OPEN(ctx, sock_fd);

	if (ioctl(sock_fd, SIOCGIFINDEX, &ifr)) {
		SW_SOCK_CLOSE(ctx, sock_fd);
		return CLI_EX_OK;
	}

	/* ask switch kernel module what it knows about this interface */
	swcfgr.cmd = SWCFG_GETIFTYPE;
	swcfgr.ifindex = ifr.ifr_ifindex;
	status = ioctl(sock_fd, SIOCSWCFG, &swcfgr);
	ioctl_errno = errno;
	SW_SOCK_CLOSE(ctx, sock_fd);

	if (status) {
		EX_STATUS_REASON_IOCTL(ctx, ioctl_errno);
		return CLI_EX_REJECTED;
	}

	if (swcfgr.ext.switchport == SW_IF_VIF)
		return remove_if_vlan(ctx, swcfgr.vlan, ifr.ifr_ifindex);

	return remove_if_ether(ctx, ifr.ifr_name, ifr.ifr_ifindex,
			swcfgr.ext.switchport != SW_IF_ROUTED);
}

int cmd_int_any(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	int status, n, sock_fd, ioctl_errno;
	struct ifreq ifr;
	struct swcfgreq swcfgr;

	if (!strcmp(nodev[0]->name, "no"))
		return cmd_no_int_any(ctx, argc - 1, argv + 1, nodev + 1);

	status = if_parse_args(argv + 1, nodev + 1, ifr.ifr_name, &n);

	switch (status) {
	case IF_T_ETHERNET:
		return use_if_ether(ctx, ifr.ifr_name, 0, 1);
	case IF_T_VLAN:
		return use_if_vlan(ctx, n, 0);
	}

	if (status == IF_T_ERROR && n == -1) {
		EX_STATUS_REASON(ctx, "invalid interface name");
		return CLI_EX_REJECTED;
	}

	if (status != IF_T_NETDEV) {
		ctx->ex_status.reason = NULL;
		return CLI_EX_REJECTED;
	}

	/* try to guess what netdev name means */

	SW_SOCK_OPEN(ctx, sock_fd);

	/* first test if the interface already exists; SIOCGIFINDEX works
	 * on any socket type (see man (7) netdevice for details) */
	do {
		if (!ioctl(sock_fd, SIOCGIFINDEX, &ifr))
			break;
		SW_SOCK_CLOSE(ctx, sock_fd);

		/* test for VIF addition */
		status = if_parse_vlan(ifr.ifr_name);
		if (status >= 0)
			return use_if_vlan(ctx, status, 0);

		/* for now no other type of interfaces can be added */
		EX_STATUS_REASON(ctx, "interface %s does not exist", argv[2]);
		return CLI_EX_REJECTED;
	} while (0);

	/* ask switch kernel module what it knows about this interface */
	swcfgr.cmd = SWCFG_GETIFTYPE;
	swcfgr.ifindex = ifr.ifr_ifindex;
	status = ioctl(sock_fd, SIOCSWCFG, &swcfgr);
	ioctl_errno = errno;
	SW_SOCK_CLOSE(ctx, sock_fd);

	if (status) {
		EX_STATUS_REASON_IOCTL(ctx, ioctl_errno);
		return CLI_EX_REJECTED;
	}

	if (swcfgr.ext.switchport == SW_IF_VIF)
		return use_if_vlan(ctx, swcfgr.vlan, ifr.ifr_ifindex);

	return use_if_ether(ctx, ifr.ifr_name, ifr.ifr_ifindex,
			swcfgr.ext.switchport != SW_IF_ROUTED);
}

int cmd_linevty(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	swcli_dump_args(ctx, argc, argv, nodev);
	return CLI_EX_OK;
}

int cmd_set_aging(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	struct swcfgreq swcfgr;
	int sock_fd, status, nsec = SW_DEFAULT_AGE_TIME;

	if (strncmp(argv[0], "no", strlen(argv[0])))
		nsec = atoi(argv[2]);

	SW_SOCK_OPEN(ctx, sock_fd);
	swcfgr.cmd = SWCFG_SETAGETIME;
	swcfgr.ext.nsec = nsec;
	status = ioctl(sock_fd, SIOCSWCFG, &swcfgr);
	SW_SOCK_CLOSE(ctx, sock_fd);

	if (status) {
		EX_STATUS_REASON_IOCTL(ctx, errno);
		return CLI_EX_REJECTED;
	}

	return CLI_EX_OK;
}

int cmd_macstatic(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	struct swcfgreq swcfgr;
	char ifname[IFNAMSIZ];
	int sock_fd, status, cmd = SWCFG_MACSTATIC;

	if (!strncmp(argv[0], "no", strlen(argv[0]))) {
		cmd = SWCFG_DELMACSTATIC;
		SHIFT_ARG(argc, argv, nodev);
	}

	if ((status = parse_mac(argv[2], swcfgr.ext.mac.addr))) {
		EX_STATUS_REASON(ctx, "Error parsing mac address %s: %s", argv[2], strerror(status));
		return CLI_EX_REJECTED;
	}

	if_name_ethernet(ifname, argv[6]);

	SW_SOCK_OPEN(ctx, sock_fd);
	swcfgr.cmd = cmd;
	swcfgr.vlan = atoi(argv[4]);
	swcfgr.ifindex = if_get_index(ifname, sock_fd);
	status = ioctl(sock_fd, SIOCSWCFG, &swcfgr);
	SW_SOCK_CLOSE(ctx, sock_fd);

	if (status) {
		EX_STATUS_REASON_IOCTL(ctx, errno);
		return CLI_EX_REJECTED;
	}

	return CLI_EX_OK;
}

int cmd_vlan(struct cli_context *ctx, int argc, char **argv, struct menu_node **nodev)
{
	struct swcfgreq swcfgr;
	int status, sock_fd, ioctl_errno;
	struct swcli_context *uc = SWCLI_CTX(ctx);
	FILE *out;

	swcfgr.vlan = atoi(argv[argc - 1]);
	if (strcmp(nodev[0]->name, "no")) {
		/* vlan is added */
		swcfgr.cmd = SWCFG_ADDVLAN;
		default_vlan_name(swcfgr.ext.vlan_desc, swcfgr.vlan);
	} else {
		/* vlan is removed */
		swcfgr.cmd = SWCFG_DELVLAN;
	}

	SW_SOCK_OPEN(ctx, sock_fd);
	status = ioctl(sock_fd, SIOCSWCFG, &swcfgr);
	ioctl_errno = errno;
	SW_SOCK_CLOSE(ctx, sock_fd); /* this can overwrite ioctl errno */

	if (status == -1 && ioctl_errno == EACCES) {
		out = ctx->out_open(ctx, 0);
		fprintf(out, "Default VLAN %d may not be deleted.\n", swcfgr.vlan);
	}

	if (swcfgr.cmd == SWCFG_ADDVLAN && (!status || ioctl_errno == EEXIST)) {
		ctx->root = &config_vlan_main;
		uc->vlan = swcfgr.vlan;
	}
	
	return CLI_EX_OK;
}
