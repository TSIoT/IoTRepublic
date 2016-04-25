#include "XBeeBroker.h"
#include "../Utility/rs232.h"
#include "../Utility/thread.h"
#include "../IoTService.h"
#include <stdlib.h>


#define MAXSENDBUFFER 60
#define MAXDEVICECOUNT 10


#if defined(WIN32)
	#define COMPORTNUMBER 21

#elif defined(__linux__) || defined(__FreeBSD__)
	#define COMPORTNUMBER 16

#endif

#define ACK_TIMEOUT 1000
//#define RESEND_Time 700

TSThread tcp_client_thread,xbee_client_thread;
TSMutex lock;

IoTSocket iot_socket;
XBee_dev_info *xbee_devices[MAXDEVICECOUNT];

IoTIp Server_ip;
IoTIp my_ip;

XBee xbee = XBee();
int xbee_frame_id = 1;

//void XBeeSendData(char *data, int dataLength);
//int XBeeReadData(char *data, int maxDataSize);

//thread
int xbee_register(Register_info *register_info);
void start_IoT_register(Register_info *register_info);
void unlock_xbee(int index);
int wait_device_ready(int deviceId);

//XBee device
int add_new_xbee_device(XBeeAddress64 addr);
void remove_xbee_device(int deviceIndex);
int find_device_index(XBeeAddress64 addr);
void update_iot_ip(XBeeAddress64 addr, IoTIp ip);
XBeeAddress64* find_xbee_addr_by_iotip(IoTIp *iotip);
void clear_all_xbee_device();


int init_XBee_broker()
{
	int i = 0;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		xbee_devices[i] = NULL;
	}

	xbee.begin(COMPORTNUMBER);
#if defined(WIN32)
	WSADATA wsa;
	//printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("XBeeBroker Failed. Error Code : %d", WSAGetLastError());
		PAUSE;
		return -1;
	}
#endif

	if ((iot_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("XBeeBroker Could not create socket\n");
		PAUSE;
		return -1;
	}

	printf("XBEE Initialised.\n");
	return 0;
}

void uninit_XBee_broker()
{
	clear_all_xbee_device();
	CloseIoTSocket(iot_socket);
	RS232_CloseComport(COMPORTNUMBER);
}

//********thread***********

//listen xbee end-device send to tcp server
int listen_to_end_device_loop(void)
{
	int n = 0;
	//const int comBufSize = 1024;
	char sendBuffer[MAXRECV];

	struct sockaddr_in address;
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_family = AF_INET;
	address.sin_port = htons(SERVERPORT);

	if (connect(iot_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		puts("tcp_client connect error");
		return -1;
	}

	/*
	IoT_Package package = generate_iot_package();
	char buffer1[200];
	memset(buffer1, 'a', 200);
	int m = 0, i = 0;
	char *msg = "hello";
	create_package(&package, "0", "0", msg, strlen(msg));
	n = encode_package(buffer1, &package);
	send(iot_socket, (char*)buffer1, n+5, 0);
	*/

	while (1)
	{
		//ms_sleep(1);
		xbee.readPacket();
		if (xbee.getResponse().isAvailable())
		{
			XBeeResponse response = XBeeResponse();
			xbee.getResponse(response);
			uint8_t apiId = response.getApiId();
			if (apiId == ZB_RX_RESPONSE)
			{
				ZBRxResponse rx_response = ZBRxResponse();
				response.getZBRxResponse(rx_response);
				XBeeAddress64 xbee_addr_64= rx_response.getRemoteAddress64();
				//uint16_t xbee_addr16 =rx_response.getRemoteAddress16();
				int index= find_device_index(xbee_addr_64);
				if (index < 0)
				{
					index=add_new_xbee_device(xbee_addr_64);
				}
				//sourceAddress = rx_response.getRemoteAddress64();
				char *rawData = (char*)rx_response.getData();
				//outputData=(char*)rx_response.getData();
				int dataLength = rx_response.getDataLength();
				//printf("From XBee[%d]:", index);
				//printAllChar(rawData, dataLength);
				/*
				printf("Received package length:%d\n", dataLength);
				printAllChar(rawData,dataLength);
				*/
				charcat(xbee_devices[index]->buffer, rawData, xbee_devices[index]->buf_offset, dataLength);
				xbee_devices[index]->buf_offset += dataLength;
				if (xbee_devices[index]->buf_offset >= MAXRECV)
				{
					puts("XBee receive buffer overflow");
					PAUSE;
				}

				//idx += dataLength;
				//printAllChar(comportBuffer, dataLength);
				recv_result result;
				//while (1)
				do
				{
					IoT_Package packageInfo = generate_iot_package();

					result = getCompletedPackage(xbee_devices[index]->buffer,
						&xbee_devices[index]->buf_offset,
						&packageInfo);

					//printAllChar(xbee_devices[index]->buffer, xbee_devices[index]->buf_offset);
					if (result== recv_result_COMPLETED)
					{
						//new XBeeip request
						if (strcmp(packageInfo.des_ip.ip, "0") == 0 && strcmp(packageInfo.sor_ip.ip, "0") == 0)
						{
							//puts("Got a register package, now send to TCP server\n");
							IoT_Package *registerPackage =(IoT_Package*) malloc(sizeof(IoT_Package));
							copy_iot_package(registerPackage,&packageInfo);
							Register_info *register_info=(Register_info*)malloc(sizeof(Register_info));
							register_info->package = registerPackage;
							register_info->xb_addr = (XBeeAddress64*)malloc(sizeof(XBeeAddress64));
							register_info->xb_addr->setLsb(xbee_addr_64.getLsb());
							register_info->xb_addr->setMsb(xbee_addr_64.getMsb());
							start_IoT_register(register_info);
						}
						//XBee device is ready to receive
						else //common communication
						{
							n = encode_package(sendBuffer, &packageInfo);
							send(iot_socket, sendBuffer, n, 0);
							free_package(&packageInfo);
						}
					}
					else if(result == recv_result_CHECKSUM_ERROR)
					{
						break;
					}
				} while (xbee_devices[index]->buf_offset>0 && result == recv_result_COMPLETED);
			}
			else if (apiId ==ZB_TX_STATUS_RESPONSE)
			{

				TxStatusResponse txStatus = TxStatusResponse();
				xbee.getResponse().getZBTxStatusResponse(txStatus);

				// get the delivery status, the fifth byte
				if (txStatus.getStatus() == SUCCESS)
				{
					int index = txStatus.getFrameId()-1; //frame id is start from 1 not 0
					if (xbee_devices[index] != NULL)
					{
						Mutex_lock(&lock);
						xbee_devices[index]->ready_to_receive = 1;
						Mutex_unlock(&lock);
						//printf("Device[%d] is ready\n", index);
					}
				}
				else
				{
					printf("TX Error:0X%X\n", txStatus.getStatus());
				}

			}
		}
		ms_sleep(1);
	}
	return 0;
}

//listen tcp server send to xbee device
int listen_to_manager_loop(void)
{
	int n=0,offset=0,i=0,j=0;
	char buf_from_tcp_server[MAXRECV];
	char tempBuffer[MAXRECV*10];
	char transferBuffer[MAXRECV];
	char sendBuffer[MAXSENDBUFFER];

	while (1)
	{
		n = recv(iot_socket, buf_from_tcp_server, MAXRECV,0);
		if (n > 0)
		{
			charcat(tempBuffer, buf_from_tcp_server,offset,n);
			offset += n;
			/*
			puts("Received from tcp server, now send to XBEE\n");
			printAllChar(comportBuffer,n);
			*/

			IoT_Package packageInfo = generate_iot_package();
			recv_result result;
			//while (offset>0)
			do
			{
				result = getCompletedPackage(tempBuffer, &offset, &packageInfo);
				if (result==recv_result_COMPLETED)
				{
					//XBeeAddress64 targetAddress = XBeeAddress64(0x0013A200, 0x40BE2757);
					XBeeAddress64 *targetAddress = find_xbee_addr_by_iotip(&packageInfo.des_ip);
					if (targetAddress != NULL) //found the xbee address
					{
						int index = find_device_index(*targetAddress);

						n = encode_package(transferBuffer, &packageInfo);
						/*
						puts("Received data from tcp server:\n");
						printAllChar(packageInfo.data, packageInfo.data_length);
						puts("\nTransfer to xbee:\n");
						printAllChar(transferBuffer, n);
						*/
						int left_data_length = n;
						int start_index = 0;
						ZBTxRequest zbTx;
						while (left_data_length > 0)
						{
							int sendLength = left_data_length > MAXSENDBUFFER ? MAXSENDBUFFER : left_data_length;
							for (j = 0, i = start_index; i < start_index + sendLength; j++, i++)
							{
								sendBuffer[j] = transferBuffer[i];
								//printf("%c", sendBuffer[j]);
							}

							zbTx = ZBTxRequest(*targetAddress, (uint8_t*)sendBuffer, sendLength);
							zbTx.setFrameId(index+1);
							//zbTx.setFrameId(xbee_frame_id);
							//xbee_frame_id=xbee_frame_id>255?1: xbee_frame_id+1;
							//puts("Sended\n");

							int is_alive = wait_device_ready(index);
							if (is_alive)
							{
								xbee.send(zbTx);
							}
							else
							{
								remove_xbee_device(index);
								printf("XBee device[%d] no response\n",index);

							}

							//Mutex_unlock(&lock);

							start_index += sendLength;
							left_data_length -= sendLength;

							//ms_sleep(90);
						}
					}
					else
					{
						printf("Cannot foud the XBee addres(IoT Ip:%s\n)", packageInfo.des_ip.ip);
					}

				}
				else
				{
					break;
				}
			} while (offset>0 && result == recv_result_COMPLETED);

			free_package(&packageInfo);
			//XBeeSendData(comportBuffer,n);
		}
	}
	return 0;
}

int wait_device_ready(int deviceId)
{
	if (xbee_devices[deviceId]->isAlive == 0)
		return 0;

	unsigned long start_time = get_millis();
	unsigned long waiting_time = 0;

	while (1)
	{
		Mutex_lock(&lock);
		if (xbee_devices[deviceId]->ready_to_receive == 1)
		{
			Mutex_unlock(&lock);
			break;
		}
		else
		{
			Mutex_unlock(&lock);
			ms_sleep(10);
			waiting_time = get_millis() - start_time;
			//printf("Wiat time:%lu\n", waiting_time);
			if (waiting_time >= ACK_TIMEOUT)
			{
				//puts("ACK time out");
				Mutex_lock(&lock);
				xbee_devices[deviceId]->isAlive = 0;
				xbee_devices[deviceId]->ready_to_receive = 0;
				Mutex_unlock(&lock);

				return 0;
			}
		}
	}

	Mutex_lock(&lock);
	xbee_devices[deviceId]->ready_to_receive = 0;
	Mutex_unlock(&lock);

	return 1;
}

int xbee_register(Register_info *register_info)
{
	IoTSocket s;
	int n = 0, offset = 0;
	char sendBuffer[MAXRECV] = { 0 };
	char tempBuffer[MAXRECV] = { 0 };
	char recvBuffer[MAXRECV] = { 0 };


	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Could not create socket(XBee register)\n");
		PAUSE;
		return -1;
	}

	printf("XBEE Register thread Initialised.\n");

	struct sockaddr_in address;
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_family = AF_INET;
	address.sin_port = htons(SERVERPORT);

	if (connect(s, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		puts("XBEE Register connect error");
		return 1;
	}
	n = encode_package(sendBuffer, register_info->package);
	send(s, sendBuffer, n, 0);
	offset = 0;
	IoT_Package package_info = generate_iot_package();

	while (1)
	{
		n = recv(s, recvBuffer, MAXRECV, 0);
		if (n > 0)
		{
			charcat(tempBuffer, recvBuffer, offset, n);
			offset += n;
			printAllChar(tempBuffer,offset);
			recv_result result = getCompletedPackage(tempBuffer, &offset, &package_info);
			if (result==recv_result_COMPLETED)
			{
				update_iot_ip(register_info->xb_addr[0], package_info.des_ip);
				//int dev_index = find_device_index(register_info->xb_addr);
				//add_new_xbee_device(package_info.des_ip, *register_info->xb_addr);
				break;
			}
		}
	}

	n = encode_package(sendBuffer, &package_info);
	puts("Send the ip response to XBee\n");
	//printAllChar(sendBuffer,n);
	//puts("\n");
	ZBTxRequest zbTx = ZBTxRequest(*register_info->xb_addr, (uint8_t*)sendBuffer, n);
	//RS232_SendBuf(comPortNumber, (unsigned char*)data, strlen(data));
	xbee.send(zbTx);

	free_package(&package_info);
	free(register_info->package);
	free(register_info->xb_addr);
	free(register_info);
	puts("End register thread");
	CloseIoTSocket(s);
	return 0;
}

int unlock_xbee_thread(void *input)
{
	int *index = (int*)input;
	//let the xbee device get its tx status first,so wait a little while here
	ms_sleep(10);
	Mutex_lock(&lock);
	xbee_devices[*index]->ready_to_receive = 1;
	Mutex_unlock(&lock);
	free(index);
	puts("Ready\n");

	return 0;
}


//********XBee device***********

int add_new_xbee_device(XBeeAddress64 addr)
{
	printf("ADD NEW Xbee device:\n XbeeAddr:%X,%X", addr.getMsb(), addr.getLsb());
	int i = 0;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_devices[i] == NULL)
		{
			xbee_devices[i] = (XBee_dev_info*)malloc(sizeof(XBee_dev_info));
			xbee_devices[i]->xb_addr = addr;
			xbee_devices[i]->buf_offset = 0;
			xbee_devices[i]->ready_to_receive = 1;
			xbee_devices[i]->isAlive = 1;
			//xbee_devices[i]->ip = ip;
			xbee_devices[i]->buffer = (char*)malloc(sizeof(char)*MAXRECV);
			memset(xbee_devices[i]->buffer, '\0', MAXRECV);
			break;
		}
	}
	return i;
}

void remove_xbee_device(int deviceIndex)
{
	printf("Server ip is:%s\n", ServerIP);

	char sendBuffer[MAXPACKETSIZE];
	char cmdBuffer[MAXPACKETSIZE];
	int cmd_len = 0,n=0,idx=0;

	IoT_Package package_info = generate_iot_package();
	IoT_Command cmd = generate_iot_command();

	cmd.cmd_type = command_t_Management;
	strcpy(cmd.ID, "Del_Dev");
	strcpy(cmd.Value,xbee_devices[deviceIndex]->ip.ip);
	memset(cmdBuffer, '\0', MAXPACKETSIZE);
	cmd_len = encode_iot_cmd(cmdBuffer, &cmd);

	IoT_Package send_package_info = generate_iot_package();
	create_package(&send_package_info, (char*)"0", ServerIP, cmdBuffer, cmd_len);
	n = encode_package(sendBuffer, &send_package_info);

	idx = package_info.belongSocketIdx;
	send(iot_socket, sendBuffer, n, 0);
	free_package(&send_package_info);

	free(xbee_devices[deviceIndex]->buffer);
	free(xbee_devices[deviceIndex]);
	xbee_devices[deviceIndex] = NULL;

}

int find_device_index(XBeeAddress64 addr)
{
	int index = -1,i=0;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_devices[i]!=NULL && xbee_devices[i]->xb_addr.isEqual(addr))
		{
			index = i;
			break;
		}
	}
	return index;
}

void update_iot_ip(XBeeAddress64 addr, IoTIp ip)
{
	int index = find_device_index(addr);
	if (index >= 0)
	{
		//xbee_devices[index]->ip.ip = (char*)malloc(sizeof(char)*ip.ipLength+1);
		memset(xbee_devices[index]->ip.ip,'\0', IP_ADDR_SIZE);
		xbee_devices[index]->ip.ipLength = ip.ipLength;
		xbee_devices[index]->isAlive = 1;
		memset(xbee_devices[index]->ip.ip, '\0', xbee_devices[index]->ip.ipLength+1);
		memcpy(xbee_devices[index]->ip.ip, ip.ip, xbee_devices[index]->ip.ipLength);
	}
}

IoTIp* find_iotip_by_XBeeAddr(XBeeAddress64 *addr)
{
	int i = 0;
	IoTIp* ip = NULL;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_devices[i] != NULL && xbee_devices[i]->xb_addr.isEqual(*addr))
		{
			ip = &xbee_devices[i]->ip;
		}
	}

	return ip;
}

XBeeAddress64* find_xbee_addr_by_iotip(IoTIp *iotip)
{
	int i = 0;
	XBeeAddress64 *addr = NULL;

	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_devices[i]!=NULL && strcmp(xbee_devices[i]->ip.ip, iotip->ip)==0)
		{
			addr = &xbee_devices[i]->xb_addr;
			break;
		}
	}

	return addr;
}

int is_xbee_device_exists(XBeeAddress64 *addr)
{
	int isExists = 0, i=0;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_devices[i] != NULL)
		{
			if (xbee_devices[i]->xb_addr.isEqual(*addr))
			{
				isExists = 1;
				break;
			}
		}
	}
	return isExists;
}

void clear_all_xbee_device()
{
	int i = 0;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_devices[i] != NULL)
		{
			free(xbee_devices[i]->buffer);
			free(xbee_devices[i]);
			xbee_devices[i] = NULL;
			break;
		}
	}
}
//*******************************

//********Start thread***********
void start_IoT_register(Register_info *register_info)
{
	TSThread register_thread;
	Thread_create(&register_thread, (TSThreadProc)xbee_register, register_info);
	Thread_run(&register_thread);
}

void unlock_xbee(int index)
{
	int *idx = (int*)malloc(sizeof(int));
	*idx = index;
	TSThread tempThread;
	Thread_create(&tempThread, (TSThreadProc)unlock_xbee_thread, idx);
	Thread_run(&tempThread);

}

void start_XBee_broker()
{
	init_XBee_broker();
	Thread_create(&tcp_client_thread, (TSThreadProc)listen_to_end_device_loop, NULL);
	Thread_run(&tcp_client_thread);

	Thread_create(&xbee_client_thread, (TSThreadProc)listen_to_manager_loop, NULL);
	Thread_run(&xbee_client_thread);

	Mutex_create(&lock);
}

void stop_XBee_broker()
{

	Thread_stop(&tcp_client_thread);
	Thread_kill(&tcp_client_thread);

	Thread_stop(&xbee_client_thread);
	Thread_kill(&xbee_client_thread);

	Mutex_free(&lock);	
	
	uninit_XBee_broker();
}

//**************************
/*
void XBeeSendData(char *data, int dataLength)
{
	int idx = 0, send_lne = 0, leftover_len=0;
	char *current_buf = data;

	if (dataLength >= MAXSENDBUFFER)
	{
		while (dataLength>0)
		{
			send_lne = MAXSENDBUFFER - dataLength < 0 ? MAXSENDBUFFER : dataLength;
			leftover_len = dataLength - send_lne;
			char *send_buf = (char*)malloc(sizeof(char)*send_lne);
			char *leftover_buf = (char*)malloc(sizeof(char)*(leftover_len));

			memset(send_buf, '0', send_lne);
			memset(leftover_buf, '0', leftover_len);
			split_char_array(current_buf, send_buf, leftover_buf, send_lne, leftover_len);

			RS232_SendBuf(COMPORTNUMBER, (unsigned char*)send_buf, send_lne);
			printAllChar(send_buf, send_lne);

			dataLength -= send_lne;
			current_buf = leftover_buf;
			free(send_buf);
			ms_sleep(300);
		}
	}
	else
	{
		RS232_SendBuf(COMPORTNUMBER, (unsigned char*)data, dataLength);
	}

}

int XBeeReadData(char *data, int maxDataSize)
{
	int data_number = 0;
	data_number = RS232_PollComport(COMPORTNUMBER, (unsigned char*)data, maxDataSize);
	return data_number;
}
*/
