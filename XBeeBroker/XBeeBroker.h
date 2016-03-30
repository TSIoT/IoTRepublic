#ifndef XBeeClient_H_INCLUDED
#define XBeeClient_H_INCLUDED
#include "XBeeApiMode.h"
#include "../Utility/IoTUtility.h"

typedef struct
{
	XBeeAddress64 xb_addr;
	IoTIp ip;
	char *buffer;
	int buf_offset;
	int ready_to_receive;
	int isAlive;
}XBee_dev_info;

typedef struct
{
	XBeeAddress64 *xb_addr;
	IoT_Package *package;
}Register_info;

void XBeeTransferTest();
int InitXBeeClient();
void UnInitXBeeClient();
void start_XBee_client();
void stop_XBee_client();
#endif