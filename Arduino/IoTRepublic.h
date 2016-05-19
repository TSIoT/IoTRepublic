#ifndef IoTRepublic_H_INCLUDED
#define IoTRepublic_H_INCLUDED
#include <Arduino.h>
#include <EEPROM.h>

#define MAX_PACKAGE_SIZE 20

enum DeviceType
{
	Unknown=-1,
	XBee_AT=0,
	XBee_API=1,
	BLEBee=2
};

enum conn_status
{
	conn_status_succeed=0,
	conn_status_timeout=-1,
	conn_status_failed=-2
};

enum recv_result
{
	recv_result_NOERROR = 0,
	recv_result_COMPLETED = 1,
	recv_result_INVALID_PACKAGE = -1,
	recv_result_CHECKSUM_ERROR = -2,
	recv_result_TIMEOUT = -3,
};

enum cmd_Type
{
	cmd_Type_None = -1,
	cmd_Type_Normal = 0,
	cmd_Type_SYN = 1,
	cmd_Type_Description_Request = 2,
	
};

typedef struct
{	
	cmd_Type cmd_type;
	char content[MAX_PACKAGE_SIZE];
	int length;
	
}Dev_Package;


extern DeviceType Current_Device_Type;

//Public function
void InitDevice(DeviceType dev_type);
Dev_Package* ReceivePackage();

//Utility
void printAllChar(char *data, int length);
void charcat(char *base_buf, char *target, int starIdx, int length);
void SendDataToManager(char *buf, int buf_len);

#endif
