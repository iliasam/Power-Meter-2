#ifndef _CONFIG_H
#define _CONFIG_H
#include "stdint.h"

#define SOCKET_DHCP			0

//Web server sockets - 1,2,3
#define SOCK_WEB_CNT                    3//SOCKET_CODE - Count, not code!

#define SNTP_SOCKET 			4 //SOCKET_CODE

#define DNS_SOCKET 			5 //SOCKET_CODE

#define RTC_SYNC_PERIOD                 (uint32_t)(1*60) // Период синхронизации RTC, сек

//Hours
#define RTC_TIMEZONE                    (3)

#define SNTP_DOMAIN_NAME                "pool.ntp.org"

#endif //_CONFIG_H