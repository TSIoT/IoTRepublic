#include "BLEBeeBroker.h"
#include "../Utility/rs232.h"
#include "../Utility/thread.h"
#include "../IoTService.h"
#include <stdlib.h>


#define MAXSENDBUFFER 100
#define MAXRECVBUFFER 100
#define MAXDEVICECOUNT 10


#if defined(WIN32)
#define COMPORTNUMBER 20

#elif defined(__linux__) || defined(__FreeBSD__)
#define COMPORTNUMBER 16

#endif

#define ACK_TIMEOUT 1000
#define BLE_INTERVAL 100
//#define RESEND_Time 700

TSThread tcp_to_ble_thread;
IoTSocket ble_broker_socket;
TSMutex ble_lock;
BLE_dev_info *ble_devices[MAXDEVICECOUNT];


//thread
//int ble_listen_to_end_device_loop(void);
int ble_listen_to_manager_loop(void);

//public main function
void start_ble_broker();
void stop_ble_broker();

//private main function
int init_BLE_broker();
void unInit_BLE_broker();

//BLE network maintain
int send_package_to_BLEBee(IoT_Package package);
int send_package_to_Manager(IoTSocket socket, IoT_Package *package);
recv_result recv_package_from_BLEBee(IoT_Package *package);
recv_result recv_package_from_Manager(IoTSocket socket, IoT_Package *package);

recv_result recv_char(char *base_buf, int max_len, unsigned long timeout);
void scan_ble_devices();
void ask_a_IoTIP(IoT_Package *package);
int ble_dev_register(char *addr);
void clear_recv_buff(unsigned long clear_time);
conn_status conn_ble_device(char *addr);
void disconn_ble_device();

//BLE device maintain
int add_new_ble_device(char *addr);
void update_iot_ip(char *addr, IoTIp ip);
char* find_ble_addr_by_iotip(IoTIp *iotip);

int is_ble_device_exists(char *addr);
void clear_all_ble_device();


//********thread***********

/*
int ble_listen_to_end_device_loop(void)
{
	int n = 0;
	//const int comBufSize = 1024;
	char sendBuffer[MAXRECV];

	struct sockaddr_in address;
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_family = AF_INET;
	address.sin_port = htons(SERVERPORT);

	if (connect(ble_broker_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		puts("ble broker connect to manager error");
		return -1;
	}

	while (1)
	{
		;
	}


	return 0;
}
*/
//listen tcp server send to ble device
int ble_listen_to_manager_loop(void)
{
	char *ble_mac_addr;
	recv_result result, result_from_end_device;
	struct sockaddr_in address;
	IoT_Package package = generate_iot_package();
	conn_status status;

	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_family = AF_INET;
	address.sin_port = htons(SERVERPORT);

	if (connect(ble_broker_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		puts("ble broker connect to manager error");
		return -1;
	}

	while (1)
	{
		result = recv_package_from_Manager(ble_broker_socket, &package);
		if (result == recv_result_COMPLETED)
		{
			//printf("Recv a completed package from manager\n");
			ble_mac_addr = find_ble_addr_by_iotip(&package.des_ip);
			if (ble_mac_addr != NULL)
			{
				//puts("Found device\n");
				status = conn_ble_device(ble_mac_addr);
				if (status == conn_status_succeed)
				{
					send_package_to_BLEBee(package);
					result_from_end_device = recv_package_from_BLEBee(&package);
					if (result_from_end_device == recv_result_COMPLETED)
					{
						send_package_to_Manager(ble_broker_socket,&package);
					}					
				}
				else
				{
					printf("Cannnot connecting to BLE Device:%d\n", status);
				}
				disconn_ble_device();

			}
			else
			{
				puts("Cannot found device\n");
			}

		}


	}
	return 0;
}

//public main function 

void start_ble_broker()
{
	init_BLE_broker();
	Mutex_create(&ble_lock);

	Thread_create(&tcp_to_ble_thread, (TSThreadProc)ble_listen_to_manager_loop, NULL);
	Thread_run(&tcp_to_ble_thread);

	scan_ble_devices();
}

void stop_ble_broker()
{

	Thread_stop(&tcp_to_ble_thread);
	Thread_kill(&tcp_to_ble_thread);
	Mutex_free(&ble_lock);

	unInit_BLE_broker();
}

//private main function
int init_BLE_broker()
{
	//int i, n, s, bdrate = 57600;
	int i = 0, bdrate = 9600;

	//Initial device list
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		ble_devices[i] = NULL;
	}

	//Initial comport
	char mode[] = { '8', 'N', '1', 0 };

	if (RS232_OpenComport(COMPORTNUMBER, bdrate, mode))
	{
		printf("BLE Broker cannot open comport %d\n", COMPORTNUMBER);
		return -1;
	}
	else
	{
		printf("COM%d is opened\n", COMPORTNUMBER);
	}
	clear_recv_buff(100);

	//Initial tcp ip
#if defined(WIN32)
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("BLE Broker Failed. Error Code : %d", WSAGetLastError());
		PAUSE;
		return -1;
	}
#endif

	if ((ble_broker_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("BLE Broker Could not create socket\n");
		PAUSE;
		return -1;
	}

	printf("BLE Initialised.\n");
	return 0;
}

void unInit_BLE_broker()
{
	//uninit device list
	clear_all_ble_device();

	//uninit comport	
	RS232_CloseComport(COMPORTNUMBER);
	printf("COM%d is closed\n", COMPORTNUMBER);

	//uninit tcp ip
	CloseIoTSocket(ble_broker_socket);
}

//BLE network maintain
int send_package_to_BLEBee(IoT_Package package)
{
	int dataLength = 0;
	char send_buf[MAXSENDBUFFER] = { '\0' };
	dataLength = encode_package(send_buf, &package);

	Mutex_lock(&ble_lock);
	RS232_SendBuf(COMPORTNUMBER, (unsigned char*)send_buf, dataLength);
	ms_sleep(BLE_INTERVAL);
	Mutex_unlock(&ble_lock);

	/*
	int dataLength = 0;
	char send_buf[MAXSENDBUFFER] = { '\0' };
	char buf[MAXPACKETSIZE] = { '\0' };
	dataLength = encode_package(buf, &package);

	int left_data_length = 0;
	int start_index = 0;
	int sendLength = 0;
	int i = 0, j = 0;

	left_data_length = dataLength;
	while (left_data_length > 0)
	{
		sendLength = left_data_length > MAXSENDBUFFER ? MAXSENDBUFFER : left_data_length;
		for (j = 0, i = start_index; i < start_index + sendLength; j++, i++)
		{
			send_buf[j] = buf[i];
		}

		RS232_SendBuf(COMPORTNUMBER, (unsigned char*)send_buf, sendLength);
		memset(send_buf,'\0', sendLength);
		//Serial1.write(XBeeBuffer, sendLength);

		start_index += sendLength;
		left_data_length -= sendLength;

		if (left_data_length > 0)
		{
			ms_sleep(BLE_INTERVAL);
			//delay(ACKTIME);
		}
	}
	*/
	//RS232_SendBuf(COMPORTNUMBER, (unsigned char*)send_buf, n);
	//puts("Send the ip response to BLE\n");
	return 0;
}

int send_package_to_Manager(IoTSocket socket, IoT_Package *package)
{
	char send_buf[1000] = { '\0' };
	int n = 0;
	n = encode_package(send_buf, package);

	Mutex_lock(&ble_lock);
	//send(ble_broker_socket, send_buf, len, 0);
	send(socket, send_buf, n, 0);
	Mutex_unlock(&ble_lock);
	return 0;
}

recv_result recv_package_from_BLEBee(IoT_Package *package)
{
	unsigned long start_taime = 0, wait_time = 0;
	int offset = 0, n = 0;

	char recv_buf[MAXRECVBUFFER] = { '\0' };
	char buf[MAXPACKETSIZE] = { '\0' };

	recv_result result;

	start_taime = get_millis();
	while (1)
	{
		wait_time = get_millis() - start_taime;

		if (wait_time > ACK_TIMEOUT)
		{
			printf("Timeout!! register package from BLE device\n");
			return recv_result_TIMEOUT;
			//break;
		}

		n = RS232_PollComport(COMPORTNUMBER, (unsigned char*)recv_buf, MAXPACKETSIZE);
		if (n > 0)
		{
			printAllChar(recv_buf, n);
			charcat(buf, recv_buf, offset, n);
			offset += n;
			result = getCompletedPackage(buf, &offset, package);
			if (result == recv_result_COMPLETED)
			{
				//send_package_to_Manager(ble_broker_socket, package);
				//puts("Got register package from BLE device\n");
				return recv_result_COMPLETED;
				//break;
			}
			start_taime = get_millis();
		}
	}
	//return recv_result_NOERROR;
}

recv_result recv_package_from_Manager(IoTSocket socket, IoT_Package *package)
{
	int n = 0, offset = 0;
	char recvBuffer[MAXRECV] = { '\0' };
	char tempBuffer[MAXRECV] = { '\0' };
	recv_result result;

	while (1)
	{
		n = recv(socket, recvBuffer, MAXRECV, 0);
		if (n > 0)
		{
			charcat(tempBuffer, recvBuffer, offset, n);
			offset += n;
			//printAllChar(tempBuffer, offset);
			result = getCompletedPackage(tempBuffer, &offset, package);
			if (result == recv_result_COMPLETED)
			{
				break;
			}
			else
			{
				return result;
			}
		}
	}
	return recv_result_COMPLETED;
}

recv_result recv_char(char *base_buf, int max_len, unsigned long timeout)//Use for AT Command
{
	const int temp_buf_len = 500;
	int len = 0, offset = 0;
	char temp_buf[temp_buf_len] = { "\0" };

	unsigned long start_time = get_millis();
	unsigned long wait_time = 0;

	//printf("Start receive:");
	while (1)
	{
		len = RS232_PollComport(COMPORTNUMBER, (unsigned char*)temp_buf, max_len);
		if (len > 0)
		{
			max_len -= len;

			//printf("%s", temp_buf);
			charcat(base_buf, temp_buf, offset, len);
			memset(temp_buf, '\0', temp_buf_len);

			offset += len;

			if (max_len == 0)
			{
				//printf("\n");
				return recv_result_NOERROR;
			}
		}

		wait_time = get_millis() - start_time;
		if (wait_time >= timeout)
		{
			//printf("\n");
			return recv_result_TIMEOUT;
		}

	}
}

void scan_ble_devices()
{
	const int buf_len = 100;
	int ack_timeout = 600;
	int search_res = 0, len = 0, i = 0, j = 0, offset = 0, total_recv_len = 0;

	char receiveBuf[buf_len] = { '\0' };
	char buffer[1000] = { '\0' };
	char ble_mac_addr[MACADDRLEN] = { '\0' };

	char *cmd_discover = "AT+DISC?";
	char *end_pattern = "OK+DISCE";
	char *mac_pattern = "DIS0:";
	char *syn_msg = "SYN";
	char *ack_msg = "ACK";

	int mac_pattern_len = strlen(mac_pattern);
	int end_pattern_len = strlen(end_pattern);

	recv_result result;
	conn_status connection_status;

	//Send iBeacon 
	RS232_SendBuf(COMPORTNUMBER, (unsigned char*)cmd_discover, strlen(cmd_discover));
	//Read beacon response
	while (1)
	{
		len = RS232_PollComport(COMPORTNUMBER, (unsigned char*)receiveBuf, buf_len);
		if (len > 0)
		{
			//printf("%s", receiveBuf);
			//printAllChar(receiveBuf, buf_len);
			charcat(buffer, receiveBuf, offset, len);
			memset(receiveBuf, '\0', buf_len);
			search_res = string_search(end_pattern, 0, buffer);

			offset += len;

			if (search_res >= 0)//scan finish
			{
				total_recv_len = offset;
				break;
			}
		}
	}

	//Start search BLE IoT Device
	search_res = 0;
	for (i = 0; i < total_recv_len; i++)
	{
		search_res = string_search(mac_pattern, i, buffer);
		if (search_res >= 0)
		{
			search_res += strlen(mac_pattern);
			for (j = 0; j < MACADDRLEN; j++)
			{
				ble_mac_addr[j] = buffer[search_res];
				//printf("%c", buffer[search_res]);
				search_res++;
			}
			clear_recv_buff(100);
			connection_status = conn_ble_device(ble_mac_addr);
			if (connection_status == conn_status_succeed)
			{
				//check if IoT Device(Send SYN Message)
				memset(receiveBuf, '\0', buf_len);
				RS232_SendBuf(COMPORTNUMBER, (unsigned char*)syn_msg, strlen(syn_msg));
				result = recv_char(receiveBuf, strlen(ack_msg), ack_timeout);
				if (result == recv_result_NOERROR)
				{
					printAllChar(receiveBuf, 3);
					if (memcmp(ack_msg, receiveBuf, strlen(ack_msg)) == 0)
					{
						printf("Got BLE device Ack\n");
						ble_dev_register(ble_mac_addr);
					}
					else
					{
						printf("BLE device ACK Error\n");
					}
				}
				else
				{
					printf("BLE device no ACK:%d\n", result);
				}

				//ms_sleep(500);
				disconn_ble_device();
				//add_new_ble_device(ble_mac_addr);
				printf("\n");
			}

			i = search_res;
		}
	}

}

void ask_a_IoTIP(IoT_Package *package)
{
	IoTSocket s;

	//Add a new device into device list	
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Could not create socket(XBee register)\n");
		PAUSE;
		return;
	}

	printf("BLE Register Initialised.\n");

	struct sockaddr_in address;
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_family = AF_INET;
	address.sin_port = htons(SERVERPORT);

	if (connect(s, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		puts("BLE IP request connect error\n");
		return;
	}

	create_package(package, "0", "0", "0", 1);
	send_package_to_Manager(s, package);

	recv_package_from_Manager(s, package);

	CloseIoTSocket(s);
}

int ble_dev_register(char *addr)
{
	//IoTIp ip;
	IoT_Package requestIp = generate_iot_package();
	IoT_Package package = generate_iot_package();
	recv_result result;
	int n = 0, offset = 0;
	unsigned long start_taime = 0, wait_time = 0;

	if (!is_ble_device_exists(addr))
	{
		add_new_ble_device(addr);
	}

	//Ask a IoTIP then give to BLE device
	ask_a_IoTIP(&requestIp);
	update_iot_ip(addr, requestIp.des_ip);
	create_package(&package, requestIp.sor_ip.ip, requestIp.des_ip.ip, "0", 1);
	puts("Send IoTIP to BLE device\n");
	send_package_to_BLEBee(package);

	result = recv_package_from_BLEBee(&package);
	if (result == recv_result_COMPLETED)
	{
		puts("Got register package from BLE device\n");
		send_package_to_Manager(ble_broker_socket, &package);
	}


	free_package(&package);
	free_package(&requestIp);
	puts("End BLE register");

	return 0;
}

void clear_recv_buff(unsigned long clear_time)
{
	RS232_SendBuf(COMPORTNUMBER, (unsigned char*)"AT", 2);

	char temp[100] = { '\0' };
	recv_char(temp, 100, clear_time);

	printf("BLE recv buffer cleared\n");
}

conn_status conn_ble_device(char *addr)
{
	int offset = 0, len = 0, i = 0;

	unsigned long timeout = 1000;
	unsigned long cannot_fout_timeout = 10000;

	const int cmd_len = 18;
	const int recv_buf_len = 8;
	char cmd_connect[cmd_len] = { "\0" };
	char buf[recv_buf_len] = { "\0" };
	char *cmd_con = "AT+CON";

	char *res_con = "OK+CONNA";
	char *res_con_succeed = "OK+CONN";
	char *res_con_failed = "OK+CONNF";

	recv_result result;

	//send conect command
	charcat(cmd_connect, cmd_con, 0, strlen(cmd_con));
	charcat(cmd_connect, addr, strlen(cmd_con), MACADDRLEN);

	//cmd_connect[MACADDRLEN - 1] = '0';
	ms_sleep(500);// AT Command safe time
	RS232_SendBuf(COMPORTNUMBER, (unsigned char*)cmd_connect, cmd_len);

	//receive connect result
	result = recv_char(buf, strlen(res_con), timeout);

	if (result == recv_result_NOERROR && memcmp(buf, res_con, strlen(res_con)) == 0)
	{
		//Start connecting : received CONNA
		printf("Connecting...\n");
		memset(buf, '\0', recv_buf_len);

		result = recv_char(buf, strlen(res_con_succeed), timeout);
		if (result == recv_result_NOERROR && memcmp(buf, res_con_succeed, strlen(res_con_succeed)) == 0)
		{
			printf("Connectd:");
			printAllChar(addr, MACADDRLEN);
			//printf("\n");
			return conn_status_succeed;
		}
		else if (result == recv_result_TIMEOUT)
		{
			printf("Time out!!:");
			//printAllChar(addr, MACADDRLEN);
			printAllChar(buf, strlen(res_con_succeed));
			//printf("\n");
			//result = recv_char(buf, strlen(res_con_failed), cannot_fout_timeout);

			return conn_status_timeout;
		}

	}
	else
	{
		printf("Response error!:%d\n", result);
		printAllChar(addr, MACADDRLEN);
		printf("\n");

		return conn_status_failed;
	}

	return conn_status_failed;

}

void disconn_ble_device()
{
	int offset = 0, len = 0, i = 0, try_times = 0;
	unsigned long timeout = 500;
	const int buf_len = 10;
	const int max_try_times = 3;

	char *discon_cmd = "AT";
	char *res_discon = "OK+LOST";
	char *res_discon_B = "OK";
	char buf[buf_len] = { "\0" };
	recv_result result;

	result = recv_result_TIMEOUT;

	do
	{
		ms_sleep(100);// AT Command safe time
		RS232_SendBuf(COMPORTNUMBER, (unsigned char*)discon_cmd, strlen(discon_cmd));
		recv_char(buf, strlen(res_discon), timeout);
		if (memcmp(buf, res_discon, strlen(res_discon)) == 0 ||
			memcmp(buf, res_discon_B, strlen(res_discon_B)) == 0)
		{
			result = recv_result_NOERROR;
		}
		else
		{
			puts("Disconnect failed:");
			printAllChar(buf, strlen(res_discon));
			clear_recv_buff(timeout);
		}

		try_times++;
		if (try_times > max_try_times)
			break;

	} while (result != recv_result_NOERROR);

	/*
	if (result == recv_result_NOERROR)
	{
		printf("Disconnected\n");
	}
	else
	{
		printf("Disconnect failed\n");
		printAllChar(buf, strlen(res_discon));
	}
	*/
}

//BLE device maintain
int add_new_ble_device(char *addr)
{
	if (is_ble_device_exists(addr))
	{
		printf("BLE device is already exists");
		//printAllChar(addr, MACADDRLEN);
		return -1;
	}
	else
	{
		printf("ADD NEW BLE device:");
		//printAllChar(addr, MACADDRLEN);
	}

	int i = 0;

	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (ble_devices[i] == NULL)
		{
			ble_devices[i] = (BLE_dev_info*)malloc(sizeof(BLE_dev_info));
			memset(ble_devices[i]->ble_mac_addr, '\0', MACADDRLEN);
			memcpy(ble_devices[i]->ble_mac_addr, addr, MACADDRLEN);
			//ble_devices[i]->buf_offset = 0;
			ble_devices[i]->ready_to_receive = 1;
			ble_devices[i]->isAlive = 1;
			//xbee_devices[i]->ip = ip;
			//ble_devices[i]->buffer = (char*)malloc(sizeof(char)*MAXRECV);
			//memset(ble_devices[i]->buffer, '\0', MAXRECV);
			break;
		}
	}
	return i;
}

void remove_ble_device(int deviceIndex)
{
	printf("Server ip is:%s\n", ServerIP);

	char sendBuffer[MAXPACKETSIZE];
	char cmdBuffer[MAXPACKETSIZE];
	int cmd_len = 0, n = 0, idx = 0;

	IoT_Package package_info = generate_iot_package();
	IoT_Command cmd = generate_iot_command();

	cmd.cmd_type = command_t_Management;
	strcpy(cmd.ID, "Del_Dev");
	strcpy(cmd.Value, ble_devices[deviceIndex]->ip.ip);
	memset(cmdBuffer, '\0', MAXPACKETSIZE);
	cmd_len = encode_iot_cmd(cmdBuffer, &cmd);

	IoT_Package send_package_info = generate_iot_package();
	create_package(&send_package_info, (char*)"0", ServerIP, cmdBuffer, cmd_len);
	n = encode_package(sendBuffer, &send_package_info);

	idx = package_info.belongSocketIdx;
	send(ble_broker_socket, sendBuffer, n, 0);
	free_package(&send_package_info);

	//free(ble_devices[deviceIndex]->buffer);
	free(ble_devices[deviceIndex]);


	ble_devices[deviceIndex] = NULL;

}

int find_device_index(char *addr)
{
	int index = -1, i = 0;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (ble_devices[i] != NULL && memcmp(ble_devices[i]->ble_mac_addr, addr, MACADDRLEN) == 0)
		{
			index = i;
			break;
		}
	}
	return index;
}

void update_iot_ip(char *addr, IoTIp ip)
{
	int index = find_device_index(addr);
	if (index >= 0)
	{
		//xbee_devices[index]->ip.ip = (char*)malloc(sizeof(char)*ip.ipLength+1);
		memset(ble_devices[index]->ip.ip, '\0', IP_ADDR_SIZE);
		ble_devices[index]->ip.ipLength = ip.ipLength;
		ble_devices[index]->isAlive = 1;
		memset(ble_devices[index]->ip.ip, '\0', ble_devices[index]->ip.ipLength + 1);
		memcpy(ble_devices[index]->ip.ip, ip.ip, ble_devices[index]->ip.ipLength);
	}
}

IoTIp* find_iotip_by_ble_addr(char *addr)
{
	int i = 0;
	IoTIp* ip = NULL;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (ble_devices[i] != NULL && memcmp(ble_devices[i]->ble_mac_addr, addr, MACADDRLEN) == 0)
		{
			ip = &ble_devices[i]->ip;
		}
	}

	return ip;
}

char* find_ble_addr_by_iotip(IoTIp *iotip)
{
	int i = 0;
	char *addr = NULL;

	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (ble_devices[i] != NULL && strcmp(ble_devices[i]->ip.ip, iotip->ip) == 0)
		{
			addr = ble_devices[i]->ble_mac_addr;
			break;
		}
	}

	return addr;
}

int is_ble_device_exists(char *addr)
{
	int isExists = 0, i = 0;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (ble_devices[i] != NULL)
		{
			//if (xbee_devices[i]->xb_addr.isEqual(*addr))
			if (memcmp(ble_devices[i]->ble_mac_addr, addr, MACADDRLEN))
			{
				isExists = 1;
				break;
			}
		}
	}
	return isExists;
}

void clear_all_ble_device()
{
	int i = 0;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (ble_devices[i] != NULL)
		{
			//free(ble_devices[i]->buffer);
			free(ble_devices[i]);
			ble_devices[i] = NULL;
			//break;
		}
	}

	printf("All device data cleared\n");
}
//*******************************
