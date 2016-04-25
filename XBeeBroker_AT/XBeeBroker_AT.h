#ifndef XBeeBroker_AT_H_INCLUDED
#define XBeeBroker_AT_H_INCLUDED

#include "../Utility/IoTUtility.h"

#define MACADDRLEN 8

typedef struct
{	
	IoTIp ip;
	char xbee_at_mac_addr_DH[MACADDRLEN];
	char xbee_at_mac_addr_DL[MACADDRLEN];
	//char *buffer;
	//int buf_offset;
	int ready_to_receive;
	int isAlive;
}XBee_at_dev_info;




void xbee_test();
void start_xbee_at_broker();
void stop_xbee_at_broker();

#endif