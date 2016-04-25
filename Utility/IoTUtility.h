#ifndef IoTUtility_H_INCLUDED
#define IoTUtility_H_INCLUDED

#if defined(WIN32)
	#include <windows.h>
#elif defined(__linux__) || defined(__FreeBSD__)
	#include <unistd.h>
	#include <sys/time.h>
#endif

extern char *CurrentVersion;
extern char SplitSymble;

#define COMMAND_CHAR_SIZE 10 //The size of value and id
#define VERSION_CHAR_SIZE 10
#define MAXPACKETSIZE 1024

#define IP_ADDR_SIZE 15

typedef struct
{
	char ip[IP_ADDR_SIZE];
	int ipLength;
} IoTIp;

typedef struct
{
	//int completed_package;
	char ver[VERSION_CHAR_SIZE];
	int ver_length;
	int header_length;
	int data_length;
	IoTIp sor_ip;
	IoTIp des_ip;
	int checksum;
	char *data;

	int belongSocketIdx;
} IoT_Package;


enum recv_result
{
	recv_result_NOERROR = 0,
	recv_result_COMPLETED = 1,
	recv_result_INVALID_PACKAGE = -1,
	recv_result_CHECKSUM_ERROR = -2,
	recv_result_TIMEOUT = -3
};

enum conn_status
{
	conn_status_succeed = 0,
	conn_status_timeout = -1,
	conn_status_failed = -2
};

enum command_t
{
	command_t_None = 0,
	command_t_ReadRequest = 1,
	command_t_ReadResponse=2,
	command_t_Write=3,
	command_t_Management = 4,
};

typedef struct
{
	command_t cmd_type;
	char ID[COMMAND_CHAR_SIZE];
	char Value[COMMAND_CHAR_SIZE];

}IoT_Command;

//Command
IoT_Command generate_iot_command();
int encode_iot_cmd(char* cmd_buffer, IoT_Command *cmd);
void decode_iot_cmd(char* cmd_buffer, IoT_Command *cmd);

//Package
void create_package(IoT_Package *package, char *sor_ip, char *des_ip, char *data, int data_length);
int encode_package(char* buffer, IoT_Package *packageInfo);
void decode_only_header(char* buffer, IoT_Package *packageInfo);
int decode_package(char* buffer, IoT_Package *packageInfo);
void free_package(IoT_Package *packageInfo);
int isCompleteHeader(char *package, int package_length);
int isCompletedPackage(char *buffer, int *buffer_length);
recv_result getCompletedPackage(char *buffer, int *buffer_length, IoT_Package *packageInfo);
IoT_Package generate_iot_package();
void copy_iot_package(IoT_Package *dest, IoT_Package *source);

//Utility
void ms_sleep(int ms);
unsigned long get_millis();
void printAllChar(char *data, int length);
void charcat(char *base_buf, char *target, int starIdx, int length);
void split_char_array(char *source, char *output1, char *output2, int split_index, int length);
int string_search(char *pattern, int startIndex, char *base_str);
#endif
