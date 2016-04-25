#ifndef BLEBeeBroker_H_INCLUDED
#define BLEBeeBroker_H_INCLUDED

#include "../Utility/IoTUtility.h"

#define MACADDRLEN 12

typedef struct
{	
	IoTIp ip;
	char ble_mac_addr[MACADDRLEN];
	//char *buffer;
	//int buf_offset;
	int ready_to_receive;
	int isAlive;
}BLE_dev_info;



void xbee_test();
void start_ble_broker();
void stop_ble_broker();

#endif