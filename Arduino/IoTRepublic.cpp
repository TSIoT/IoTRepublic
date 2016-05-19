#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <IoTRepublic.h>


#define MAX_WAIT_TIME 100

DeviceType Current_Device_Type= Unknown;
HardwareSerial *RF_Serial=NULL;
Serial_ *Debug_Serial = NULL;
Dev_Package package;

char recv_buf[MAX_PACKAGE_SIZE];
char send_buf[MAX_PACKAGE_SIZE];
char buf[MAX_PACKAGE_SIZE];
char start_code = 0x02;
char end_code = 0x03;
int start_receive = 0;
int start_blank = 0; //for BLE_BEE
int offset = 0;
unsigned long start_receive_time = 0;
unsigned long start_blank_time = 0; //for BLE_BEE
unsigned long wait_time = 0;
unsigned long blank_time = 0; //for BLE_BEE

char *cmd_syn = "SYN";
char *cmd_ack = "ACK";
char *cmd_des = "DES"; //ask device description


//private function
void charcat(char *base_buf, char *target, int starIdx, int length)
{
	int i = 0, idx = starIdx;

	for (i = 0; i < length; i++)
	{
		base_buf[idx] = target[i];
		idx++;
	}
}

int combineInt(uint8_t *raw_data)
{
	int result = 0;
	result |= raw_data[0];
	result = result << 8;
	result |= raw_data[1];


	return result;
}

void send_device_description()
{
	const int max_buf_size = 60;
	int description_len = 0;
	int left_data_length = 0;
	int addr = 0;
	int sendLength = 0;
	unsigned int sum = 0;
	int i = 0;
	int ack_time = 100;
	uint8_t row_length[2] = { 0 };
	char buf[max_buf_size] = { '\0' };
	char checksum[2] = { 0 };

	row_length[0] = EEPROM.read(0);
	row_length[1] = EEPROM.read(1);

	description_len = combineInt(row_length);

	left_data_length = description_len;
	addr = 2; //start from index 2. index 1&2 is length
	memset(buf, '\0', max_buf_size);
	RF_Serial->write(start_code);
	while (left_data_length > 0)
	{
		sendLength = left_data_length > max_buf_size ? max_buf_size : left_data_length;
		for (i = 0; i < sendLength; i++)
		{
			buf[i] = EEPROM.read(addr);
			sum += buf[i];
			addr++;
		}

		//printAllChar(BLEBeeBuffer,sendLength);
		left_data_length -= sendLength;
		RF_Serial->write(buf, sendLength);
		//Debug_Serial->write(buf, sendLength);
		//SendDataToBLE(BLEBeeBuffer, sendLength);
		//delay(ack_time);
	}

	
	checksum[0] = (char)(sum >> 8);
	checksum[1] = (char)(sum);
	checksum[2] =  end_code ;
	RF_Serial->write(checksum, 3);
	
	/*
	Debug_Serial->print("sum:");
	Debug_Serial->println(sum);

	Debug_Serial->println("Check sum:");
	Debug_Serial->print(checksum[0],HEX);
	Debug_Serial->print("_");
	Debug_Serial->println(checksum[1], HEX);

	Debug_Serial->println("Device description sended!");
	*/
}

void reset_recv()
{
	int len = 0,i=0;
	char tempByte;		

	delay(50);
	len = RF_Serial->available();
	if (len>0)
	{
		for (i = 0; i < len; i++)
		{
			tempByte=RF_Serial->read();
		}
	}

	offset = 0;
	start_receive = 0;
	start_receive_time = 0;
		
}

recv_result get_dev_package(char *buf,int *buffer_length, Dev_Package *package)
{	
	int endIndex = 0,i=0,j=0,len=0;	

	//start code error
	if (buf[0] != start_code)
	{
		*buffer_length = 0;
		return recv_result_NOERROR;		
	}
			
	
	for (i = 0; i < *buffer_length; i++)
	{
		if (buf[i] == end_code)
		{
			endIndex = i;
			break;
		}			
	}

	//not have end_code
	if (endIndex == 0)
		return recv_result_NOERROR;

	//packing package
	for (i = 0,j=1; j < endIndex; i++,j++)
	{
		len++;
		package->content[i] = buf[j];
	}
	package->length = len;		
		
	if (package->length == 3 && memcmp(package->content, cmd_syn, strlen(cmd_syn)) == 0)
	{		
		Debug_Serial->println("Got syn, return ACK");
		RF_Serial->write(cmd_ack,strlen(cmd_ack));
		//*buffer_length = 0;
		//start_receive = 0;
		reset_recv();
		return recv_result_NOERROR;
	}
	else if (package->length == 3 && memcmp(package->content, cmd_des, strlen(cmd_des)) == 0)
	{
		Debug_Serial->println("Got DES, return device description");
		send_device_description();
		reset_recv();
		return recv_result_NOERROR;
	}
	

	return recv_result_COMPLETED;					

}


//public function
void InitDevice(DeviceType dev_type)
{
	Current_Device_Type = dev_type;

	if (Current_Device_Type == XBee_AT)
	{
		RF_Serial = &Serial1;
		Debug_Serial = &Serial;

		Debug_Serial->begin(9600);
		RF_Serial->begin(9600);

		Debug_Serial->println("XBee_AT Device Start");

		//XBee sleep pin
		/*
		pinMode(22, OUTPUT);
		digitalWrite(22, LOW);
		Debug_Serial->println("XBee sleep ping low");
		*/
	}
	else if (Current_Device_Type == BLEBee)
	{
		RF_Serial = &Serial1;
		Debug_Serial = &Serial;

		Debug_Serial->begin(9600);
		RF_Serial->begin(9600);

		Debug_Serial->println("BLEBee Device Start");
	}

	package.cmd_type = cmd_Type_None;
	memset(package.content, '\0', MAX_PACKAGE_SIZE);
	package.length = 0;
}

Dev_Package* ReceivePackage()
{
	recv_result res;
	int len = 0;
	int i = 0;
	Dev_Package* completed_package=NULL;
	
	len = RF_Serial->available();
	if (len>0)
	{
		RF_Serial->readBytes(recv_buf,len);
		
		charcat(buf, recv_buf, offset, len);

		if (start_receive == 0)
		{
			start_receive = 1;			
			start_receive_time = millis();
			wait_time = 0;
		}

		if (start_receive == 1)
		{			
			offset += len;

			res=get_dev_package(buf,&offset, &package);
			if (res == recv_result_COMPLETED)
			{
				completed_package = &package;
				offset = 0;
				start_receive = 0;
			}			
		}		
	}
	

	if (start_receive == 1)
	{
		wait_time = millis() - start_receive_time;
		if (wait_time > MAX_WAIT_TIME)
		{
			reset_recv();
		}
	}
	
	if(Current_Device_Type== BLEBee)
	{
		if (start_receive == 1)
		{
			start_blank = 0;
		}
		else if (start_blank == 0)
		{
			start_blank = 1;
			start_blank_time = millis();
		}
		else if(millis()-start_blank_time>5000)
		{			
			RF_Serial->print("AT");
			Debug_Serial->println("Blank timeout! send AT");
			reset_recv();
			start_blank_time = millis();
		}
	}
	
	return completed_package;
}

void SendDataToManager(char *buf,int buf_len)
{
	memset(send_buf, '\0', MAX_PACKAGE_SIZE);
	send_buf[0] = start_code;
	charcat(send_buf, buf, 1, buf_len);

	send_buf[buf_len + 1] = end_code;
	RF_Serial->write(send_buf, buf_len + 2);

	
	Debug_Serial->print(":");
	for (int i = 0; i < buf_len + 2; i++)
	{
		Debug_Serial->print(send_buf[i],HEX);
		Debug_Serial->print("_");
	}
	Debug_Serial->print(":");
	
	reset_recv();
	
	//printAllChar(send_buf, buf_len + 2);
}

//Utility
void printAllChar(char *data, int length)
{
	int i = 0;
	for (i = 0; i < length; i++)
	{
		if (data[i] == '\0')			
			Debug_Serial->print(",");
		//printf(",");
		else
			Debug_Serial->print(data[i]);
		//printf("%c", data[i]);
	}
	Debug_Serial->println();
	//printf("\n");
}


//private function
/*
recv_result recv_char_from_Serial1(char *base_buf, int max_len, unsigned long timeout)//Use for AT Command
{
const int temp_buf_len = 500;
int len = 0, offset = 0;
char temp_buf[temp_buf_len] = { "\0" };

unsigned long start_time = get_millis();
unsigned long wait_time = 0;

//printf("Start receive:");
while (1)
{
//len = RS232_PollComport(COMPORTNUMBER, (unsigned char*)temp_buf, max_len);
len =Serial1.
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


conn_status enter_command_mode()
{
conn_status res = conn_status_failed;
recv_result temp_res = recv_result_INVALID_PACKAGE;
const int buf_len = 5;
unsigned long wait_time = 1000;

char *enter_cmd = "+++";
char *succeed = "OK\r";
char buf[buf_len] = { '\0' };

delay(1000); //safe time
Serial.write(enter_cmd,strlen(enter_cmd));

temp_res = recv_char_from_XBee_at(buf, strlen(succeed), wait_time);
if (temp_res == recv_result_NOERROR)
{
puts("Entering AT Command mode");
printAllChar(buf, strlen(succeed));
res = conn_status_succeed;
}
else
{
printf("Entering command mode Error:%d\n", temp_res);
}
//ms_sleep(wait_time);

return res;
}
*/
