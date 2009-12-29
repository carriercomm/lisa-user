#include <stdio.h>
#include "rstp_bpdu.h"
#include "vector.h"
#include "rstp.h"
#include <arpa/inet.h>

void
print_vector(struct priority_vector4 *vec)
{
	Dprintf("ROOT_BRIDGE_ID %02x%02x.%02x:%02x:%02x:%02x:%02x:%02x\n",
		vec->root_bridge_id.bridge_priority[0],
		vec->root_bridge_id.bridge_priority[1],
		vec->root_bridge_id.bridge_address[0],
		vec->root_bridge_id.bridge_address[1],
		vec->root_bridge_id.bridge_address[2],
		vec->root_bridge_id.bridge_address[3],
		vec->root_bridge_id.bridge_address[4],
		vec->root_bridge_id.bridge_address[5]);
	Dprintf("ROOT_PATH_COST %d\n", *(unsigned int *)vec->root_path_cost);
	Dprintf("DESIGNATED_BRIDGE_ID %02x%02x.%02x:%02x:%02x:%02x:%02x:%02x\n",
		vec->designated_bridge_id.bridge_priority[0],
		vec->designated_bridge_id.bridge_priority[1],
		vec->designated_bridge_id.bridge_address[0],
		vec->designated_bridge_id.bridge_address[1],
		vec->designated_bridge_id.bridge_address[2],
		vec->designated_bridge_id.bridge_address[3],
		vec->designated_bridge_id.bridge_address[4],
		vec->designated_bridge_id.bridge_address[5]);
	Dprintf("DESIGNATED_PORT_ID %02x%02x\n\n", vec->designated_port_id[0], vec->designated_port_id[1]);
}

void dissect_frame(struct bpdu_t * stpframe)
{
	printf("*** MAC HEADER ***\n");
	printf("Destination MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
			stpframe->mac.dst_mac[0] ,
			stpframe->mac.dst_mac[1] ,
			stpframe->mac.dst_mac[2] ,
			stpframe->mac.dst_mac[3] ,
			stpframe->mac.dst_mac[4] ,
			stpframe->mac.dst_mac[5]);
	printf("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
			stpframe->mac.src_mac[0] ,
			stpframe->mac.src_mac[1] ,
			stpframe->mac.src_mac[2] ,
			stpframe->mac.src_mac[3] ,
			stpframe->mac.src_mac[4] ,
			stpframe->mac.src_mac[5]);

	printf("\n*** ETHERNET HEADER ***\n");
	printf("Length: %d bytes\n"	, ntohs(*(unsigned short *)stpframe->eth.len));
	printf("DSAP: %02x\n"	, stpframe->eth.dsap);
	printf("LSAP: %02x\n"	, stpframe->eth.ssap);
	printf("LLC: %02x\n"	, stpframe->eth.llc);

	printf("\n*** BPDU HEADER ***\n");
	printf("Protocol type: %02x%02x\n"	, stpframe->hdr.protocol[0], stpframe->hdr.protocol[1]);
	printf("Version: %02x\n"		, stpframe->hdr.version);
	printf("BPDU Type: %02x\n"		, stpframe->hdr.bpdu_type);

	printf("\n*** BPDU BODY ***\n");
	printf("Flags: %02x\n", stpframe->body.flags);
	printf("Root ID: %02x%02x.%02x:%02x:%02x:%02x:%02x:%02x\n",
			stpframe->body.root_id[0] ,
			stpframe->body.root_id[1] ,
			stpframe->body.root_id[2] ,
			stpframe->body.root_id[3] ,
			stpframe->body.root_id[4] ,
			stpframe->body.root_id[5] ,
			stpframe->body.root_id[6] ,
			stpframe->body.root_id[7]);
	printf("Root path cost: %d\n", ntohs(*(unsigned int *)stpframe->body.root_path_cost));
	printf("Bridge ID: %02x%02x.%02x:%02x:%02x:%02x:%02x:%02x\n",
			stpframe->body.bridge_id[0] ,
			stpframe->body.bridge_id[1] ,
			stpframe->body.bridge_id[2] ,
			stpframe->body.bridge_id[3] ,
			stpframe->body.bridge_id[4] ,
			stpframe->body.bridge_id[5] ,
			stpframe->body.bridge_id[6] ,
			stpframe->body.bridge_id[7]);
	printf("Port identifier: %02x%02x\n",
			stpframe->body.port_id[0],
			stpframe->body.port_id[1]);
	printf("Message age: %ds\n"	, *(unsigned short *)stpframe->body.message_age);
	printf("Max age: %ds\n"		, *(unsigned short *)stpframe->body.max_age);
	printf("Hello time: %ds\n"	, *(unsigned short *)stpframe->body.hello_time);
	printf("Forward delay: %ds\n"	, *(unsigned short *)stpframe->body.forward_delay);
}

