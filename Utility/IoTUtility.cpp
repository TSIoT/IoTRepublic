#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "IoTUtility.h"
#include "jsFileParser.h"

char *CurrentVersion = "IUDP1.0";
char SplitSymble = '\0';

//Command
IoT_Command generate_iot_command()
{
	IoT_Command cmd;
	cmd.cmd_type = command_t_None;
	memset(cmd.ID,'\0', COMMAND_CHAR_SIZE);
	memset(cmd.Value, '\0', COMMAND_CHAR_SIZE);

	return cmd;
}

void free_command(IoT_Command *cmd)
{
	if (cmd->ID != NULL)
		free(cmd->ID);

	if (cmd->Value != NULL)
		free(cmd->Value);
}

int encode_iot_cmd(char* cmd_buffer, IoT_Command *cmd)
{
	int empty_length = 0;
	char *keys[2];
	char *empty_cmd = "{\"IOTCMD\":{\"Type\":None,\"ID\":0,\"Value\":0}}";

	keys[0] = "IOTCMD";

	empty_length = strlen(empty_cmd);

	JsonData json_obj;
	strcpy(json_obj.content, empty_cmd);
	init_token(&json_obj);
	//PrintAllToken(json);
	char *value;
	keys[1] = "Type";
	switch (cmd->cmd_type)
	{
	case command_t_ReadRequest:
		value = "Req";
		set_jsfile_single_value(value, keys, 2, &json_obj);
		break;

	case command_t_ReadResponse:

		value = "Res";
		set_jsfile_single_value(value, keys, 2, &json_obj);
		break;

	case command_t_Write:
		value = "Wri";
		set_jsfile_single_value(value, keys, 2, &json_obj);
		break;

	case command_t_Management:
		value = "Man";
		set_jsfile_single_value(value, keys, 2, &json_obj);
		break;

	default:
		puts("Invalid command type");
		return -1;
		break;
	}

	if (cmd->ID != NULL)
	{
		keys[1] = "ID";
		set_jsfile_single_value(cmd->ID, keys, 2, &json_obj);
	}

	if (cmd->Value != NULL)
	{
		keys[1] = "Value";
		set_jsfile_single_value(cmd->Value, keys, 2, &json_obj);
	}

	strcpy(cmd_buffer, json_obj.content);

	return json_obj.content_size + 1;
}

void decode_iot_cmd(char* cmd_buffer, IoT_Command *cmd)
{
	const int max_value_size = 100;
	JsonData json_obj;

	strcpy(json_obj.content, cmd_buffer);
	init_token(&json_obj);
	char *keys[2];
	keys[0] = "IOTCMD";
	char tempValue[max_value_size];

	memset(tempValue, '\0', max_value_size);
	keys[1] = "Type";
	get_jsfile_value(keys, 2, json_obj, tempValue);
	if (strcmp(tempValue, "Req") == 0)
	{
		cmd->cmd_type = command_t_ReadRequest;
	}
	else if (strcmp(tempValue, "Res") == 0)
	{
		cmd->cmd_type = command_t_ReadResponse;
	}
	else if (strcmp(tempValue, "Wri") == 0)
	{
		cmd->cmd_type = command_t_Write;
	}
	else if (strcmp(tempValue, "Man") == 0)
	{
		cmd->cmd_type = command_t_Management;
	}

	memset(tempValue, '\0', max_value_size);
	keys[1] = "ID";
	get_jsfile_value(keys, 2, json_obj, tempValue);
	memset(cmd->ID,'\0', COMMAND_CHAR_SIZE);
	//cmd->ID = (char*)malloc(sizeof(char)*strlen(tempValue) + 1);
	strcpy(cmd->ID, tempValue);

	memset(tempValue, '\0', max_value_size);
	keys[1] = "Value";
	get_jsfile_value(keys, 2, json_obj, tempValue);
	memset(cmd->Value, '\0', COMMAND_CHAR_SIZE);
	//cmd->Value = (char*)malloc(sizeof(char)*strlen(tempValue) + 1);
	strcpy(cmd->Value, tempValue);

}



//package
int isVaildPackage(char *buffer, int buffer_length)
{
	int isVaild = 1;
	int versionStringLength = strlen(CurrentVersion);

	//Check version
	if (buffer_length < versionStringLength)
	{
		isVaild = 0;
	}
	else if (memcmp(CurrentVersion, buffer, versionStringLength) != 0)
	{
		isVaild = 0;
	}

	return isVaild;
}

void create_package(IoT_Package *package, char *sor_ip, char *des_ip, char *data, int data_length)
{
	int header_length = 0;
	char str_header_length[10], str_data_length[10];

	package->ver_length = strlen(CurrentVersion);
	//package->ver = (char*)malloc(sizeof(char)*package->ver_length+1);
	memset(package->ver,'\0', VERSION_CHAR_SIZE);
	strcpy(package->ver, CurrentVersion);

	package->sor_ip.ipLength = strlen(sor_ip);
	//package->sor_ip.ip = (char*)malloc(sizeof(char)*package->sor_ip.ipLength + 1);
	memset(package->sor_ip.ip, '\0', IP_ADDR_SIZE);
	strcpy(package->sor_ip.ip, sor_ip);


	package->des_ip.ipLength = strlen(des_ip);
	//package->des_ip.ip = (char*)malloc(sizeof(char)*package->des_ip.ipLength + 1);
	memset(package->des_ip.ip, '\0', IP_ADDR_SIZE);
	strcpy(package->des_ip.ip, des_ip);


	package->data_length = data_length;

	package->data = (char*)malloc(sizeof(char)*package->data_length);
	memset(package->data, '\0', package->data_length);
	memcpy(package->data, data, package->data_length);

	header_length += package->ver_length;
	header_length++; //for split symbol
	header_length += package->sor_ip.ipLength;
	header_length++; //for split symbol
	header_length += package->des_ip.ipLength;
	header_length++; //for split symbol
	sprintf(str_data_length, "%d", data_length);
	header_length += strlen(str_data_length);
	header_length++; //for split symbol

	if (header_length <= 8)
	{
		header_length += 1;
	}
	else if (header_length <= 97)
	{
		header_length += 2;
	}
	else
	{
		header_length += 3;
	}
	header_length++; //for split symbol
	package->header_length = header_length;

}

int encode_package(char* buffer, IoT_Package *packageInfo)
{
	int i = 0, offset = 0;
	uint32_t sum = 0;
	char str_head_len[5], str_data_len[5];

	//version
	for (i = 0; i < packageInfo->ver_length; i++)
	{
		buffer[offset] = packageInfo->ver[i];
		offset++;
	}
	buffer[offset] = SplitSymble;
	offset++;

	//header length
	sprintf(str_head_len, "%d", packageInfo->header_length);
	for (i = 0; i < strlen(str_head_len); i++)
	{
		buffer[offset] = str_head_len[i];
		offset++;
	}
	buffer[offset] = SplitSymble;
	offset++;

	//data length
	sprintf(str_data_len, "%d", packageInfo->data_length);
	for (i = 0; i < strlen(str_data_len); i++)
	{
		buffer[offset] = str_data_len[i];
		offset++;
	}
	buffer[offset] = SplitSymble;
	offset++;

	//source ip
	IoTIp sor_ip = packageInfo->sor_ip;
	for (i = 0; i < sor_ip.ipLength; i++)
	{
		buffer[offset] = sor_ip.ip[i];
		offset++;
	}
	buffer[offset] = SplitSymble;
	offset++;

	//destination ip
	IoTIp des_ip = packageInfo->des_ip;
	for (i = 0; i < des_ip.ipLength; i++)
	{
		buffer[offset] = des_ip.ip[i];
		offset++;
	}
	buffer[offset] = SplitSymble;
	offset++;

	//buffer[idx] = splitSymble;

	//data
	for (i = 0; i < packageInfo->data_length; i++)
	{
		buffer[offset] = packageInfo->data[i];
		offset++;
	}


	//checksum
	for (i = 0; i < offset; i++)
	{
		sum += (uint8_t) buffer[i];
	}

	while ((sum >> 16) > 0)
	{
		sum = (sum & 0xFFFF) + (sum >> 16);
	}

	buffer[offset] = (uint8_t)(sum >> 8);
	offset++;
	buffer[offset] = (uint8_t)sum;
	offset++;

	/*
	uint16_t testSum = 0;
	testSum |= (uint8_t)buffer[offset - 2];
	testSum = testSum << 8;
	testSum |= (uint8_t)buffer[offset - 1];
	*/
	return offset;
}

void decode_only_header(char* buffer, IoT_Package *packageInfo)
{
	const int tempBufferSize = 200;
	int i = 0, idx = 0;
	char temp[tempBufferSize];
	//version
	i = 0;
	memset(temp, '\0', tempBufferSize);
	while (buffer[idx] != SplitSymble)
	{
		temp[i] = buffer[idx];
		idx++;
		i++;
	}
	//packageInfo->ver = (char*)malloc(sizeof(char)*i + 1);
	memset(packageInfo->ver,'\0',VERSION_CHAR_SIZE);
	memcpy(packageInfo->ver, temp, i + 1);
	packageInfo->ver_length = i;

	//header length
	idx++;
	i = 0;
	memset(temp, '\0', tempBufferSize);
	while (buffer[idx] != SplitSymble)
	{
		temp[i] = buffer[idx];
		idx++;
		i++;
	}
	packageInfo->header_length = atoi(temp);

	//data length
	idx++;
	i = 0;
	memset(temp, '\0', tempBufferSize);
	while (buffer[idx] != SplitSymble)
	{
		temp[i] = buffer[idx];
		idx++;
		i++;
	}
	packageInfo->data_length = atoi(temp);

	//source ip
	idx++;
	i = 0;
	memset(temp, '\0', tempBufferSize);
	while (buffer[idx] != SplitSymble)
	{
		temp[i] = buffer[idx];
		idx++;
		i++;
	}
	//packageInfo->sor_ip.ip = (char*)malloc(sizeof(char)*i + 1);
	packageInfo->sor_ip.ipLength = i;
	memset(packageInfo->sor_ip.ip, '\0', IP_ADDR_SIZE);
	memcpy(packageInfo->sor_ip.ip, temp, i + 1);


	//destination ip
	idx++;
	i = 0;
	memset(temp, '\0', tempBufferSize);
	while (buffer[idx] != SplitSymble)
	{
		temp[i] = buffer[idx];
		idx++;
		i++;
	}
	//packageInfo->des_ip.ip = (char*)malloc(sizeof(char)*i + 1);
	memset(packageInfo->des_ip.ip, '\0', IP_ADDR_SIZE);
	packageInfo->des_ip.ipLength = i;
	memcpy(packageInfo->des_ip.ip, temp, i + 1);
}

int decode_package(char* buffer, IoT_Package *packageInfo)
{
	int i = 0, offset = 0;
	uint32_t sum = 0;
	uint16_t receive_sum=0;
	char str_head_len[5], str_data_len[5];
	decode_only_header(buffer, packageInfo);

	/*
	char temp[200];
	//version
	i = 0;
	memset(temp, '\0', 200);
	while (buffer[idx] != splitSymble)
	{
	temp[i] = buffer[idx];
	idx++;
	i++;
	}
	packageInfo->ver = (char*)malloc(sizeof(char)*i+1);
	memcpy(packageInfo->ver, temp, i+1);
	packageInfo->ver_length = i;

	//header length
	idx++;
	i = 0;
	memset(temp, '\0', 200);
	while (buffer[idx] != splitSymble)
	{
	temp[i] = buffer[idx];
	idx++;
	i++;
	}
	packageInfo->header_length = atoi(temp);

	//data length
	idx++;
	i = 0;
	memset(temp, '\0', 200);
	while (buffer[idx] != splitSymble)
	{
	temp[i] = buffer[idx];
	idx++;
	i++;
	}
	packageInfo->data_length = atoi(temp);

	//source ip
	idx++;
	i = 0;
	memset(temp, '\0', 200);
	while (buffer[idx] != splitSymble)
	{
	temp[i] = buffer[idx];
	idx++;
	i++;
	}
	packageInfo->sor_ip.ip = (char*)malloc(sizeof(char)*i + 1);
	packageInfo->sor_ip.ipLength = i;
	memcpy(packageInfo->sor_ip.ip, temp, i + 1);


	//destination ip
	idx++;
	i = 0;
	memset(temp, '\0', 200);
	while (buffer[idx] != splitSymble)
	{
	temp[i] = buffer[idx];
	idx++;
	i++;
	}
	packageInfo->des_ip.ip = (char*)malloc(sizeof(char)*i + 1);
	packageInfo->des_ip.ipLength = i;
	memcpy(packageInfo->des_ip.ip, temp, i + 1);
	*/

	//data
	offset = packageInfo->header_length;
	packageInfo->data = (char*)malloc(sizeof(char)*packageInfo->data_length);
	for (i = 0; i < packageInfo->data_length; i++)
	{
		packageInfo->data[i] = buffer[offset];
		offset++;
	}

	//checksum
	for (i = 0; i < offset; i++)
	{
		sum += (uint8_t)buffer[i];
	}

	while ((sum >> 16) > 0)
	{
		sum = (sum & 0xFFFF) + (sum >> 16);
	}


	receive_sum |= (uint8_t)buffer[offset];
	receive_sum = receive_sum << 8;
	receive_sum |= (uint8_t)buffer[offset + 1];


	if (receive_sum == sum)
	{
		return 1;
	}
	else
	{
		puts("Checksum error!\n");
		return 0;
	}

	//packageInfo->data[i] = '\0';
	//int a = 10;

}

void free_package(IoT_Package *packageInfo)
{

	if (packageInfo->data != NULL)
		free(packageInfo->data);

	/*
	if (packageInfo->ver != NULL)
		free(packageInfo->ver);


	if (packageInfo->sor_ip.ip != NULL)
		free(packageInfo->sor_ip.ip);

	if (packageInfo->des_ip.ip != NULL)
		free(packageInfo->des_ip.ip);
	*/
}

int isCompleteHeader(char *package,int package_length)
{
	if (package[0] == '\0')
		return 0;

	int splitSymbleCount = 0, dataCount = 0, idx = 0;
	int headerCompleted = 0;

	//Check header is completed
	while (1)
	{
		if (package[idx] == SplitSymble && dataCount>0)
		{
			splitSymbleCount++;
			if (splitSymbleCount == 5)
			{
				headerCompleted = 1;
				break;
			}
			dataCount = 0;
		}
		else if (package[idx] == SplitSymble && dataCount == 0)
		{
			headerCompleted = 0;
			break;
		}
		else
		{
			dataCount++;
		}
		idx++;

		if (idx >= package_length)
		{
			headerCompleted = 0;
			break;
		}
	}

	return headerCompleted;
}

int isCompletedPackage(char *buffer, int *buffer_length)
{
	int packageCompleted = 0;
	int headerCompleted = isCompleteHeader(buffer, *buffer_length);
	if (headerCompleted == 1)
	{
		IoT_Package packageInfo = generate_iot_package();
		decode_only_header(buffer, &packageInfo);
		int package_total_len = packageInfo.header_length + packageInfo.data_length;
		if (*buffer_length >= package_total_len)
		{
			packageCompleted = 1;
		}

		free_package(&packageInfo);
	}

	return packageCompleted;
}

recv_result getCompletedPackage(char *buffer, int *buffer_length, IoT_Package *packageInfo)
{
	if (*buffer_length < strlen(CurrentVersion))
	{
		return recv_result_NOERROR;
	}
	//if the package is not valid, just drop it
	else if (isVaildPackage(buffer, *buffer_length) != 1)
	{
		memset(buffer, '\0', *buffer_length);
		*buffer_length = 0;
		return recv_result_INVALID_PACKAGE;
	}

	int i=0;
	int headerCompleted = isCompleteHeader(buffer, *buffer_length);
	if (headerCompleted == 1)
	{
		//IoT_Package iot_pack = generate_iot_package();
		decode_only_header(buffer, packageInfo);
		int package_total_len = packageInfo->header_length + packageInfo->data_length + 2; //2 is for check sum
		if (*buffer_length >= package_total_len)
		{
			if (decode_package(buffer, packageInfo))
			{
				int another_package_len = *buffer_length - package_total_len;
				int idx = package_total_len;
				for (i = 0; i < another_package_len; i++)
				{
					buffer[i] = buffer[idx];
					idx++;
				}
				for (; i < *buffer_length; i++)
				{
					buffer[i] = '\0';
				}
				*buffer_length = another_package_len;
				return recv_result_COMPLETED;
			}
			else
			{
				memset(buffer, '\0', *buffer_length);
				*buffer_length = 0;
				return recv_result_CHECKSUM_ERROR;
			}
		}
	}
	return recv_result_NOERROR;
}

IoT_Package generate_iot_package()
{
	IoT_Package package_info;
	//package_info.completed_package = 0;
	package_info.checksum = 0;
	package_info.data = NULL;
	package_info.data_length = 0;
	memset(package_info.des_ip.ip,'\0', IP_ADDR_SIZE);
	package_info.header_length = 0;
	//package_info.sor_ip.ip = NULL;
	memset(package_info.sor_ip.ip, '\0', IP_ADDR_SIZE);
	//package_info.ver = NULL;
	memset(package_info.ver,'\0', VERSION_CHAR_SIZE);
	package_info.belongSocketIdx = -1;

	return package_info;
}

void copy_iot_package(IoT_Package *dest, IoT_Package *source)
{
	create_package(dest, source->sor_ip.ip, source->des_ip.ip, source->data,source->data_length);
}

//Utility
void ms_sleep(int ms)
{
#if defined(WIN32)
	Sleep(ms);
#elif defined(__linux__) || defined(__FreeBSD__)
	usleep(ms * 1000);
#endif
}

unsigned long get_millis()
{
	unsigned long long millisecondsSinceEpoch = 0;
#if defined(__linux__) || defined(__FreeBSD__)
	struct timeval tv;
	gettimeofday(&tv, NULL);
	millisecondsSinceEpoch =
		(unsigned long long)(tv.tv_sec) * 1000 +
		(unsigned long long)(tv.tv_usec) / 1000;
	return millisecondsSinceEpoch;
#elif defined(WIN32)
	millisecondsSinceEpoch = GetTickCount();
#endif
	return millisecondsSinceEpoch;
}

void printAllChar(char *data, int length)
{
	int i = 0;
	for (i = 0; i < length; i++)
	{
		if (data[i] == '\0')
			printf(",");
		else
		{
			printf("%c", data[i]);
			//printf("%X ", data[i]);
		}
			
	}
	printf("\n");
}

void charcat(char *base_buf, char *target, int starIdx, int length)
{
	int i = 0, idx = starIdx;

	for (i = 0; i < length; i++)
	{
		base_buf[idx] = target[i];
		idx++;
	}
}

void split_char_array(char *source, char *output1, char *output2, int split_index, int length)
{
	int i = 0, idx = 0;

	for (i = 0; i < split_index; i++)
	{
		output1[i] = source[idx];
		idx++;
	}

	for (i = 0; i < length; i++)
	{
		char temp = source[idx];
		output2[i] = source[idx];
		idx++;
	}

}

int string_search(char *pattern, int startIndex, char *base_str)
{
	int pos_search = 0;
	int pos_text = 0;
	int len_search = strlen(pattern);
	int len_text = strlen(base_str);
	int targetIndex = -1;
	for (pos_text = startIndex; pos_text < len_text; pos_text++)
	{
		if (base_str[pos_text] == pattern[pos_search])
		{
			++pos_search;
			if (pos_search == len_search)
			{
				// match
				targetIndex = pos_text - (len_search - 1);
				//printf("match from %d to %d\n", targetIndex, pos_text);
				//return targetIndex;
				break;
			}
		}
		else
		{
			//pos_text -= pos_search;
			pos_search = 0;
		}
	}


	return targetIndex;
}
