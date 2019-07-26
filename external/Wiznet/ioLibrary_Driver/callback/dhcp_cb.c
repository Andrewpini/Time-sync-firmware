
#include <stdint.h>
#include <string.h>
#include "dhcp.h"
#include "wizchip_conf.h"

void w5500_dhcp_assign(void)
{
	wiz_NetInfo prev_gWIZNETINFO;
	wiz_NetInfo gWIZNETINFO;
    
    ctlnetwork(CN_GET_NETINFO, (void*) &prev_gWIZNETINFO);
    memcpy((void *)gWIZNETINFO.mac, (void *)prev_gWIZNETINFO.mac, sizeof(gWIZNETINFO.mac[0]) * 6);
    
	getIPfromDHCP(gWIZNETINFO.ip);
	getGWfromDHCP(gWIZNETINFO.gw);
	getSNfromDHCP(gWIZNETINFO.sn);
	getDNSfromDHCP(gWIZNETINFO.dns);
    gWIZNETINFO.dhcp = NETINFO_DHCP;

	ctlnetwork(CN_SET_NETINFO, (void*) &gWIZNETINFO);
}

void w5500_dhcp_conflict(void)
{
	// TODO
	;
}

