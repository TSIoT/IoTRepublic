#include "BroadcastServer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Utility/IoTSocket.h"
#include "Utility/thread.h"
#include "Utility/IoTUtility.h"

TSThread broadcast_server_thread;
IoTSocket listeningSocket;
#define BroadcastPort 6215
int server_loop(void);

void start_broadcast_server()
{
	Thread_create(&broadcast_server_thread, (TSThreadProc)server_loop, NULL);
	Thread_run(&broadcast_server_thread);
	puts("Broadcast server started");
}

int server_loop(void)
{
    const int buf_size = 500;
	char buf[buf_size];
	const char *sendData="I am here";

#if defined(WIN32)
	WSADATA wsa;
	//printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Broadcast server Failed. Error Code : %d", WSAGetLastError());
		PAUSE;
		return -1;
	}
	//printf("Initialised.\n");
#endif

	listeningSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (listeningSocket <= 0)
	{
		puts("Broadcast server Error: listenForPackets - socket() failed.");
		return 0;
	}

	// bind the port
	struct sockaddr_in sockaddr;
	memset(&sockaddr, 0, sizeof(sockaddr));
	//sockaddr.sin_len = sizeof(sockaddr);
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	//sockaddr.sin_addr.s_addr = inet_addr("192.168.0.255");
	sockaddr.sin_port = htons(BroadcastPort);

	int bBroadcast = 1;
	if (setsockopt(listeningSocket, SOL_SOCKET, SO_BROADCAST, (const char*)&bBroadcast, sizeof(int)) == -1)
	{
		puts("Enable broadcast failed");
	}

	int status = bind(listeningSocket, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
	if (status == -1)
	{
		puts("Broadcast server Error: listenForPackets - bind() failed.");
		CloseIoTSocket(listeningSocket);

		PAUSE
		return 0;
	}

	// receive
	struct sockaddr_in receiveSockaddr;
	#if defined(WIN32)
        int receiveSockaddrLen = sizeof(receiveSockaddr);
	#elif defined(__linux__) || defined(__FreeBSD__)
        socklen_t receiveSockaddrLen = sizeof(receiveSockaddr);
    #endif

    puts("Broad cast server started\n");

	while (1)
	{
		int result = recvfrom(listeningSocket, buf, buf_size, 0, (struct sockaddr *)&receiveSockaddr, &receiveSockaddrLen);
		/*
		in_addr addr = receiveSockaddr.sin_addr;
		unsigned int lastIp = addr.S_un.S_un_b.s_b4;
		
		if (lastIp != 217)
			result = 0;
		*/
		if (result > 0)
		{
			buf[result] = '\0';
			printf("%s\n", buf);
			//ms_sleep(1000);
			
			if (sendto(listeningSocket, sendData, strlen(sendData), 0, (struct sockaddr *)&receiveSockaddr, sizeof(receiveSockaddr))<0)
			{
				puts("Broadcast server Send error");
			}
			else
			{
				puts("Broadcast server Response sended");
			}
		}
		else
		{
			//puts("Nodata");
		}
	}

	CloseIoTSocket(listeningSocket);
	return 0;
}

void stop_broadcast_server()
{
	Thread_stop(&broadcast_server_thread);
	Thread_kill(&broadcast_server_thread);
	CloseIoTSocket(listeningSocket);
}
