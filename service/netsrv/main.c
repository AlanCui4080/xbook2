#include <string.h>
#include <stdio.h>
#include <stdio.h>
#include <lwip/netif.h>
#include <lwip/ip.h>
#include <lwip/tcp.h>
#include <lwip/init.h>
#include <netif/etharp.h>
#include <lwip/timers.h>
#include <lwip/udp.h>
#include <lwip/tcpip.h>
#include <pthread.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/srvcall.h>
#include <time.h>
#include <errno.h>
#include <sys/res.h>
#include <unistd.h>
#include <math.h>
#include <netsrv.h>

extern err_t ethernetif_init(struct netif *netif);
extern void ethernetif_input(struct netif *netif);

struct netif rtl8139_netif;

void lwip_init_task(void)
{
    struct ip_addr ipaddr, netmask, gateway;
    lwip_init();

    IP4_ADDR(&gateway, 192,168,0,1);
    IP4_ADDR(&netmask, 255,255,255,0);
    IP4_ADDR(&ipaddr, 192,168,0,105);
    
    netif_add(&rtl8139_netif, &ipaddr, &netmask, &gateway, NULL, ethernetif_init, ethernet_input);
    netif_set_default(&rtl8139_netif);
    netif_set_up(&rtl8139_netif);
}

/*
Net Service:
+------------------------+
| 网络服务接口            |
| 网络服务环境            |
\                        /
+------------------------+
| LWIP                   |
\                        /
+------------------------+
| 驱动                   |
+------------------------+
*/


int main(int argc, char *argv[])
{
    //printf("%s: started.\n", SRV_NAME);
    //init LwIP
	lwip_init_task();
    
    // setup echo server
 	//echo_client_init();
    //http_server_init();
	
    //ping_init();
	
	while(1)
	{
        /* 检测输入，并进行超时检测 */
        ethernetif_input(&rtl8139_netif);
        sys_check_timeouts();
		
		//todo: add your own user code here
	}
    return 0;
}
