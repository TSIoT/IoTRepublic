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

enum conn_status
{
	conn_status_succeed=0,
	conn_status_timeout=-1,
	conn_status_failed=-2
};


void ble_test();
void start_ble_broker();
void stop_ble_broker();

#endif