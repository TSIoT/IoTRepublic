#include "XBeeBroker_AT.h"
#include "../Utility/rs232.h"
#include "../Utility/thread.h"
#include "../IoTService.h"
#include <stdlib.h>


#define MAXSENDBUFFER 100
#define MAXRECVBUFFER 1000
#define MAXDEVICECOUNT 10


#if defined(WIN32)
#define COMPORTNUMBER 4

#elif defined(__linux__) || defined(__FreeBSD__)
#define COMPORTNUMBER 16

#endif

#define ACK_TIMEOUT 1000
#define XBee_at_INTERVAL 100
//#define RESEND_Time 700

TSThread tcp_to_xbee_at_thread;
IoTSocket xbee_at_broker_socket;
TSMutex xbee_at_lock;
XBee_at_dev_info *xbee_at_devices[MAXDEVICECOUNT];

char start_code = 0x02;
char end_code = 0x03;


//thread
//int ble_listen_to_end_device_loop(void);
int xbee_at_listen_to_manager_loop(void);

//public main function
void start_xbee_at_broker();
void stop_xbee_at_broker();

//private main function
int init_XBee_at_broker();
void unInit_XBee_at_broker();

//XBee device control
conn_status enter_command_mode();
conn_status exit_command_mode();
conn_status send_AT_command(char *cmd);
conn_status send_command_to_end_device(char *cmd, int len);
recv_result recv_command_from_end_device(char *cmd, int *len);
recv_result check_sum(char *buf, int len, unsigned int sended_sum);


//XBee network maintain
//int send_package_to_XBee_at(IoT_Package package);
int send_package_to_Manager_XBee_at(IoTSocket socket, IoT_Package *package);
//recv_result recv_package_from_XBee_at(IoT_Package *package);
//recv_result recv_package_from_Manager_XBee_at(IoTSocket socket, IoT_Package *package);


recv_result recv_char_from_XBee_at(char *base_buf, int max_len, unsigned long timeout);
void scan_XBee_at_devices();
void ask_a_IoTIP_for_XBee_at(IoT_Package *package);
int XBee_at_dev_register(char *addr_dh, char *addr_dl);
void clear_XBee_at_recv_buff(unsigned long clear_time);
conn_status conn_XBee_at_device(char *addr_dh, char *addr_dl);
void disconn_xbee_at_device();

//BLE device maintain
int add_new_XBee_at_device(char *addr_dh, char *addr_dl);
void update_XBee_at_iot_ip(char *addr_dh, char *addr_dl, IoTIp ip);
//char* find_XBee_at_addr_by_iotip(IoTIp *iotip);
int find_XBee_at_device_index_by_IoTIp(IoTIp *ip);

int is_XBee_at_device_exists(char *addr_dh, char *addr_dl);
void clear_all_XBee_at_device();


void xbee_test()
{
	init_XBee_at_broker();
	//enter_command_mode();
	scan_XBee_at_devices();
	//exit_command_mode();
}


//********thread***********

//listen tcp server send to ble device
int xbee_at_listen_to_manager_loop(void)
{
	const int recv_buf_size = 10;
	const int max_cmd_buf_size = 50;
	//char *addr_dh,*addr_dl;

	recv_result result, result_from_end_device;
	struct sockaddr_in address;
	IoT_Package recv_package = generate_iot_package();
	//IoT_Package send_package = generate_iot_package();
	IoT_Package send_package;
	IoT_Command recv_cmd;
	IoT_Command send_cmd;
	conn_status status;
	int target_index = -1,len=0,cmd_len=0;
	int offset = 0,n=0;

	char cmd_id[COMMAND_CHAR_SIZE] = {'\0'};
	char recv_buf[recv_buf_size] = { '\0' };
	char cmd_buf[max_cmd_buf_size] = { '\0' };
	char temp_buf[MAXRECV] = {'\0'};
	char temp_recv[MAXRECV] = { '\0' };


	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_family = AF_INET;
	address.sin_port = htons(SERVERPORT);

	if (connect(xbee_at_broker_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		puts("XBee_AT broker connect to manager error");
		return -1;
	}

	while (1)
	{
		//result = recv_package_from_Manager_XBee_at(xbee_at_broker_socket, &recv_package);
		n = recv(xbee_at_broker_socket, temp_recv, MAXRECV, 0);
		if (n > 0)
		{
			charcat(temp_buf, temp_recv, offset, n);
			offset += n;
			do
			{				
				//printAllChar(tempBuffer, offset);
				result = getCompletedPackage(temp_buf, &offset, &recv_package);
				if (result == recv_result_COMPLETED)
				{
					target_index = -1;
					target_index = find_XBee_at_device_index_by_IoTIp(&recv_package.des_ip);

					if (target_index >= 0)//found target device
					{
						enter_command_mode();
						status = conn_XBee_at_device(xbee_at_devices[target_index]->xbee_at_mac_addr_DH, xbee_at_devices[target_index]->xbee_at_mac_addr_DL);
						exit_command_mode();

						if (status == conn_status_succeed)// device conected
						{
							//parse the IoT command, only send id to end device
							recv_cmd = generate_iot_command();
							decode_iot_cmd(recv_package.data, &recv_cmd);
							memset(cmd_id, '\0', COMMAND_CHAR_SIZE);
							strcpy(cmd_id, recv_cmd.ID);

							send_command_to_end_device(cmd_id, strlen(recv_cmd.ID));

							//result_from_end_device = recv_char_from_XBee_at(recv_buf, 4,300 );
							result_from_end_device = recv_command_from_end_device(recv_buf, &len);
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
								send_package_to_Manager_XBee_at(xbee_at_broker_socket, &send_package);

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
							printf("Cannnot connecting to XBee_AT Device:%d\n", status);
						}
						enter_command_mode();
						disconn_xbee_at_device();
						exit_command_mode();
					}
					else
					{
						puts("Cannot found XBee_AT device\n");
					}
					//free_package(&recv_package);
				}

			} while (result == recv_result_COMPLETED);
		}


		/*
		if (result == recv_result_COMPLETED)
		{
			
		}
		*/
	}

	return 0;
}

//public main function

void start_xbee_at_broker()
{
	init_XBee_at_broker();
	Mutex_create(&xbee_at_lock);

	Thread_create(&tcp_to_xbee_at_thread, (TSThreadProc)xbee_at_listen_to_manager_loop, NULL);
	Thread_run(&tcp_to_xbee_at_thread);

	scan_XBee_at_devices();
}

void stop_xbee_at_broker()
{

	Thread_stop(&tcp_to_xbee_at_thread);
	Thread_kill(&tcp_to_xbee_at_thread);
	Mutex_free(&xbee_at_lock);

	unInit_XBee_at_broker();
}

//private main function
int init_XBee_at_broker()
{
	//int i, n, s, bdrate = 57600;
	int i = 0, bdrate = 9600;

	//Initial device list
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		xbee_at_devices[i] = NULL;
	}

	//Initial comport
	char mode[] = { '8', 'N', '1', 0 };

	if (RS232_OpenComport(COMPORTNUMBER, bdrate, mode))
	{
		printf("XBee AT Broker cannot open comport %d\n", COMPORTNUMBER);
		return -1;
	}
	else
	{
		printf("COM%d is opened\n", COMPORTNUMBER);
	}
	clear_XBee_at_recv_buff(100);

	//Initial tcp ip
#if defined(WIN32)
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("XBee AT Broker Failed. Error Code : %d", WSAGetLastError());
		PAUSE;
		return -1;
	}
#endif

	if ((xbee_at_broker_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("XBee Broker Could not create socket\n");
		PAUSE;
		return -1;
	}

	printf("XBee AT Initialised.\n");
	return 0;
}

void unInit_XBee_at_broker()
{
	//uninit device list
	clear_all_XBee_at_device();

	//uninit comport
	RS232_CloseComport(COMPORTNUMBER);
	printf("COM%d is closed\n", COMPORTNUMBER);

	//uninit tcp ip
	CloseIoTSocket(xbee_at_broker_socket);
}


//XBee device control
conn_status enter_command_mode()
{
	conn_status res = conn_status_failed;
	recv_result temp_res = recv_result_INVALID_PACKAGE;
	const int buf_len = 5;
	unsigned long wait_time = 1100;

	char *enter_cmd = "+++";
	char *succeed = "OK\r";
	char buf[buf_len] = { '\0' };
	//puts("Entering AT Command mode");
	ms_sleep(wait_time); //safe time
	RS232_SendBuf(COMPORTNUMBER, (unsigned char*)enter_cmd, strlen(enter_cmd));
	temp_res = recv_char_from_XBee_at(buf, strlen(succeed), wait_time);
	if (temp_res == recv_result_NOERROR)
	{
		//printAllChar(buf, strlen(succeed));
		res = conn_status_succeed;
	}
	else
	{
		printf("Entering command mode Error:%d\n", temp_res);
	}
	//ms_sleep(wait_time);

	return res;
}

conn_status exit_command_mode()
{
	conn_status res = conn_status_failed;
	recv_result temp_res = recv_result_INVALID_PACKAGE;
	const int buf_len = 5;
	unsigned long wait_time = 1000;

	char *exit_cmd = "ATCN\r";
	char *succeed = "OK\r";
	char buf[buf_len] = { '\0' };

	RS232_SendBuf(COMPORTNUMBER, (unsigned char*)exit_cmd, strlen(exit_cmd));
	temp_res = recv_char_from_XBee_at(buf, strlen(succeed), wait_time);
	if (temp_res == recv_result_NOERROR)
	{
		//puts("Exiting AT Command mode");
		//printAllChar(buf, strlen(succeed));
		res = conn_status_succeed;
	}
	else
	{
		printf("Exiting command mode Error:%d\n", temp_res);
	}

	return res;
}

conn_status send_AT_command(char *cmd)
{
	int cmd_len = 0;
	unsigned long wait_time = 500;
	conn_status res = conn_status_failed;
	recv_result recv_res = recv_result_INVALID_PACKAGE;
	char buf[6] = { '\0' };
	char *succeed = "OK\r";
	cmd_len = strlen(cmd);
	RS232_SendBuf(COMPORTNUMBER, (unsigned char*)cmd, cmd_len);

	recv_res = recv_char_from_XBee_at(buf, 3, wait_time);

	if (recv_res == recv_result_NOERROR && memcmp(buf, succeed, strlen(succeed)) == 0)
	{
		res = conn_status_succeed;
	}
	else if (recv_res == recv_result_TIMEOUT)
	{
		res = conn_status_timeout;
		puts("AT command faild with timeout");
	}
	else
	{
		res = conn_status_failed;
		puts("AT command faild");
	}

	return res;
}

conn_status send_command_to_end_device(char *cmd, int len)
{
	conn_status res = conn_status_failed;
	int send_buf_len = len + 2; //add start code and end code
	char *send_buf = (char*)malloc(sizeof(char)*send_buf_len);

	memset(send_buf, '\0', send_buf_len);
	charcat(send_buf, cmd, 1, len);
	//memcpy(send_buf, cmd, len);
	send_buf[0] = 0x02; //end of the command
	send_buf[send_buf_len - 1] = 0x03; //end of the command
	RS232_SendBuf(COMPORTNUMBER, (unsigned char*)send_buf, len + 2);

	free(send_buf);

	return res;
}

recv_result recv_command_from_end_device(char *buf, int *len)
{
	recv_result res = recv_result_INVALID_PACKAGE;
	char recv_buf[MAXRECVBUFFER] = { '\0' };
	char temp_buf[MAXRECVBUFFER] = { '\0' };
	int recv_len = 0,offset=0,i=0,j=0;
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
			if (start_recv == 0 && temp_buf[0] == start_code)
			{
				start_recv = 1;
			}

			if (start_recv==1)
			{
				for (i = offset - recv_len; i < offset; i++)
				{
					if (temp_buf[i] == end_code)
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
			for (i = 1,j=0; i < end_recv; i++,j++)
			{
				buf[j] = temp_buf[i];
			}
			*len = end_recv-1;
			res = recv_result_COMPLETED;
			break;
		}
	}

	clear_XBee_at_recv_buff(300);
	return res;
}

//XBee network maintain
/*
int send_package_to_XBee_at(IoT_Package package)
{
	int dataLength = 0;
	char send_buf[MAXSENDBUFFER] = { '\0' };
	dataLength = encode_package(send_buf, &package);

	Mutex_lock(&xbee_at_lock);
	RS232_SendBuf(COMPORTNUMBER, (unsigned char*)send_buf, dataLength);
	ms_sleep(XBee_at_INTERVAL);
	Mutex_unlock(&xbee_at_lock);

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
			ms_sleep(XBee_at_INTERVAL);
			//delay(ACKTIME);
		}
	}

	return 0;
}
*/

int send_package_to_Manager_XBee_at(IoTSocket socket, IoT_Package *package)
{
	char send_buf[1000] = { '\0' };
	int n = 0;
	n = encode_package(send_buf, package);

	Mutex_lock(&xbee_at_lock);
	//send(xbee_at_broker_socket, send_buf, len, 0);
	send(socket, send_buf, n, 0);
	Mutex_unlock(&xbee_at_lock);
	return 0;
}

/*
recv_result recv_package_from_XBee_at(IoT_Package *package)
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
				//send_package_to_Manager_XBee_at(xbee_at_broker_socket, package);
				//puts("Got register package from BLE device\n");
				return recv_result_COMPLETED;
				//break;
			}
			start_taime = get_millis();
		}
	}
	//return recv_result_NOERROR;
}
*/

/*
recv_result recv_package_from_Manager_XBee_at(IoTSocket socket, IoT_Package *package)
{
	int n = 0;
	int offset = 0;
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
		}
	}
	return result;
}
*/
recv_result recv_char_from_XBee_at(char *base_buf, int max_len, unsigned long timeout)//Use for AT Command
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

void scan_XBee_at_devices()
{
	const int buf_len = 1000;
	int ack_timeout = 600;
	int search_res = 0, len = 0, i = 0, j = 0, offset = 0, total_recv_len = 0;

	char receiveBuf[buf_len] = { '\0' };
	char buffer[buf_len] = { '\0' };
	char xbee_mac_addr_dh[MACADDRLEN] = { '\0' };
	char xbee_mac_addr_dl [MACADDRLEN] = { '\0' };

	char *cmd_discover = "ATND\r";
	char *end_pattern = "\r\r\r";
	char *mac_pattern = "\r\r";
	char *syn_msg = "SYN";
	char *ack_msg = "ACK";

	//int mac_pattern_len = strlen(mac_pattern);
	//int end_pattern_len = strlen(end_pattern);

	recv_result result;
	conn_status connection_status;

	enter_command_mode();
	//Send beacon
	RS232_SendBuf(COMPORTNUMBER, (unsigned char*)cmd_discover, strlen(cmd_discover));
	//Read beacon response
	while (1)
	{
		len = RS232_PollComport(COMPORTNUMBER, (unsigned char*)receiveBuf, buf_len);
		if (len > 0)
		{
			//printf("%s", receiveBuf);
			printAllChar(receiveBuf, len);

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
	exit_command_mode();
	//printAllChar(buffer, offset);

	//Start search BLE IoT Device
	search_res = 0;
	for (i = 0; i < total_recv_len; i++)
	{
		search_res = string_search(mac_pattern, i, buffer);
		if (search_res >= 0 && search_res<(total_recv_len-strlen(end_pattern)))
		{
			//search_res += strlen(mac_pattern);
			for (j = 0; j < MACADDRLEN; j++)
			{
				xbee_mac_addr_dh[j] = buffer[search_res+7];
				xbee_mac_addr_dl[j] = buffer[search_res + 16];
				//printf("%c", buffer[search_res]);
				search_res++;
			}
			clear_XBee_at_recv_buff(100);
			enter_command_mode();
			connection_status = conn_XBee_at_device(xbee_mac_addr_dh, xbee_mac_addr_dl);
			exit_command_mode();
			if (connection_status == conn_status_succeed)
			{
				//check if IoT Device(Send SYN Message)
				memset(receiveBuf, '\0', buf_len);
				//RS232_SendBuf(COMPORTNUMBER, (unsigned char*)syn_msg, strlen(syn_msg));
				send_command_to_end_device(syn_msg, strlen(syn_msg));

				/*
				//just test device packing function
				char test_buf[10] = {'\0'};
				test_buf[0] = 0x02;
				test_buf[1] = 'A';
				test_buf[2] = 'B';
				test_buf[3] = 'C';
				test_buf[4] = 0x03;
				test_buf[5] = 0x02;
				test_buf[6] = 'D';
				RS232_SendBuf(COMPORTNUMBER, (unsigned char*)test_buf, 7);

				ms_sleep(5000);
				test_buf[0] = 'E';
				test_buf[1] = 'F';
				test_buf[2] = 'G';
				test_buf[3] = 0X03;
				RS232_SendBuf(COMPORTNUMBER, (unsigned char*)test_buf, 4);
				//RS232_SendBuf(COMPORTNUMBER, (unsigned char*)"", strlen(syn_msg));
				//********************************
				*/

				result = recv_char_from_XBee_at(receiveBuf, strlen(ack_msg), ack_timeout);
				if (result == recv_result_NOERROR)
				{
					printAllChar(receiveBuf, 3);
					if (memcmp(ack_msg, receiveBuf, strlen(ack_msg)) == 0)
					{
						printf("Got XBee AT device Ack\n");
						XBee_at_dev_register(xbee_mac_addr_dh, xbee_mac_addr_dl);
					}
					else
					{
						printf("XBee AT device ACK Error\n");
					}
				}
				else
				{
					printf("XBee device no ACK:%d\n", result);
				}

				//ms_sleep(500);
				enter_command_mode();
				disconn_xbee_at_device();
				exit_command_mode();
				//add_new_XBee_at_device(xbee_at_mac_addr);
				printf("\n");
			}


			i = search_res;
		}
	}


}

void ask_a_IoTIP_for_XBee_at(IoT_Package *package)
{
	IoTSocket s;
	recv_result result;
	int n = 0,offset=0;
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
	send_package_to_Manager_XBee_at(s, package);

	while(1)
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
	
	

	//recv_package_from_Manager_XBee_at(s, package);

	CloseIoTSocket(s);
}

int XBee_at_dev_register(char *addr_dh,char *addr_dl)
{
	//IoTIp ip;
	IoT_Package requestIp = generate_iot_package();
	IoT_Package package = generate_iot_package();
	recv_result result;
	int  length = 0;
	unsigned int sum = 0;
	//unsigned long start_taime = 0, wait_time = 0;
	char dev_pack_buf[MAXRECVBUFFER] = {'\0'};

	char *cmde_des = "DES";

	if (!is_XBee_at_device_exists(addr_dh, addr_dl))
	{
		add_new_XBee_at_device(addr_dh, addr_dl);
	}

	//Ask a IoTIP then give to XBee_AT device
	ask_a_IoTIP_for_XBee_at(&requestIp);
	update_XBee_at_iot_ip(addr_dh,addr_dl, requestIp.des_ip);
	
	ms_sleep(100); //target need be slow a little
	send_command_to_end_device(cmde_des, strlen(cmde_des));
	result = recv_command_from_end_device(dev_pack_buf, &length);
	if (result == recv_result_COMPLETED)
	{
		sum = 0;
		sum |= (unsigned char)dev_pack_buf[length - 2];
		sum = sum << 8;
		sum |= (unsigned char)dev_pack_buf[length - 1];
		puts("Received device description\n");

		result=check_sum(dev_pack_buf, length - 2, sum);
		if (result == recv_result_NOERROR)
		{
			puts("Checksum No-error\n");
		}
		else if(result==recv_result_CHECKSUM_ERROR)
		{
			puts("Checksum error!!\n");
		}

	}
	else if (result == recv_result_TIMEOUT)
	{
		puts("Timeout!!\n");
	}

	if (result == recv_result_NOERROR)
	{
		//-2 means no need the check sum for device description
		dev_pack_buf[length - 2] = '\0'; //json file need a '\0' to be a end of file
		create_package(&package, requestIp.des_ip.ip,requestIp.sor_ip.ip, dev_pack_buf, length - 1);
		send_package_to_Manager_XBee_at(xbee_at_broker_socket, &package);
	}

	free_package(&package);
	free_package(&requestIp);
	puts("End XBee_AT register");

	return 0;
}

void clear_XBee_at_recv_buff(unsigned long clear_time)
{
	//RS232_SendBuf(COMPORTNUMBER, (unsigned char*)"AT", 2);

	char temp[100] = { '\0' };
	recv_char_from_XBee_at(temp, 100, clear_time);

	printf("XBee recv buffer cleared\n");
}

conn_status conn_XBee_at_device(char *addr_dh, char *addr_dl)
{
	//int offset = 0, len = 0, i = 0;

	const int cmd_len = 15;
	//const int recv_buf_len = 8;
	char cmd_connect[cmd_len] = { "\0" };
	//char buf[recv_buf_len] = { "\0" };
	char *cmd_con_dh = "ATDH";
	char *cmd_con_dl = "ATDL";
	//char *res_con_succeed = "OK\r";

	//recv_result result;
	conn_status conn_res = conn_status_failed;
	conn_status conn_res_A, conn_res_B;

	//send conect command
	charcat(cmd_connect, cmd_con_dh, 0, strlen(cmd_con_dh));
	charcat(cmd_connect, addr_dh, strlen(cmd_con_dh), MACADDRLEN);
	cmd_connect[strlen(cmd_connect)] = '\r';
	conn_res_A = send_AT_command(cmd_connect);

	memset(cmd_connect,'\0', cmd_len);
	charcat(cmd_connect, cmd_con_dl, 0, strlen(cmd_con_dl));
	charcat(cmd_connect, addr_dl, strlen(cmd_con_dl), MACADDRLEN);
	cmd_connect[strlen(cmd_connect)] = '\r';
	conn_res_B = send_AT_command(cmd_connect);

	if (conn_res_A == conn_status_succeed && conn_res_B == conn_status_succeed)
	{
		conn_res = conn_status_succeed;
	}

	return conn_res;
}

void disconn_xbee_at_device()
{
	send_AT_command("ATDH0\r");
	send_AT_command("ATDL0\r");
	puts("XBee AT device disconnected\n");
}


//XBee device maintain
int add_new_XBee_at_device(char *addr_dh, char *addr_dl)
{
	if (is_XBee_at_device_exists(addr_dh, addr_dl))
	{
		printf("XBee_AT device is already exists");
		//printAllChar(addr, MACADDRLEN);
		return -1;
	}
	else
	{
		printf("ADD NEW XBee_AT device:");
		//printAllChar(addr, MACADDRLEN);
	}

	int i = 0;

	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_at_devices[i] == NULL)
		{
			xbee_at_devices[i] = (XBee_at_dev_info*)malloc(sizeof(XBee_at_dev_info));
			memset(xbee_at_devices[i]->xbee_at_mac_addr_DH, '\0', MACADDRLEN);
			memcpy(xbee_at_devices[i]->xbee_at_mac_addr_DH, addr_dh, MACADDRLEN);
			memset(xbee_at_devices[i]->xbee_at_mac_addr_DL, '\0', MACADDRLEN);
			memcpy(xbee_at_devices[i]->xbee_at_mac_addr_DL, addr_dl, MACADDRLEN);
			//xbee_at_devices[i]->buf_offset = 0;
			xbee_at_devices[i]->ready_to_receive = 1;
			xbee_at_devices[i]->isAlive = 1;
			//xbee_devices[i]->ip = ip;
			//xbee_at_devices[i]->buffer = (char*)malloc(sizeof(char)*MAXRECV);
			//memset(xbee_at_devices[i]->buffer, '\0', MAXRECV);
			break;
		}
	}


	return i;
}

void remove_XBee_at_device(int deviceIndex)
{
	printf("Server ip is:%s\n", ServerIP);

	char sendBuffer[MAXPACKETSIZE];
	char cmdBuffer[MAXPACKETSIZE];
	int cmd_len = 0, n = 0;

	//IoT_Package package_info = generate_iot_package();
	IoT_Command cmd = generate_iot_command();

	cmd.cmd_type = command_t_Management;
	strcpy(cmd.ID, "Del_Dev");
	strcpy(cmd.Value, xbee_at_devices[deviceIndex]->ip.ip);
	memset(cmdBuffer, '\0', MAXPACKETSIZE);
	cmd_len = encode_iot_cmd(cmdBuffer, &cmd);

	IoT_Package send_package_info = generate_iot_package();
	create_package(&send_package_info, (char*)"0", ServerIP, cmdBuffer, cmd_len);
	n = encode_package(sendBuffer, &send_package_info);

	//idx = package_info.belongSocketIdx;
	send(xbee_at_broker_socket, sendBuffer, n, 0);
	free_package(&send_package_info);

	//free(xbee_at_devices[deviceIndex]->buffer);
	free(xbee_at_devices[deviceIndex]);


	xbee_at_devices[deviceIndex] = NULL;

}

int find_XBee_at_device_index(char *addr_dh, char *addr_dl)
{
	int index = -1, i = 0;

	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_at_devices[i] != NULL &&
			memcmp(xbee_at_devices[i]->xbee_at_mac_addr_DH, addr_dh, MACADDRLEN) == 0 &&
			memcmp(xbee_at_devices[i]->xbee_at_mac_addr_DL, addr_dl, MACADDRLEN) == 0)
		{
			index = i;
			break;
		}
	}

	return index;
}

int find_XBee_at_device_index_by_IoTIp(IoTIp *ip)
{
	int index = -1, i = 0;

	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_at_devices[i] != NULL &&
			xbee_at_devices[i]->ip.ipLength == ip->ipLength &&
			memcmp(xbee_at_devices[i]->ip.ip,ip->ip, ip->ipLength)==0)
		{
			index = i;
			break;
		}
	}

	return index;
}

void update_XBee_at_iot_ip(char *addr_dh, char *addr_dl, IoTIp ip)
{
	int index = find_XBee_at_device_index(addr_dh, addr_dl);
	if (index >= 0)
	{
		//xbee_devices[index]->ip.ip = (char*)malloc(sizeof(char)*ip.ipLength+1);
		memset(xbee_at_devices[index]->ip.ip, '\0', IP_ADDR_SIZE);
		xbee_at_devices[index]->ip.ipLength = ip.ipLength;
		xbee_at_devices[index]->isAlive = 1;
		memset(xbee_at_devices[index]->ip.ip, '\0', xbee_at_devices[index]->ip.ipLength + 1);
		memcpy(xbee_at_devices[index]->ip.ip, ip.ip, xbee_at_devices[index]->ip.ipLength);
	}
}

int is_XBee_at_device_exists(char *addr_dh, char *addr_dl)
{
	int isExists = 0, i = 0;

	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_at_devices[i] != NULL)
		{
			//if (xbee_devices[i]->xb_addr.isEqual(*addr))
			if (memcmp(xbee_at_devices[i]->xbee_at_mac_addr_DH, addr_dh, MACADDRLEN)==0 &&
				memcmp(xbee_at_devices[i]->xbee_at_mac_addr_DL, addr_dl, MACADDRLEN) == 0)
			{
				isExists = 1;
				break;
			}
		}
	}

	return isExists;
}

void clear_all_XBee_at_device()
{
	int i = 0;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_at_devices[i] != NULL)
		{
			//free(xbee_at_devices[i]->buffer);
			free(xbee_at_devices[i]);
			xbee_at_devices[i] = NULL;
			//break;
		}
	}

	printf("All device data cleared\n");
}
//*******************************
