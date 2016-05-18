#include "XBeeBroker_API.h"
#include "../Utility/rs232.h"
#include "../Utility/thread.h"
#include "../IoTService.h"

#include <stdlib.h>


#define MAXSENDBUFFER 100
#define MAXRECVBUFFER 1000
#define MAXDEVICECOUNT 10


#if defined(WIN32)
#define COM_XBee_API 20

#elif defined(__linux__) || defined(__FreeBSD__)
#define COM_XBee_API 16

#endif

#define ACK_TIMEOUT 1000
#define XBee_at_INTERVAL 100
//#define RESEND_Time 700
XBee xbee_obj = XBee();

TSThread tcp_to_xbee_api_thread;
IoTSocket xbee_api_broker_socket;
TSMutex xbee_api_lock;
XBee_api_dev_info *xbee_api_devices[MAXDEVICECOUNT];

char start_code_api = 0x02;
char end_code_api = 0x03;



//thread
//int ble_listen_to_end_device_loop(void);
int xbee_api_listen_to_manager_loop(void);

//public main function
void start_xbee_api_broker();
void stop_xbee_api_broker();

//private main function
int init_XBee_api_broker();
void unInit_XBee_api_broker();

//XBee device network maintain
conn_status send_AT_command_api_mode(char *cmd, char *value, int value_len);
conn_status recv_response_api_mode(char *value, int *len, int expect_response);

conn_status send_command_to_api_end_device(XBeeAddress64 xbee_mac_addrs, char *cmd, int len);
recv_result recv_command_from_api_end_device(char *cmd, int *len);
recv_result check_sum(char *buf, int len, unsigned int sended_sum);
int send_package_to_Manager_XBee_api(IoTSocket socket, IoT_Package *package);
recv_result recv_raw_char_from_XBee_api(char *base_buf, int max_len, unsigned long timeout);
void scan_XBee_api_devices();
void ask_a_IoTIP_for_XBee_api(IoT_Package *package);
int XBee_api_dev_register(XBeeAddress64 addr);
void clear_XBee_api_recv_buff(unsigned long clear_time);


//XBee device maintain
int add_new_XBee_api_device(XBeeAddress64 addr);
void update_XBee_api_iot_ip(XBeeAddress64 addr, IoTIp ip);
//char* find_XBee_at_addr_by_iotip(IoTIp *iotip);
int find_XBee_api_device_index_by_IoTIp(IoTIp *ip);

int is_XBee_api_device_exists(XBeeAddress64 addr);
void clear_all_XBee_api_device();


void xbee_api_test()
{	
	init_XBee_api_broker();
	//send_AT_command_api_mode("ID");
	scan_XBee_api_devices();
	unInit_XBee_api_broker();	
}


//********thread***********

//listen tcp server send to ble device
int xbee_api_listen_to_manager_loop(void)
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

	if (connect(xbee_api_broker_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		puts("XBee_AT broker connect to manager error");
		return -1;
	}

	while (1)
	{
		//result = recv_package_from_Manager_XBee_at(xbee_at_broker_socket, &recv_package);
		n = recv(xbee_api_broker_socket, temp_recv, MAXRECV, 0);
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
					target_index = find_XBee_api_device_index_by_IoTIp(&recv_package.des_ip);

					if (target_index >= 0)//found target device
					{						

						//parse the IoT command, only send id to end device
						recv_cmd = generate_iot_command();
						decode_iot_cmd(recv_package.data, &recv_cmd);
						memset(cmd_id, '\0', COMMAND_CHAR_SIZE);
						strcpy(cmd_id, recv_cmd.ID);

						send_command_to_api_end_device(xbee_api_devices[target_index]->xbee_at_mac_addr, cmd_id, strlen(recv_cmd.ID));
						recv_response_api_mode(NULL, &len, ZB_TX_STATUS_RESPONSE);
						
						if (recv_cmd.cmd_type == command_t_ReadRequest)
						{
							result_from_end_device = recv_command_from_api_end_device(recv_buf, &len);
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
								send_package_to_Manager_XBee_api(xbee_api_broker_socket, &send_package);

								free_package(&send_package);
							}
							else if (result_from_end_device == recv_result_TIMEOUT)
							{
								puts("Recv timeout!");
							}
						}							
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

void start_xbee_api_broker()
{
	init_XBee_api_broker();
	Mutex_create(&xbee_api_lock);

	Thread_create(&tcp_to_xbee_api_thread, (TSThreadProc)xbee_api_listen_to_manager_loop, NULL);
	Thread_run(&tcp_to_xbee_api_thread);

	scan_XBee_api_devices();
}

void stop_xbee_api_broker()
{

	Thread_stop(&tcp_to_xbee_api_thread);
	Thread_kill(&tcp_to_xbee_api_thread);
	Mutex_free(&xbee_api_lock);

	unInit_XBee_api_broker();
}

//private main function
int init_XBee_api_broker()
{
	//int i, n, s, bdrate = 57600;
	int i = 0;

	//Initial device list
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		xbee_api_devices[i] = NULL;
	}

	//Initial comport

	xbee_obj.begin(COM_XBee_API);

	//Initial tcp ip
#if defined(WIN32)
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("XBee API Broker Failed. Error Code : %d", WSAGetLastError());
		PAUSE;
		return -1;
	}
#endif

	if ((xbee_api_broker_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("XBee API Broker Could not create socket\n");
		PAUSE;
		return -1;
	}

	printf("XBee API Initialised.\n");
	return 0;
}

void unInit_XBee_api_broker()
{
	//uninit device list
	clear_all_XBee_api_device();

	//uninit comport
	xbee_obj.end(COM_XBee_API);

	//uninit tcp ip
	CloseIoTSocket(xbee_api_broker_socket);
}

//XBee device control
conn_status send_AT_command_api_mode(char *cmd,char *value,int value_len)
{
	conn_status res = conn_status_failed;
	AtCommandRequest request = AtCommandRequest();	
	const int cmd_len = 2;
	//char recv_buf[100] = { 0 };
	//int recv_len = 0;

	uint8_t ac_cmd[cmd_len] = { 0 };
	memcpy(ac_cmd,cmd, cmd_len);

	request.setCommand(ac_cmd);
	if (value_len>0)
	{
		request.setCommandValue((uint8_t*)value);
		request.setCommandValueLength(value_len);
	}

	xbee_obj.send(request);


	//recv_api_response(recv_buf, &recv_len,AT_COMMAND_RESPONSE);
	
	return res;
}

conn_status recv_response_api_mode(char *value,int *len, int expect_response)
{
	conn_status res = conn_status_failed;	
	AtCommandResponse response = AtCommandResponse();
	ZBRxResponse rx_response = ZBRxResponse();
	TxStatusResponse txStatus = TxStatusResponse();
	
	*len = 0;
	
	if (xbee_obj.readPacket(ACK_TIMEOUT))
	{
		// got a response!
		// should be an AT command response
		if (expect_response== AT_COMMAND_RESPONSE && xbee_obj.getResponse().getApiId() == AT_COMMAND_RESPONSE)
		{
			xbee_obj.getResponse().getAtCommandResponse(response);
			if (response.isOk()) 
			{
				res = conn_status_succeed;
				//printf("Command[%c%c]was successful!\n", response.getCommand()[0], response.getCommand()[1]);

				if (response.getValueLength() > 0) 
				{
					*len = response.getValueLength();
					for (int i = 0; i < response.getValueLength(); i++) 
					{
						
						value[i] = response.getValue()[i];
						//printf("%X ", value[i]);
					}
				}
				printf("\n");
			}
			else 
			{
				printf("AT Command return error code:%X\n", response.getStatus());
				
			}
		}
		else if (expect_response == ZB_RX_RESPONSE && xbee_obj.getResponse().getApiId() == ZB_RX_RESPONSE)
		{
			xbee_obj.getResponse().getZBRxResponse(rx_response);
			if (rx_response.isAvailable())
			{
				res = conn_status_succeed;
				*len = rx_response.getDataLength();
				for (int i = 0; i < rx_response.getDataLength(); i++)
				{
					value[i] = rx_response.getData()[i];
					//printf("%X ", value[i]);
				}
			}
			else
			{
				printf("Recv return error code:%X\n", rx_response.getErrorCode());
			}
		}
		else if (expect_response == ZB_TX_STATUS_RESPONSE && xbee_obj.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE)//ZB_TX_STATUS_RESPONSE
		{
			xbee_obj.getResponse().getZBTxStatusResponse(txStatus);
			if (txStatus.getStatus() == SUCCESS)
			{
				res = conn_status_succeed;
				//puts("TX success\n");
			}
			else
			{
				printf("TX Error:0X%X\n", txStatus.getStatus());
			}
		}
		else 
		{
			
			printf("Expected %X response but got%X\n", AT_COMMAND_RESPONSE, xbee_obj.getResponse().getApiId());

		}
	}
	else 
	{		
		res = conn_status_timeout;
		puts("Recv timeout\n");
		// command failed
		/*
		if (xbee_obj.getResponse().isError())
		{			
			printf("Error reading packet.  Error code:%X\n", xbee_obj.getResponse().getErrorCode());
		}
		else 
		{
			puts("No response from XBee api radio");
			
		}
		*/
	}
	return res;
}

conn_status send_command_to_api_end_device(XBeeAddress64 xbee_mac_addr, char *cmd, int len)
{
	conn_status res = conn_status_failed;
	int send_buf_len = len + 2; //add start code and end code
	char *send_buf = (char*)malloc(sizeof(char)*send_buf_len);
	ZBTxRequest zbTx;

	memset(send_buf, '\0', send_buf_len);
	charcat(send_buf, cmd, 1, len);
	//memcpy(send_buf, cmd, len);
	send_buf[0] = 0x02; //end of the command
	send_buf[send_buf_len - 1] = 0x03; //end of the command

	zbTx = ZBTxRequest(xbee_mac_addr, (uint8_t*)send_buf, send_buf_len);
	xbee_obj.send(zbTx);
	//RS232_SendBuf(COM_XBee_API, (unsigned char*)send_buf, len + 2);

	free(send_buf);

	return res;
}

recv_result recv_command_from_api_end_device(char *buf, int *len)
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
		//recv_len = RS232_PollComport(COM_XBee_API, (unsigned char*)recv_buf, MAXRECVBUFFER);
		recv_response_api_mode(recv_buf, &recv_len, ZB_RX_RESPONSE);
		if (recv_len>0)
		{
			charcat(temp_buf, recv_buf, offset, recv_len);
			offset += recv_len;
			if (start_recv == 0 && temp_buf[0] == start_code_api)
			{
				start_recv = 1;
			}

			if (start_recv==1)
			{
				for (i = offset - recv_len; i < offset; i++)
				{
					if (temp_buf[i] == end_code_api)
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

	clear_XBee_api_recv_buff(300);
	return res;
}

//XBee network maintain

int send_package_to_Manager_XBee_api(IoTSocket socket, IoT_Package *package)
{
	char send_buf[1000] = { '\0' };
	int n = 0;
	n = encode_package(send_buf, package);

	Mutex_lock(&xbee_api_lock);
	//send(xbee_at_broker_socket, send_buf, len, 0);
	send(socket, send_buf, n, 0);
	Mutex_unlock(&xbee_api_lock);
	return 0;
}

recv_result recv_raw_char_from_XBee_api(char *base_buf, int max_len, unsigned long timeout)
{
	const int temp_buf_len = 500;
	int len = 0, offset = 0;
	char temp_buf[temp_buf_len] = { "\0" };

	unsigned long start_time = get_millis();
	unsigned long wait_time = 0;

	//printf("Start receive:");
	while (1)
	{
		len = RS232_PollComport(COM_XBee_API, (unsigned char*)temp_buf, max_len);
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

void scan_XBee_api_devices()
{
	const int buf_len = 1000;
	const int max_fail_times=5;

	int ack_timeout = 600,len =0,i=0,j=0, fail_times=0,index=0;
	uint32_t dh = 0, dl = 0;

	char receiveBuf[buf_len] = { '\0' };
	char status_buf[100] = { 0 };
	//char xbee_mac_addrs[MAXDEVICECOUNT][MACADDRLEN] = {0};
	XBeeAddress64 xbee_mac_addrs[MAXDEVICECOUNT];
	ZBTxRequest zbTx;
	//char buffer[buf_len] = { '\0' };
	//char xbee_mac_addr_dh[MACADDRLEN] = { '\0' };
	//char xbee_mac_addr_dl [MACADDRLEN] = { '\0' };

	char *cmd_discover = "ND";

	char *syn_msg = "SYN";
	char *ack_msg = "ACK";

	//int mac_pattern_len = strlen(mac_pattern);
	//int end_pattern_len = strlen(end_pattern);

	recv_result result;
	conn_status connection_status;
	
	//Send beacon
	send_AT_command_api_mode(cmd_discover,NULL,0);
	//Read beacon response
	while (1)
	{
		//len = RS232_PollComport(COM_XBee_API, (unsigned char*)receiveBuf, buf_len);
		connection_status = recv_response_api_mode(receiveBuf, &len, AT_COMMAND_RESPONSE);
		if (len > 0 && connection_status==conn_status_succeed)
		{
			
			fail_times = 0;
			if (receiveBuf[0] != 0 && receiveBuf[1] != 0)
			{
				puts("Found Xbee device\n");
				dh = 0, dl = 0;				
				for (i = 0; i < MACADDRLEN/2; i++)
				{
					dh |= (unsigned char)receiveBuf[i + 2];
					if (i < (MACADDRLEN / 2)-1)
					{
						dh = dh << 8;
					}											
				}

				for (i = MACADDRLEN / 2; i < MACADDRLEN ; i++)
				{
					dl |= (unsigned char)receiveBuf[i + 2];
					if (i < (MACADDRLEN) - 1)
					{
						dl = dl << 8;
					}
				}

				xbee_mac_addrs[index].setMsb(dh);
				xbee_mac_addrs[index].setLsb(dl);
				index++;
			}			
		}
		else
		{
			fail_times++;
		}

		if (fail_times >= max_fail_times)
		{
			break;
		}
	}
	
	for (i = 0; i < index; i++)
	{
		printf("Connect to XBee:%X%X\n", xbee_mac_addrs[i].getMsb(), xbee_mac_addrs[i].getLsb());
		send_command_to_api_end_device(xbee_mac_addrs[i],syn_msg,strlen(syn_msg));
		connection_status=recv_response_api_mode(status_buf, &len, ZB_TX_STATUS_RESPONSE);
		if (connection_status==conn_status_succeed)
		{
			connection_status = recv_response_api_mode(status_buf, &len, ZB_RX_RESPONSE);
			if (connection_status == conn_status_succeed)
			{				
				if (len==strlen(ack_msg) && memcmp(ack_msg, status_buf, len) == 0)
				{
					printf("Found a alive XBee device%X%X\n", xbee_mac_addrs[i].getMsb(), xbee_mac_addrs[i].getLsb());
					//add_new_XBee_api_device(xbee_mac_addrs[i]);
					XBee_api_dev_register(xbee_mac_addrs[i]);
					
				}
			}
			else if (connection_status == conn_status_timeout)
			{
				puts("Timeout");
			}
			else
			{
				puts("Receive error");
			}
		}		
	}	
}

void ask_a_IoTIP_for_XBee_api(IoT_Package *package)
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
	send_package_to_Manager_XBee_api(s, package);

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

int XBee_api_dev_register(XBeeAddress64 addr)
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

	if (!is_XBee_api_device_exists(addr))
	{
		add_new_XBee_api_device(addr);
	}

	//Ask a IoTIP then give to XBee_AT device
	ask_a_IoTIP_for_XBee_api(&requestIp);
	update_XBee_api_iot_ip(addr, requestIp.des_ip);
	
	ms_sleep(100); //target need be slow a little
	send_command_to_api_end_device(addr,cmde_des, strlen(cmde_des));
	result = recv_command_from_api_end_device(dev_pack_buf, &length);
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
		send_package_to_Manager_XBee_api(xbee_api_broker_socket, &package);
	}

	free_package(&package);
	free_package(&requestIp);
	puts("End XBee_AT register");
	
	return 0;
}

void clear_XBee_api_recv_buff(unsigned long clear_time)
{
	//RS232_SendBuf(COMPORTNUMBER, (unsigned char*)"AT", 2);

	char temp[100] = { '\0' };
	recv_raw_char_from_XBee_api(temp, 100, clear_time);

	printf("XBee recv buffer cleared\n");
}

//XBee device maintain
int add_new_XBee_api_device(XBeeAddress64 addr)
{
	int i = 0;
	
	if (is_XBee_api_device_exists(addr))
	{
		printf("XBee_AT device is already exists");
		//printAllChar(addr, MACADDRLEN);
		return -1;
	}
	else
	{
		//printf("ADD NEW XBee_AT device:");
		//printAllChar(addr, MACADDRLEN);
	}

	

	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_api_devices[i] == NULL)
		{
			xbee_api_devices[i] = (XBee_api_dev_info*)malloc(sizeof(XBee_api_dev_info));
			xbee_api_devices[i]->xbee_at_mac_addr.setLsb(addr.getLsb());
			xbee_api_devices[i]->xbee_at_mac_addr.setMsb(addr.getMsb());
			xbee_api_devices[i]->ready_to_receive = 1;
			xbee_api_devices[i]->isAlive = 1;
			break;
		}
	}

	return i;
}

void remove_XBee_api_device(int deviceIndex)
{
	printf("Server ip is:%s\n", ServerIP);

	char sendBuffer[MAXPACKETSIZE];
	char cmdBuffer[MAXPACKETSIZE];
	int cmd_len = 0, n = 0;

	//IoT_Package package_info = generate_iot_package();
	IoT_Command cmd = generate_iot_command();

	cmd.cmd_type = command_t_Management;
	strcpy(cmd.ID, "Del_Dev");
	strcpy(cmd.Value, xbee_api_devices[deviceIndex]->ip.ip);
	memset(cmdBuffer, '\0', MAXPACKETSIZE);
	cmd_len = encode_iot_cmd(cmdBuffer, &cmd);

	IoT_Package send_package_info = generate_iot_package();
	create_package(&send_package_info, (char*)"0", ServerIP, cmdBuffer, cmd_len);
	n = encode_package(sendBuffer, &send_package_info);

	//idx = package_info.belongSocketIdx;
	send(xbee_api_broker_socket, sendBuffer, n, 0);
	free_package(&send_package_info);

	//free(xbee_at_devices[deviceIndex]->buffer);
	free(xbee_api_devices[deviceIndex]);


	xbee_api_devices[deviceIndex] = NULL;

}

int find_XBee_api_device_index(XBeeAddress64 addr)
{
	int index = -1, i = 0;
	
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_api_devices[i] != NULL && xbee_api_devices[i]->xbee_at_mac_addr.isEqual(addr))
		{
			index = i;
			break;
		}
	}
	
	return index;
}

int find_XBee_api_device_index_by_IoTIp(IoTIp *ip)
{
	int index = -1, i = 0;

	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_api_devices[i] != NULL &&
			xbee_api_devices[i]->ip.ipLength == ip->ipLength &&
			memcmp(xbee_api_devices[i]->ip.ip,ip->ip, ip->ipLength)==0)
		{
			index = i;
			break;
		}
	}

	return index;
}

void update_XBee_api_iot_ip(XBeeAddress64 addr, IoTIp ip)
{
	int index = find_XBee_api_device_index(addr);
	if (index >= 0)
	{
		//xbee_devices[index]->ip.ip = (char*)malloc(sizeof(char)*ip.ipLength+1);
		memset(xbee_api_devices[index]->ip.ip, '\0', IP_ADDR_SIZE);
		xbee_api_devices[index]->ip.ipLength = ip.ipLength;
		xbee_api_devices[index]->isAlive = 1;
		memset(xbee_api_devices[index]->ip.ip, '\0', xbee_api_devices[index]->ip.ipLength + 1);
		memcpy(xbee_api_devices[index]->ip.ip, ip.ip, xbee_api_devices[index]->ip.ipLength);
	}
	
}

int is_XBee_api_device_exists(XBeeAddress64 addr)
{
	int isExists = 0, i = 0;
	
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_api_devices[i] != NULL)
		{
			//if (xbee_devices[i]->xb_addr.isEqual(*addr))
			if (xbee_api_devices[i]->xbee_at_mac_addr.isEqual(addr))
			{
				isExists = 1;
				break;
			}
		}
	}
	
	return isExists;
}

void clear_all_XBee_api_device()
{
	int i = 0;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (xbee_api_devices[i] != NULL)
		{
			//free(xbee_at_devices[i]->buffer);
			free(xbee_api_devices[i]);
			xbee_api_devices[i] = NULL;
			//break;
		}
	}

	printf("All device data cleared\n");
}
//*******************************
