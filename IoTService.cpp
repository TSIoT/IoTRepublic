#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "IoTService.h"
#include "XBeeBroker_API/XBeeBroker_API.h"
#include "XBeeBroker_AT/XBeeBroker_AT.h"
#include "Utility/jsFileParser.h"
#include "Utility/file.h"
#include "BroadcastServer.h"
#include "BLEBeeBroker/BLEBeeBroker.h"


//Just for test
//#include "XBeeBroker\XBeeApiMode.h"
//#include "XBeeBroker\rs232.h"
#define MAXDEVICECOUNT 100
char *ServerIP = "Master";
char *prefix = "DEV.";
int ip_count=0;
//IoT_Device_Info *all_registed_device;
IoT_Device_Info *registed_devices[MAXDEVICECOUNT];

IoTSocket client_socket[MAXCLIENTS];
Package_Buffer *packageBuffer[MAXCLIENTS];

/********************************************************/
//main function
void init_server_info();
int init_main_server(Server_Object *server_obj);
int main(int argc, char *argv[]);
int start_main_server(Server_Object *server_obj);
int main_server_loop(Server_Object *server_obj);
int close_main_server(Server_Object *server_obj);

//package info
void handle_package(IoT_Package *package_info);
void handle_managent_package(IoT_Package *package_info);
void handle_forwarding_package(IoT_Package *package_info);
void handle_ip_request_package(IoT_Package *package_info);
void discover_package_replier(IoT_Package *package_info, IoT_Command *cmd);

Package_Buffer* generate_package_buffer();
void free_package_buffer(Package_Buffer *package_buffer);

//device info
void print_device_info(IoT_Device_Info *device_info);
void scan_all_device();

int find_device_index_with_ip(IoTIp iotip);
IoT_Device_Info* find_device_with_ip(IoTIp iotip);
int find_device_index_with_id(char *id);
int find_device_index_with_ipstr(char *iotip);
IoT_Device_Info* find_device_with_id(char *id);

void add_new_device(IoTIp iotip, JsonData jsonData, int socketIndex);
void clear_all_device_info();
void free_device_info(IoT_Device_Info *device);
//int encodeAllDevices(char *buffer);
int encodeAllDevices_except_one(char *buffer, char *except_device_ip);
int encodePxdDevices_except_one(char *buffer, char *except_device_ip, int isProxied);

//Config file
void loadConfigFile();

/********************************************************/
//main function

int main(int argc, char *argv[])
{
	//ble_test();

	init_server_info();

	Server_Object serverObj;
	init_main_server(&serverObj);
	start_main_server(&serverObj);
	
	start_broadcast_server();
	start_xbee_api_broker();
	start_ble_broker();
	/*
	start_xbee_at_broker();	
	*/	
	PAUSE;	
	/*
	stop_xbee_at_broker();	
	*/
	stop_ble_broker();
	stop_xbee_api_broker();
	stop_broadcast_server();

	close_main_server(&serverObj);

	return 0;
}

void init_server_info()
{
	int i=0;
	ip_count = 0;
	//all_registed_device = NULL;
	for (i = 0; i < MAXCLIENTS; i++)
	{
		client_socket[i] = 0;
		packageBuffer[i] = NULL;
	}

	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		registed_devices[i] = NULL;
	}

}

int init_main_server(Server_Object *server_obj)
{
#if defined(WIN32)
	WSADATA wsa;
	//printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Main server WSAStartup failed. Error Code : %d", WSAGetLastError());
		PAUSE;
		return -1;
	}
	//printf("Initialised.\n");
#endif

	//Create a socket
	if ((server_obj->listener = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		puts("Main server Could not create socket\n");
		PAUSE;
		return -1;
	}
	puts("Main server Socket created.\n");

	//Prepare the sockaddr_in structure
	server_obj->address.sin_family = AF_INET;
	server_obj->address.sin_addr.s_addr = INADDR_ANY;
	server_obj->address.sin_port = htons(SERVERPORT);


	return 0;
}

int start_main_server(Server_Object *server_obj)
{
	Thread_create(&server_obj->serverThread, (TSThreadProc)main_server_loop, server_obj);
	Thread_run(&server_obj->serverThread);
	return 0;
}

int main_server_loop(Server_Object *server_obj)
{
	IoTSocket new_socket;
	struct sockaddr_in  address;
	int activity, i,  valread;
	//first received buffer
	char buffer[MAXRECV];

	TimeSpan timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;
	//set of socket descriptors
	fd_set master, readfds;

	//Bind
	if (bind(server_obj->listener, (struct sockaddr *)&server_obj->address, sizeof(server_obj->address)) == -1)
	{
		printf("Main server Bind failed\n");
		PAUSE;
		exit(EXIT_FAILURE);
	}

	puts("Main server  Bind done");

	//Listen to incoming connections
	listen(server_obj->listener, MAXCLIENTS);

	//Accept and incoming connection
	puts("Main server Waiting for incoming connections...");


	#if defined(WIN32)
        int addrlen = sizeof(struct sockaddr_in);
	#elif defined(__linux__) || defined(__FreeBSD__)
        socklen_t addrlen = sizeof(struct sockaddr_in);
    #endif

	//clear the socket fd set
	FD_ZERO(&master);
	FD_ZERO(&readfds);

	//add listener socket to fd set

	FD_SET(server_obj->listener, &master);

	while (1)
	{
		readfds = master;
		activity = select(100, &readfds, NULL, NULL, &timeout);

		if (activity == -1)
		{
			printf("Main server select call failed");
			PAUSE;
			exit(EXIT_FAILURE);
		}

		//Add new connection into set
		if (FD_ISSET(server_obj->listener, &readfds))
		{
			if ((new_socket = accept(server_obj->listener, (struct sockaddr *)&address, &addrlen))<0)
			{
				perror("accept");
				PAUSE;
				exit(EXIT_FAILURE);
			}

			//inform user of socket number - used in send and receive commands
			printf("Main server has new connection , socket fd is %d , ip is : %s , port : %d \n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

			//add new socket to array of sockets
			for (i = 0; i < MAXCLIENTS; i++)
			{
				if (client_socket[i] == 0)
				{
					client_socket[i] = new_socket;
					FD_SET(new_socket, &master);
					printf("Main server adding a new item to list of sockets at index %d \n", i);
					packageBuffer[i] = generate_package_buffer();
					break;
				}
			}
		}

		//handle connected conndetion,find which connection active
		for (i = 0; i < MAXCLIENTS; i++)
		{
			/*
			if (FD_ISSET(client_socket[i], &readfds))
			{
				valread = recv(client_socket[i], buffer, MAXRECV, 0);

				if (valread <= 0)
				{
					//Somebody disconnected , get his details and print
					printf("Host disconnected , ip %s , port %d \n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
					//Close the socket and mark as 0 in list for reuse
					FD_CLR(client_socket[i], &master);
					CloseIoTSocket(client_socket[i]);
					client_socket[i] = 0;
					free_package_buffer(packageBuffer[i]);
					packageBuffer[i] = NULL;
				}
				//Echo back the message that came in
				else if (valread>0)
				{
					//add null character, if you want to use with printf/puts or other string handling functions
					//buffer[valread] = '\0';
					printf("FD_Index:%d\n", i);
					printf("%s:%d-", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
					printAllChar(buffer, valread);
					char buf[10];
					strcpy(buf, "Hello");
					//ms_sleep(4000);
					while (1)
					{
						send(client_socket[i], buf, 5, 0);
						ms_sleep(1000);
						printf("Sended\n");
					}


				}
			}
			*/

			if (FD_ISSET(client_socket[i], &readfds))
			{
				//get details of the client
				getpeername(client_socket[i], (struct sockaddr*)&address, &addrlen);

				//Check if it was for closing , and also read the incoming message
				//recv does not place a null terminator at the end of the string (whilst printf %s assumes there is one).
				memset(buffer, '\0', MAXRECV);
				valread = recv(client_socket[i], buffer, MAXRECV, 0);

				if (valread <= 0)
				{
					//Somebody disconnected , get his details and print
					printf("Main server got host disconnected , ip %s , port %d \n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
					//Close the socket and mark as 0 in list for reuse
					FD_CLR(client_socket[i], &master);
					CloseIoTSocket(client_socket[i]);
					client_socket[i] = 0;
					free_package_buffer(packageBuffer[i]);
					packageBuffer[i] = NULL;
				}
				//Echo back the message that came in
				else if (valread>0)
				{
					if (packageBuffer[i]->receiveCount+ valread > MAXRECV)
					{
						puts("Buffer over flow clear All buffer");
						packageBuffer[i]->receiveCount = 0;
						memset(packageBuffer[i]->tempBuffer,'\0', MAXRECV);
					}
					else
					{
						charcat(packageBuffer[i]->tempBuffer, buffer, packageBuffer[i]->receiveCount, valread);
						packageBuffer[i]->receiveCount += valread;
					}


					//try to get a completed IoT Package
					recv_result result;
					do
					{
						IoT_Package package_info = generate_iot_package();

						result = getCompletedPackage(packageBuffer[i]->tempBuffer,
							&packageBuffer[i]->receiveCount,
							&package_info);

						if (result== recv_result_COMPLETED)
						{
							package_info.belongSocketIdx = i;
							handle_package(&package_info);
						}

						free_package(&package_info);
					} while (result == recv_result_COMPLETED);

					if (packageBuffer[i]->receiveCount == 0)
					{
						printf("Buffer[%d] cleared\n",i);
					}


				}
			}

		}
	}
}

int close_main_server(Server_Object *server_obj)
{
	printf("Server closed_1");

	int i;

	CloseIoTSocket(server_obj->listener);
	//printf("Server closed_2");
	//Thread_stop(&server_obj->serverThread);
	//printf("Server closed_2.5");
	//Thread_kill(&server_obj->serverThread);
	//printf("Server closed_3");
	for (i = 0; i < MAXCLIENTS; i++)
	{
		if (packageBuffer[i] != NULL)
		{
			//free_package_buffer(packageBuffer[i]);
		}
	}
	printf("Server closed_4");

	clear_all_device_info();


#if defined(WIN32)
	WSACleanup();
#endif
	printf("Server closed_5");
	return 0;
}


void handle_package(IoT_Package *package_info)
{
	/*
	puts("Completed package:");
	printAllChar(package_info->data, package_info->data_length);
	puts("End Completed package");
	*/
	//ip request package
	if (strcmp(package_info->des_ip.ip, "0") == 0 && strcmp(package_info->sor_ip.ip, "0") == 0)
	{
		//puts("A ip request package");
		handle_ip_request_package(package_info);
		//free_package(&new_package_info);
	}
	//Managent package"
	else if (strcmp(package_info->des_ip.ip, ServerIP) == 0)
	{
		//puts("A Managent package");
		handle_managent_package(package_info);

	}
	//Forwarding package
	else
	{
		//puts("Forwarding package");
		handle_forwarding_package(package_info);
	}
}

void handle_ip_request_package(IoT_Package *package_info)
{

	char suffix[3];
	char newIp[20];
	int offset = 0;
	sprintf(suffix, "%d", ip_count);
	int prefix_len = strlen(prefix);
	//int suffix_len = strlen(suffix);
	ip_count++;
	charcat(newIp, prefix, offset, prefix_len);
	offset += prefix_len;
	charcat(newIp, suffix, offset, prefix_len);

	char buffer[MAXRECV];
	IoT_Package new_package_info = generate_iot_package();
	create_package(&new_package_info, ServerIP, newIp, "0", 1);
	int packageLength = encode_package(buffer, &new_package_info);
	send(client_socket[package_info->belongSocketIdx], buffer, packageLength, 0);
	printf("Give ip:%s\n", new_package_info.des_ip.ip);
	free_package(&new_package_info);
}

void handle_managent_package(IoT_Package *package_info)
{
	//printAllChar(package_info->data, package_info->data_length);
	const int temp_str_len = 50;
	int deviceIndex = -1;
	char str_temp[temp_str_len] = { '\0' };
	JsonData jsonData;
	charcat(jsonData.content, package_info->data, 0, package_info->data_length);
	init_token(&jsonData);
	range_t range;
	range.start = jsonData.tokens[1].start;
	range.end = jsonData.tokens[1].end;
	getPatteren(range, jsonData.content, str_temp);

	//Register
	if (strcmp("IOTDEV", str_temp) == 0)
	{
		puts("A Register package include device info");
		add_new_device(package_info->sor_ip, jsonData, package_info->belongSocketIdx);
	}
	//Command
	else if (strcmp("IOTCMD", str_temp) == 0)
	{
		puts("A Command package");
		IoT_Command cmd = generate_iot_command();
		decode_iot_cmd(package_info->data, &cmd);
		if (cmd.cmd_type != command_t_Management)
		{
			puts("IOTCMD has wrong command type");
			return;
		}

		//if (strcmp(cmd.ID,"Dis_All") == 0 && cmd.cmd_type==command_t_Management)
		if (memcmp(cmd.ID, "Dis_",4) == 0)
		{
			discover_package_replier(package_info,&cmd);
		}
		else if (strcmp(cmd.ID, "Del_Dev") == 0)
		{
			deviceIndex = find_device_index_with_ipstr(cmd.Value);
			if (deviceIndex >= 0)
			{
				printf("Device[%d]: has bean deleted\n", deviceIndex);
				free_device_info(registed_devices[deviceIndex]);
				registed_devices[deviceIndex] = NULL;
			}
		}
		else if (strcmp(cmd.ID, "Prx_Add") == 0)
		{
			deviceIndex = find_device_index_with_ipstr(cmd.Value);
			if (deviceIndex >= 0)
			{
				printf("Device[%d]: has bean proxied\n", deviceIndex);
				registed_devices[deviceIndex]->proxied = 1;
			}
		}
		else if (strcmp(cmd.ID, "Prx_Rmv") == 0)
		{
			deviceIndex = find_device_index_with_ipstr(cmd.Value);
			if (deviceIndex >= 0)
			{
				printf("Device[%d]: proxy released\n", deviceIndex);
				registed_devices[deviceIndex]->proxied = 0;
			}
		}
		else if (strcmp(cmd.ID, "Re_Conf") == 0)
		{
			printf("Reload config");
		}
		else
		{
			printf("unknow manage command:%s", cmd.ID);
		}
	}
}

void handle_forwarding_package(IoT_Package *package_info)
{
	int n=0, idx=0,cmd_len=0;
	char sendBuffer[MAXPACKETSIZE];
	char cmdBuffer[MAXPACKETSIZE];
	printf("forwarding to:%s\n", package_info->des_ip.ip);
	//printAllChar(package_info->data, package_info->data_length);
	IoT_Device_Info *device_info = NULL;

	device_info = find_device_with_ip(package_info->des_ip);
	if (device_info == NULL)
	{
		puts("Cannot found target device ip");
		IoT_Command cmd = generate_iot_command();
		cmd.cmd_type = command_t_Management;
		strcpy(cmd.ID, "Del_Dev");
		strcpy(cmd.Value, package_info->des_ip.ip);
		memset(cmdBuffer,'\0', MAXPACKETSIZE);
		cmd_len =encode_iot_cmd(cmdBuffer, &cmd);

		IoT_Package send_package_info = generate_iot_package();
		create_package(&send_package_info, ServerIP, package_info->sor_ip.ip, cmdBuffer, cmd_len);
		n = encode_package(sendBuffer, &send_package_info);

		idx = package_info->belongSocketIdx;
		send(client_socket[idx], sendBuffer,n,0);
		free_package(&send_package_info);
	}
	else
	{
		n = encode_package(sendBuffer, package_info);
		//printf("Send to XBee agent:\n");
		//printAllChar(sendBuffer,n);
		idx = device_info->socket_index;
		send(client_socket[idx], sendBuffer, n, 0);

		//print_device_info(device_info);
	}
	//scan_all_device();
}

void discover_package_replier(IoT_Package *package_info, IoT_Command *cmd)
{
	const int max_device_list_buf = 4096;
	int data_len = 0,n=0;
	char allDeviceList[max_device_list_buf] = { '\0' };
	char send_buf[max_device_list_buf] = { '\0' };

	if (strcmp(cmd->ID, "Dis_All") == 0)
	{
		puts("Discover all device\n");
		data_len = encodeAllDevices_except_one(allDeviceList, package_info->sor_ip.ip);
	}
	else if (strcmp(cmd->ID, "Dis_Pxd")==0)
	{
		puts("Discover Proxied device\n");
		data_len = encodePxdDevices_except_one(allDeviceList, package_info->sor_ip.ip,1);
	}
	else if (strcmp(cmd->ID, "Dis_NPx")==0)
	{
		puts("Discover Non-Proxied device\n");
		data_len = encodePxdDevices_except_one(allDeviceList, package_info->sor_ip.ip, 0);
	}

	//int data_len=encodeAllDevices(allDeviceList);
	IoT_Package new_package = generate_iot_package();
	create_package(&new_package, ServerIP, package_info->sor_ip.ip, allDeviceList, data_len);
	n = encode_package(send_buf, &new_package);

	send(client_socket[package_info->belongSocketIdx], send_buf, n, 0);
	printf("Device info Data:%d sended\n", n);
	free_package(&new_package);
}

Package_Buffer* generate_package_buffer()
{
	Package_Buffer *bufferInfo;
	bufferInfo = (Package_Buffer*)malloc(sizeof(Package_Buffer));
	bufferInfo->receiveCount = 0;
	//bufferInfo->tempBuffer = (char*)malloc(sizeof(char)*MAXRECV);
	//memset(bufferInfo->tempBuffer,'\0',MANAGER_MAX_RECV_BUF);
	memset(bufferInfo->tempBuffer, '\0', MAXRECV);
	return bufferInfo;
}

void free_package_buffer(Package_Buffer *package_buffer)
{
	/*
	if (package_buffer->tempBuffer != NULL)
		free(package_buffer->tempBuffer);
	*/
	free(package_buffer);
}

//device info
void print_device_info(IoT_Device_Info *device_info)
{
	puts("--------------");
	printf("IP:%s\n", device_info->ip.ip);
	printf("Idx:%d\n", device_info->socket_index);
	printf("F_Group:%s\n", device_info->funGroup);
	printf("D_ID:%s\n", device_info->devID);
	printf("Description:%s\n", device_info->devDesc);
	puts("--------------");
}

void scan_all_device()
{
	int i = 0;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (registed_devices[i] != NULL)
		{
			printf("%d:\n", i);
			//printf("IP:%s,D_ID:%s,F_GP:%s,idx:%d", temp->ip, temp->devID,temp->funGroup,temp->socket_index);
			printf("IP:%s\n", registed_devices[i]->ip.ip);
			printf("D_ID:%s\n", registed_devices[i]->devID);
			printf("F_Group:%s\n", registed_devices[i]->funGroup);
			printf("Idx:%d\n", registed_devices[i]->socket_index);
			puts("\n");
		}
	}
	/*
	IoT_Device_Info *temp = all_registed_device;
	int num = 0;
	while (temp != NULL)
	{
		printf("%d:\n",num);
		//printf("IP:%s,D_ID:%s,F_GP:%s,idx:%d", temp->ip, temp->devID,temp->funGroup,temp->socket_index);
		printf("IP:%s\n", temp->ip.ip);
		printf("D_ID:%s\n", temp->devID);
		printf("F_Group:%s\n", temp->funGroup);
		printf("Idx:%d\n", temp->socket_index);
		puts("\n");
		num++;

		if (temp->nextDevice != NULL)
		{
			temp = (IoT_Device_Info*)temp->nextDevice;
		}
		else
		{
			temp = NULL;
		}
	}
	*/
}

int find_device_index_with_ip(IoTIp iotip)
{
	int i = 0;
	int index=-1;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (registed_devices[i]!=NULL && strcmp(registed_devices[i]->ip.ip, iotip.ip) == 0)
		{
			index = i;
			break;
		}
	}

	return index;
}

int find_device_index_with_ipstr(char *iotip)
{
	int i = 0;
	int index = -1;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (registed_devices[i] != NULL && strcmp(registed_devices[i]->ip.ip, iotip) == 0)
		{
			index = i;
			break;
		}
	}

	return index;
}

IoT_Device_Info* find_device_with_ip(IoTIp iotip)
{
	int i = 0;
	IoT_Device_Info *target_device = NULL;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (registed_devices[i]!=NULL)
		{
			if (strcmp(registed_devices[i]->ip.ip, iotip.ip) == 0)
			{
				target_device = registed_devices[i];
				break;
			}
		}
	}
	/*

	IoT_Device_Info *target_device = all_registed_device;
	int num = 0;
	while (target_device != NULL)
	{
		print_device_info(target_device);

		if (strcmp(target_device->ip.ip, iotip.ip) == 0)
		{
			break;
		}

		if (target_device->nextDevice != NULL)
		{
			target_device = (IoT_Device_Info*)target_device->nextDevice;
		}
		else
		{
			target_device = NULL;
		}
	}
	*/
	return target_device;
}

int find_device_index_with_id(char *id)
{
	int i = 0;
	int index = -1;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (registed_devices[i] != NULL && strcmp(registed_devices[i]->devID, id) == 0)
		{
			index = i;
			break;
		}
	}

	return index;
}

IoT_Device_Info* find_device_with_id(char *id)
{
	int i = 0;
	IoT_Device_Info *target_device = NULL;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (registed_devices[i] != NULL && strcmp(registed_devices[i]->devID, id) == 0)
		{
			target_device = registed_devices[i];
			break;
		}
	}
	/*
	IoT_Device_Info *target_device = all_registed_device;
	int num = 0;
	while (target_device != NULL)
	{
		//print_device_info(target_device);

		if (strcmp(target_device->devID, id) == 0)
		{
			break;
		}

		if (target_device->nextDevice != NULL)
		{
			target_device = (IoT_Device_Info*)target_device->nextDevice;
		}
		else
		{
			target_device = NULL;
		}
	}
	*/
	return target_device;
}

void add_new_device(IoTIp iotip,JsonData jsonData,int socketIndex)
{
	char *keys[2];
	const int temp_buffer_size = 200;
	int i = 0;
	IoT_Device_Info *newDevice = (IoT_Device_Info*)malloc(sizeof(IoT_Device_Info));;


	newDevice->proxied = 0;
	newDevice->socket_index = socketIndex;

	keys[0] = "IOTDEV";
	//newDevice->ip.ip = (char*)malloc(sizeof(char)*iotip.ipLength+1);
	memset(newDevice->ip.ip, '\0', IP_ADDR_SIZE);
	strcpy(newDevice->ip.ip, iotip.ip);

	char temp_str[temp_buffer_size];
	keys[1] = "DeviceID";
	get_jsfile_value(keys, 2, jsonData, temp_str);
	newDevice->devID = (char*)malloc(sizeof(char)*strlen(temp_str) + 1);
	strcpy(newDevice->devID, temp_str);

	keys[1] = "FunctionGroup";
	get_jsfile_value(keys, 2, jsonData, temp_str);
	newDevice->funGroup = (char*)malloc(sizeof(char)*strlen(temp_str) + 1);
	strcpy(newDevice->funGroup, temp_str);

	newDevice->devDesc = (char*)malloc(sizeof(char)*strlen(jsonData.content) + 1);
	strcpy(newDevice->devDesc, jsonData.content);
	//int len1 = strlen(jsonData.content);
	//int len2 = strlen(newDevice->devDesc);



	//Check if the device id is already exists
	//IoT_Device_Info *existsDevice = find_device_with_id(newDevice->devID);
	int deviceIndex = find_device_index_with_id(newDevice->devID);
	if (deviceIndex>=0) //if device is already exists
	{
		printf("%s is already exists!! update the exists device infomation\n",registed_devices[deviceIndex]->devID);
		free_device_info(registed_devices[deviceIndex]);
		registed_devices[deviceIndex] = newDevice;

	}
	else //a new device, add into all_registed_device link-list
	{
		//attach new device in head
		for (i = 0; i < MAXDEVICECOUNT; i++)
		{
			if (registed_devices[i] == NULL)
			{
				registed_devices[i] = newDevice;
				break;
			}
		}

	}

}

void clear_all_device_info()
{
	int i = 0;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (registed_devices[i] != NULL)
		{
			printf("Free:%d\n", i);
			free_device_info(registed_devices[i]);
			registed_devices[i] = NULL;
		}
	}
	/*
	IoT_Device_Info *temp = all_registed_device;
	int num = 0;

	while (temp != NULL)
	{
		num++;
		printf("Free:%d\n",num);

		if (temp->nextDevice != NULL)
		{
			all_registed_device = (IoT_Device_Info*)temp->nextDevice;
			free_device_info(temp);
			temp = all_registed_device;
		}
		else
		{
			free_device_info(temp);
			temp = NULL;
		}
	}
	*/
}

void free_device_info(IoT_Device_Info *device)
{
	free(device->funGroup);
	free(device->devID);
	free(device->devDesc);
	free(device->ip.ip);
}
/*
int encodeAllDevices(char *buffer)
{
	IoT_Device_Info *temp = all_registed_device;
	int idx = 0,len=0;
	while (temp != NULL)
	{
		len = strlen(temp->ip.ip);
		charcat(buffer, temp->ip.ip, idx, len);
		idx += len;
		buffer[idx] = SplitSymble;
		idx++;
		len = strlen(temp->devDesc);
		charcat(buffer, temp->devDesc, idx, len);
		idx += len;
		buffer[idx] = SplitSymble;
		idx++;

		if (temp->nextDevice != NULL)
		{
			temp = (IoT_Device_Info*)temp->nextDevice;
		}
		else
		{
			temp = NULL;
		}
	}
	return idx;
}
*/
int encodeAllDevices_except_one(char *buffer,char *except_device_ip)
{
	//IoT_Device_Info *temp = all_registed_device;
	int offset = 0, len = 0,i=0;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (registed_devices[i] != NULL && (strcmp(registed_devices[i]->ip.ip, except_device_ip)!=0))
		{
			if (strcmp(registed_devices[i]->funGroup, "UI") == 0)
				continue;

			len = strlen(registed_devices[i]->ip.ip);
			charcat(buffer, registed_devices[i]->ip.ip, offset, len);
			offset += len;
			buffer[offset] = SplitSymble;
			offset++;
			len = strlen(registed_devices[i]->devDesc);
			charcat(buffer, registed_devices[i]->devDesc, offset, len);
			offset += len;
			buffer[offset] = SplitSymble;
			offset++;
		}
	}

	/*
	while (temp != NULL)
	{
		if(strcmp(temp->ip.ip, except_device_ip)!=0)
		{
			len = strlen(temp->ip.ip);
			charcat(buffer, temp->ip.ip, offset, len);
			offset += len;
			buffer[offset] = SplitSymble;
			offset++;
			len = strlen(temp->devDesc);
			charcat(buffer, temp->devDesc, offset, len);
			offset += len;
			buffer[offset] = SplitSymble;
			offset++;
		}


		if (temp->nextDevice != NULL)
		{
			temp = (IoT_Device_Info*)temp->nextDevice;
		}
		else
		{
			temp = NULL;
		}
	}
	*/
	return offset;
}

int encodePxdDevices_except_one(char *buffer, char *except_device_ip,int isProxied)
{
	//IoT_Device_Info *temp = all_registed_device;
	int offset = 0, len = 0, i = 0;
	for (i = 0; i < MAXDEVICECOUNT; i++)
	{
		if (registed_devices[i] != NULL &&
			(strcmp(registed_devices[i]->ip.ip, except_device_ip) != 0) &&
			registed_devices[i]->proxied== isProxied)
		{
			if (strcmp(registed_devices[i]->funGroup, "UI") == 0)
				continue;

			len = strlen(registed_devices[i]->ip.ip);
			charcat(buffer, registed_devices[i]->ip.ip, offset, len);
			offset += len;
			buffer[offset] = SplitSymble;
			offset++;
			len = strlen(registed_devices[i]->devDesc);
			charcat(buffer, registed_devices[i]->devDesc, offset, len);
			offset += len;
			buffer[offset] = SplitSymble;
			offset++;
		}
	}
	return offset;
}

//Config file
void loadConfigFile()
{
	printf("Start load config file\n");
	TSFile fileHandle;
	const int bufferSize = 1000;
	char buffer[bufferSize];
	memset(buffer,'\0', bufferSize);


	File_open(&fileHandle,"C:\\Users\\loki.chuang\\Desktop\\MobileDesc.txt", O_RDONLY);
	File_read(&fileHandle, buffer, bufferSize);
	File_close(&fileHandle);

	printAllChar(buffer, bufferSize);
}
