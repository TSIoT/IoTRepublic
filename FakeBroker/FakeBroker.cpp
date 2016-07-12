#include "FakeBroker.h"
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

TSThread tcp_to_faker_thread;
IoTSocket fake_broker_socket;

//thread
//int ble_listen_to_end_device_loop(void);
int faker_listen_to_manager_loop(void);

//public main function
void start_fake_broker();
void stop_fake_broker();

//private main function
int init_faker_broker();
void unInit_faker_broker();

void faker_register();

//BLE network maintain
//int send_package_to_BLEBee(IoT_Package package);
int send_fake_package_to_Manager(IoTSocket socket, IoT_Package *package);
//recv_result recv_package_from_BLEBee(IoT_Package *package);
//recv_result recv_package_from_Manager(IoTSocket socket, IoT_Package *package);

void ask_a_faker_IoTIP(IoT_Package *package);

//listen tcp server send to ble device
int faker_listen_to_manager_loop(void)
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

	if (connect(fake_broker_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		puts("Fake broker connect to manager error");
		return -1;
	}	

	faker_register();


	while (1)
	{
		n = recv(fake_broker_socket, temp_recv, MAXRECV, 0);
		if (n > 0)
		{
			charcat(temp_buf, temp_recv, offset, n);
			offset += n;
			do
			{				
				result = getCompletedPackage(temp_buf, &offset, &recv_package);
				if (result == recv_result_COMPLETED)
				{
					
					recv_cmd = generate_iot_command();
					decode_iot_cmd(recv_package.data, &recv_cmd);					

					puts("Receive a package from manager");
					//Generate a Iot package with IoT_Command
					send_package = generate_iot_package();
					send_cmd = generate_iot_command();
					send_cmd.cmd_type = command_t_ReadResponse;
					memcpy(send_cmd.ID, recv_cmd.ID, COMMAND_CHAR_SIZE);

					//handle fake value
					if(recv_cmd.cmd_type==command_t_ReadRequest)
					{
						int value_int = (rand()%100);
						char value_char[20] = { '\0' };
						sprintf(value_char, "%d", value_int);

						//sprintf()
						memset(send_cmd.Value, '\0', COMMAND_CHAR_SIZE
						);
						memcpy(send_cmd.Value, value_char, strlen(value_char));
					}
					else if (recv_cmd.cmd_type == command_t_Write)
					{
						memset(send_cmd.Value, '\0', COMMAND_CHAR_SIZE);
						memcpy(send_cmd.Value, "OK", 2);
					}
					

					memset(cmd_buf, '\0', max_cmd_buf_size);
					cmd_len = encode_iot_cmd(cmd_buf, &send_cmd);

					create_package(&send_package, recv_package.des_ip.ip, recv_package.sor_ip.ip, cmd_buf, cmd_len);
					send_fake_package_to_Manager(fake_broker_socket, &send_package);

					free_package(&send_package);
				}

			} while (result == recv_result_COMPLETED);
		}				
	}
	return 0;
}

//public main function

void start_fake_broker()
{
	init_faker_broker();	
	Thread_create(&tcp_to_faker_thread, (TSThreadProc)faker_listen_to_manager_loop, NULL);
	Thread_run(&tcp_to_faker_thread);
	
}

void stop_fake_broker()
{
	Thread_stop(&tcp_to_faker_thread);
	Thread_kill(&tcp_to_faker_thread);	

	unInit_faker_broker();
}

//private main function
int init_faker_broker()
{
	//int i, n, s, bdrate = 57600;
	int i = 0;
	

	//Initial tcp ip
#if defined(WIN32)
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Fake Broker Failed. Error Code : %d", WSAGetLastError());
		PAUSE;
		return -1;
	}
#endif

	if ((fake_broker_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Fake Broker Could not create socket\n");
		PAUSE;
		return -1;
	}

	printf("BLE Initialised.\n");
	return 0;
}

void unInit_faker_broker()
{	
	//uninit tcp ip
	CloseIoTSocket(fake_broker_socket);
}

//BLE network maintain
int send_fake_package_to_Manager(IoTSocket socket, IoT_Package *package)
{
	char send_buf[1000] = { '\0' };
	int n = 0;
	n = encode_package(send_buf, package);	
	//send(ble_broker_socket, send_buf, len, 0);
	send(socket, send_buf, n, 0);
	
	return 0;
}


void ask_a_faker_IoTIP(IoT_Package *package)
{
	IoTSocket s;
	recv_result result;

	int n = 0, offset = 0;
	char temp_buf[MAXRECV] = { '\0' };
	char temp_recv[MAXRECV] = { '\0' };

	//Add a new device into device list
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Could not create socket(Fake register)\n");
		PAUSE;
		return;
	}

	printf("Faker Register Initialised.\n");

	struct sockaddr_in address;
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_family = AF_INET;
	address.sin_port = htons(SERVERPORT);

	if (connect(s, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		puts("Faker IP request connect error\n");
		return;
	}

	create_package(package, "0", "0", "0", 1);
	send_fake_package_to_Manager(s, package);

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

void faker_register()
{
	char *deviceDesc="{\"IOTDEV\":{\"DeviceName\":\"Moisture sensor\",\"FunctionGroup\":\"Sensor\",\"DeviceID\":\"arduino.grove.mois01\",\"Component\":[{\"ID\":\"mois\",\"Group\":1,\"Name\":\"Moisture Value\",\"Type\":\"Content\",\"ReadFrequency\":2000}]}}\0";
	IoT_Package ip_request_package = generate_iot_package();
	IoT_Package register_package = generate_iot_package();
	
	ask_a_faker_IoTIP(&ip_request_package);
	create_package(&register_package, ip_request_package.des_ip.ip, ip_request_package.sor_ip.ip,  deviceDesc, strlen(deviceDesc)+1);
	send_fake_package_to_Manager(fake_broker_socket,&register_package);

	
	deviceDesc="{\"IOTDEV\":{\"DeviceName\":\"Digital light sensor\",\"FunctionGroup\":\"Sensor\",\"DeviceID\":\"arduino.grove.dl01\",\"Component\":[{\"ID\":\"lux\",\"Group\":1,\"Name\":\"Brightness\",\"Type\":\"Content\",\"ReadFrequency\":10000}]}}";
	ask_a_faker_IoTIP(&ip_request_package);
	create_package(&register_package, ip_request_package.des_ip.ip, ip_request_package.sor_ip.ip, deviceDesc, strlen(deviceDesc) + 1);
	send_fake_package_to_Manager(fake_broker_socket, &register_package);


	deviceDesc="{\"IOTDEV\":{\"DeviceName\":\"Temperature and humidity sensor\",\"FunctionGroup\":\"Sensor\",\"DeviceID\":\"arduino.grove.pro.ths01\",\"Component\":[{\"ID\":\"tempe\",\"Group\":1,\"Name\":\"Temperature\",\"Type\":\"Content\",\"ReadFrequency\":10000},{\"ID\":\"humi\",\"Group\":2,\"Name\":\"Humidity\",\"Type\":\"Content\",\"ReadFrequency\":10000}]}}";
	ask_a_faker_IoTIP(&ip_request_package);
	create_package(&register_package, ip_request_package.des_ip.ip, ip_request_package.sor_ip.ip, deviceDesc, strlen(deviceDesc) + 1);
	send_fake_package_to_Manager(fake_broker_socket, &register_package);

	deviceDesc = "{\"IOTDEV\":{\"DeviceName\":\"ACT\",\"FunctionGroup\":\"Actuator\",\"DeviceID\":\"arduino.uno.act01\",\"Component\":[{\"ID\":\"FON\",\"Group\":1,\"Name\":\"Fan ON\",\"Type\":\"Button\"},{\"ID\":\"FOF\",\"Group\":1,\"Name\":\"FAN Off\",\"Type\":\"Button\"},{\"ID\":\"WON\",\"Group\":2,\"Name\":\"Water mercury On\",\"Type\":\"Button\"},{\"ID\":\"WOF\",\"Group\":2,\"Name\":\"Water mercury Off\",\"Type\":\"Button\"},{\"ID\":\"SON\",\"Group\":3,\"Name\":\"Smoke On\",\"Type\":\"Button\"},{\"ID\":\"SOF\",\"Group\":3,\"Name\":\"Smoke Off\",\"Type\":\"Button\"}]}}";
	ask_a_faker_IoTIP(&ip_request_package);
	create_package(&register_package, ip_request_package.des_ip.ip, ip_request_package.sor_ip.ip, deviceDesc, strlen(deviceDesc) + 1);
	send_fake_package_to_Manager(fake_broker_socket, &register_package);
	
}


//*******************************
