#ifndef XBeeBroker_API_H_INCLUDED
#define XBeeBroker_API_H_INCLUDED

#include "../Utility/IoTUtility.h"
#include "XBeeApiMode.h"

#define MACADDRLEN 8

typedef struct
{	
	IoTIp ip;
	XBeeAddress64 xbee_at_mac_addr;
	//char *buffer;
	//int buf_offset;
	int ready_to_receive;
	int isAlive;
}XBee_api_dev_info;


void xbee_api_test();
void start_xbee_api_broker();
void stop_xbee_api_broker();

#endif