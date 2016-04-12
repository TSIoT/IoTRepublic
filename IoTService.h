#ifndef IoTService_H_INCLUDED
#define IoTService_H_INCLUDED
#include "Utility/IoTSocket.h"
#include "Utility/IoTUtility.h"

#define MAXRECV 2000 //receive buffer max size
#define MAXCLIENTS 30 //max client size
#define SERVERPORT 6210 //Server portnumber

extern char *ServerIP;

typedef struct
{
	IoTIp ip;
	int socket_index;
	char *funGroup;
	char *devID;
	char *devDesc;
	//void *nextDevice;
	int proxied;
} IoT_Device_Info;

typedef struct
{
	IoTSocket listener;
	struct sockaddr_in address;
	TSThread serverThread;
} Server_Object;

typedef struct
{
	char *tempBuffer;
	//char tempBuffer[MAXRECV];
	int receiveCount;
} Package_Buffer;

#endif

