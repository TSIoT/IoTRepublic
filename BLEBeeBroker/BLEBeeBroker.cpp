#include "BLEBeeBroker.h"
#include "../Utility/rs232.h"
#include "../Utility/thread.h"
#include "../IoTService.h"
#include <stdlib.h>


#define MAXSENDBUFFER 100
#define MAXRECVBUFFER 1000
#define MAXDEVICECOUNT 10


#if defined(WIN32)
#define COMPORTNUMBER 28

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

char start_code_ble = 0x02;
char end_code_ble = 0x03;

//thread
//int ble_listen_to_end_device_loop(void);
int ble_listen_to_manager_loop(void);

//public main function
void start_ble_broker();
void stop_ble_broker();

//private main function
int init_BLE_broker();
void unInit_BLE_broker();

//BLE device control
conn_status send_command_to_ble_device(char *cmd, int len);
recv_result recv_command_from_ble_device(char *buf, int *len);

//BLE network maintain
//int send_package_to_BLEBee(IoT_Package package);
int send_package_to_Manager(IoTSocket socket, IoT_Package *package);
//recv_result recv_package_from_BLEBee(IoT_Package *package);
//recv_result recv_package_from_Manager(IoTSocket socket, IoT_Package *package);

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
int find_BLE_device_index_by_IoTIp(IoTIp *ip);
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
	const int recv_buf_size = 10;
	const int max_cmd_buf_size = 50;

	recv_result result, result_from_end_device;
	struct sockaddr_in address;
	IoT_Package recv_package = generate_iot_package();
	//IoT_Package send_package = generate_iot_package();
	IoT_Package send_package;
	IoT_Command recv_cmd;
	IoT_Command send_cmd;
	conn_status status;
	int target_index = -1, len = 0, cmd_len = 0;
	int n=0, offset=0;

	char cmd_id[COMMAND_CHAR_SIZE] = { '\0' };
	char recv_buf[recv_buf_size] = { '\0' };
	char cmd_buf[max_cmd_buf_size] = { '\0' };
	char temp_buf[MAXRECV] = { '\0' };
	char temp_recv[MAXRECV] = { '\0' };

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
		n = recv(ble_broker_socket, temp_recv, MAXRECV, 0);
		if (n > 0)
		{
			charcat(temp_buf, temp_recv, offset, n);
			offset += n;
			do
			{				
				result = getCompletedPackage(temp_buf, &offset, &recv_package);
				if (result == recv_result_COMPLETED)
				{
					//printf("Recv a completed package from manager\n");
					//ble_mac_addr = find_ble_addr_by_iotip(&package.des_ip);
					
					target_index = find_BLE_device_index_by_IoTIp(&recv_package.des_ip);
					if (target_index >= 0)
					{
						//puts("Found device\n");						
						status = conn_ble_device(ble_devices[target_index]->ble_mac_addr);
						if (status == conn_status_succeed)
						{
							//parse the IoT command, only send id to end device
							recv_cmd = generate_iot_command();
							decode_iot_cmd(recv_package.data, &recv_cmd);
							memset(cmd_id, '\0', COMMAND_CHAR_SIZE);
							strcpy(cmd_id, recv_cmd.ID);

							send_command_to_ble_device(cmd_id, strlen(recv_cmd.ID));

							//result_from_end_device = recv_char_from_XBee_at(recv_buf, 4,300 );
							result_from_end_device = recv_command_from_ble_device(recv_buf, &len);
							if (result_from_end_device == recv_result_COMPLETED && len>0)
							{
								puts("Got response:");
								printAllChar(recv_buf, len);

								//Generate a Iot package with IoT_Command
								send_package = generate_iot_package();
								send_cmd = generate_iot_command();
								send_cmd.cmd_type = command_t_ReadResponse;
								memcpy(send_cmd.ID, recv_cmd.ID, COMMAND_CHAR_SIZE);
								memset(send_cmd.Value, '\0', COMMAND_CHAR_SIZE);
								memcpy(send_cmd.Value, recv_buf, len);

								memset(cmd_buf, '\0', max_cmd_buf_size);
								cmd_len = encode_iot_cmd(cmd_buf, &send_cmd);

								create_package(&send_package, recv_package.des_ip.ip, recv_package.sor_ip.ip, cmd_buf, cmd_len);
								send_package_to_Manager(ble_broker_socket, &send_package);

								free_package(&send_package);
								//memset(send_package.data,)
								//send_package_to_Manager_XBee_at(xbee_at_broker_socket,&package);
							}
							else if (result_from_end_device == recv_result_TIMEOUT)
							{
								puts("Recv timeout!");
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

			} while (result == recv_result_COMPLETED);
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

//BLE device control
conn_status send_command_to_ble_device(char *cmd, int len)
{
	conn_status res = conn_status_failed;
	int send_buf_len = len + 2; //add start code and end code
	char *send_buf = (char*)malloc(sizeof(char)*send_buf_len);

	memset(send_buf, '\0', send_buf_len);
	charcat(send_buf, cmd, 1, len);
	//memcpy(send_buf, cmd, len);
	send_buf[0] = start_code_ble; //end of the command
	send_buf[send_buf_len - 1] = end_code_ble; //end of the command
	RS232_SendBuf(COMPORTNUMBER, (unsigned char*)send_buf, len + 2);

	free(send_buf);

	return res;
}

//BLE network maintain
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

recv_result recv_command_from_ble_device(char *buf, int *len)
{
	recv_result res = recv_result_INVALID_PACKAGE;
	char recv_buf[MAXRECVBUFFER] = { '\0' };
	char temp_buf[MAXRECVBUFFER] = { '\0' };
	int recv_len = 0, offset = 0, i = 0, j = 0;
	int start_recv = 0;
	int end_recv = 0;
	unsigned long wait_time = 0;
	unsigned long start_time = get_millis();

	while (1)
	{
		//check if timeout
		wait_time = get_millis() - start_time;
		if (wait_time > ACK_TIMEOUT)
		{
			res = recv_result_TIMEOUT;
			break;
		}

		recv_len = RS232_PollComport(COMPORTNUMBER, (unsigned char*)recv_buf, MAXRECVBUFFER);
		if (recv_len>0)
		{
			charcat(temp_buf, recv_buf, offset, recv_len);
			offset += recv_len;
			if (start_recv == 0 && temp_buf[0] == start_code_ble)
			{
				start_recv = 1;
			}

			if (start_recv == 1)
			{
				for (i = offset - recv_len; i < offset; i++)
				{
					if (temp_buf[i] == end_code_ble)
					{
						//puts("Received finish\n");
						end_recv = i;
						break;
					}
				}
			}
		}

		//found end_code break the loop
		if (end_recv > 0)
		{
			//remove start&end code
			for (i = 1, j = 0; i < end_recv; i++, j++)
			{
				buf[j] = temp_buf[i];
			}
			*len = end_recv - 1;
			res = recv_result_COMPLETED;
			break;
		}
	}
	//clear_recv_buff(300);
	//clear_XBee_at_recv_buff(300);
	return res;
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

	recv_result result;
	conn_status connection_status;


	clear_recv_buff(500);
	//Send iBeacon
	puts("iBeacon sended\n");
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
			
			connection_status = conn_ble_device(ble_mac_addr);
			if (connection_status == conn_status_succeed)
			{
				//check if IoT Device(Send SYN Message)
				memset(receiveBuf, '\0', buf_len);
				//RS232_SendBuf(COMPORTNUMBER, (unsigned char*)syn_msg, strlen(syn_msg));
				send_command_to_ble_device(syn_msg, strlen(syn_msg));
				result = recv_char(receiveBuf, strlen(ack_msg), ack_timeout);

				if (result == recv_result_NOERROR)
				{
					//printAllChar(receiveBuf, 3);
					if (memcmp(ack_msg, receiveBuf, strlen(ack_msg)) == 0)
					{
						printf("Got BLE device Ack\n");
						disconn_ble_device();
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
			else
			{
				printf("Connect to %s failed", ble_mac_addr);
			}

			i = search_res;
		}
	}

}

void ask_a_IoTIP(IoT_Package *package)
{
	IoTSocket s;
	recv_result result;

	int n = 0, offset = 0;
	char temp_buf[MAXRECV] = { '\0' };
	char temp_recv[MAXRECV] = { '\0' };

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

	//recv_package_from_Manager(s, package);
	while (1)
	{
		n = recv(s, temp_recv, MAXRECV, 0);
		if (n > 0)
		{
			charcat(temp_buf, temp_recv, offset, n);
			offset += n;
			//printAllChar(tempBuffer, offset);
			result = getCompletedPackage(temp_buf, &offset, package);
			if (result == recv_result_COMPLETED)
			{
				break;
			}
		}
	}
	CloseIoTSocket(s);
}

int ble_dev_register(char *addr)
{
	//IoTIp ip;
	IoT_Package requestIp = generate_iot_package();
	IoT_Package package = generate_iot_package();
	recv_result result;
	int length = 0;
	unsigned int sum = 0;
	char dev_pack_buf[MAXRECVBUFFER] = { '\0' };

	char *cmde_des = "DES";

	if (!is_ble_device_exists(addr))
	{
		add_new_ble_device(addr);
	}

	//Ask a IoTIP then give to BLE device
	ask_a_IoTIP(&requestIp);
	update_iot_ip(addr, requestIp.des_ip);
	
	do
	{
		conn_ble_device(addr);
		send_command_to_ble_device(cmde_des, strlen(cmde_des));
		result = recv_command_from_ble_device(dev_pack_buf, &length);
		if (result == recv_result_COMPLETED)
		{
			sum = 0;
			sum |= (unsigned char)dev_pack_buf[length - 2];
			sum = sum << 8;
			sum |= (unsigned char)dev_pack_buf[length - 1];
			puts("Received device description\n");

			result = check_sum(dev_pack_buf, length - 2, sum);
			if (result == recv_result_NOERROR)
			{
				puts("Checksum No-error\n");
			}
			else if (result == recv_result_CHECKSUM_ERROR)
			{
				puts("Checksum error!!\n");
			}
		}
		else if (result == recv_result_TIMEOUT)
		{
			puts("Timeout!!\n");
		}
		disconn_ble_device();
	} while (result!= recv_result_NOERROR);
	

	//-2 means no need the check sum for device description
	dev_pack_buf[length - 2] = '\0'; //json file need a '\0' to be a end of file
	create_package(&package, requestIp.des_ip.ip, requestIp.sor_ip.ip, dev_pack_buf, length - 1);
	send_package_to_Manager(ble_broker_socket, &package);

	//create_package(&package, requestIp.sor_ip.ip, requestIp.des_ip.ip, "0", 1);
	//puts("Send IoTIP to BLE device\n");
	//send_package_to_BLEBee(package);

	/*
	result = recv_package_from_BLEBee(&package);
	if (result == recv_result_COMPLETED)
	{
		puts("Got register package from BLE device\n");
		send_package_to_Manager(ble_broker_socket, &package);
	}
	*/


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
	unsigned long timeout = 1000;

	const int cmd_len = 18;
	const int recv_buf_len = 8;
	char cmd_connect[cmd_len] = { "\0" };
	char buf[recv_buf_len] = { "\0" };
	char *cmd_con = "AT+CON";

	char *res_con = "OK+CONNA";
	char *res_con_succeed = "OK+CONN";
	recv_result result;

	//send conect command
	charcat(cmd_connect, cmd_con, 0, strlen(cmd_con));
	charcat(cmd_connect, addr, strlen(cmd_con), MACADDRLEN);


	clear_recv_buff(50);
	//cmd_connect[MACADDRLEN - 1] = '0';
	ms_sleep(50);// AT Command safe time
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
	int try_times = 0;
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
		clear_recv_buff(50);
		ms_sleep(50);// AT Command safe time
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
	int cmd_len = 0, n = 0;

	//IoT_Package package_info = generate_iot_package();
	IoT_Command cmd = generate_iot_command();

	cmd.cmd_type = command_t_Management;
	strcpy(cmd.ID, "Del_Dev");
	strcpy(cmd.Value, ble_devices[deviceIndex]->ip.ip);
	memset(cmdBuffer, '\0', MAXPACKETSIZE);
	cmd_len = encode_iot_cmd(cmdBuffer, &cmd);

	IoT_Package send_package_info = generate_iot_package();
	create_package(&send_package_info, (char*)"0", ServerIP, cmdBuffer, cmd_len);
	n = encode_package(sendBuffer, &send_package_info);

	//idx = package_info.belongSocketIdx;
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

int find_BLE_device_index_by_IoTIp(IoTIp *ip)
{
	int index = -1, i = 0;

	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (ble_devices[i] != NULL &&
			ble_devices[i]->ip.ipLength == ip->ipLength &&
			memcmp(ble_devices[i]->ip.ip, ip->ip, ip->ipLength) == 0)
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
			if (memcmp(ble_devices[i]->ble_mac_addr, addr, MACADDRLEN)==0)
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
