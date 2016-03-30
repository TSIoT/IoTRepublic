#ifndef IoTSocket_H_INCLUDED
#define IoTSocket_H_INCLUDED

#include "thread.h"

#if defined(WIN32)
	#pragma comment(lib, "ws2_32.lib") //Winsock Library

	typedef SOCKET IoTSocket;
	typedef TIMEVAL TimeSpan;
	#define CloseIoTSocket closesocket
	#define PAUSE system("Pause");

#elif defined(__linux__) || defined(__FreeBSD__)
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <sys/time.h>
    #include <unistd.h>
	typedef int IoTSocket;
	typedef struct timeval TimeSpan;
	#define CloseIoTSocket close
	#define PAUSE printf("Press Enter any key to continue..."); fgetc(stdin);

#endif

#endif
