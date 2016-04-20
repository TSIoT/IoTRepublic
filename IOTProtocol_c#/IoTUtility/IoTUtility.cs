using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using IOTProtocol.JsonUtility;

namespace IOTProtocol.Utility
{
    public enum recv_result
    {
        NOERROR = 0,
        COMPLETED = 1,
        INVALID_PACKAGE = -1,
        CHECKSUM_ERROR = -2,
        recv_result_TIMEOUT = -3,
    };

    public class IoTUtility
    {
        public string CurrentVersion = "IUDP1.0";

        private char splitSymble = '\0';
        private JsFileParser jsParser = new JsFileParser();
        #region Command
        public int encode_iot_cmd(ref char[] cmd_buffer, IoT_Command cmd)
        {
            int empty_length = 0;
            char[][] keys = new char[2][];
            //char *empty_cmd = "{\"IOTCMD\":{\"CommandType\":None,\"ReadRequest\":{\"ID\":0},\"ReadResponse\":{\"ID\":0,\"Text\":\"None\"},\"Write\":{\"ID\":0,\"Data\":0}} }";

            string empty_cmd_str = "{\"IOTCMD\":{\"Type\":None,\"ID\":0,\"Value\":0}}\0";
            char[] empty_cmd = new char[JsFileParser.MAXPACKETSIZE];
            empty_cmd_str.CopyTo(0, empty_cmd, 0, empty_cmd_str.Length);

            keys[0] = "IOTCMD\0".ToArray();
            

            empty_length = empty_cmd.Length;

            JsonData json_obj = new JsonData();
            json_obj.content = empty_cmd;
            //strcpy(json_obj.content, empty_cmd);

            jsParser.init_token(ref json_obj);
            //PrintAllToken(json);
            char[] value;
            keys[1] = "Type\0".ToCharArray();
            switch (cmd.cmd_type)
            {
                case command_t.ReadRequest:
                    
                    value = "Req\0".ToCharArray();
                    jsParser.set_jsfile_single_value(value, keys, 2, ref json_obj);
                    break;

                case command_t.ReadResponse:                    
                    value = "Res\0".ToCharArray();
                    jsParser.set_jsfile_single_value(value, keys, 2, ref json_obj);
                    break;

                case command_t.Write:                    
                    value = "Wri\0".ToCharArray();
                    jsParser.set_jsfile_single_value(value, keys, 2, ref json_obj);
                    break;

                case command_t.Management:                    
                    value = "Man\0".ToCharArray();
                    jsParser.set_jsfile_single_value(value, keys, 2, ref json_obj);
                    break;

                default:
                    throw new Exception("Invalid command type");                                       
            }

            if (cmd.ID != null)
            {
                keys[1] = "ID\0".ToCharArray();
                jsParser.set_jsfile_single_value((cmd.ID + "\0").ToCharArray(), keys, 2, ref json_obj);
            }

            if (cmd.Value != null)
            {
                keys[1] = "Value\0".ToCharArray();
                jsParser.set_jsfile_single_value((cmd.Value + "\0").ToCharArray(), keys, 2, ref json_obj);
            }
            cmd_buffer = json_obj.content;
            return json_obj.content_size + 1;
        }

        public void decode_iot_cmd(char[] cmd_buffer, ref IoT_Command cmd)
        {
            const int max_value_size = 100;
            JsFileParser jsParser = new JsFileParser();
            JsonData json_obj = new JsonData();

            json_obj.content = cmd_buffer;
            //strcpy(json_obj.content, cmd_buffer);
            jsParser.init_token(ref json_obj);
            char[][] keys = new char[2][];
            keys[0] = "IOTCMD\0".ToCharArray();
            char[] tempValue = new char[max_value_size];

            //memset(tempValue, '\0', max_value_size);
            keys[1] = "Type\0".ToCharArray();
            jsParser.get_jsfile_value(keys, 2, json_obj, ref tempValue);

            if (jsParser.getFixString(tempValue) == "Req")
            {
                cmd.cmd_type = command_t.ReadRequest;
            }
            else if (jsParser.getFixString(tempValue) == "Res")
            {
                cmd.cmd_type = command_t.ReadResponse;
            }
            else if (jsParser.getFixString(tempValue) == "Wri")
            {
                cmd.cmd_type = command_t.Write;
            }
            else if (jsParser.getFixString(tempValue) == "Man")
            {
                cmd.cmd_type = command_t.Management;
            }
            else
            {
                throw new Exception("Invalid Command type");
            }

            //memset(tempValue, '\0', max_value_size);
            keys[1] = "ID\0".ToCharArray();
            jsParser.get_jsfile_value(keys, 2, json_obj, ref tempValue);
            cmd.ID = jsParser.getFixString(tempValue);

            keys[1] = "Value\0".ToCharArray();
            jsParser.get_jsfile_value(keys, 2, json_obj, ref tempValue);
            cmd.Value = jsParser.getFixString(tempValue);

        }

        #endregion

        #region Package
        public void create_package(ref IoT_Package package, string sor_ip, string des_ip, char[] data, int data_length)
        {
            int header_length = 0;
            char[] str_header_length = new char[10];
            string str_data_length;

            package.ver = CurrentVersion;
            package.ver_length = package.ver.Length;
            package.sor_ip.ip = sor_ip;
            package.des_ip.ip = des_ip;
            package.data_length = data_length;
            package.data = data;

            header_length += package.ver_length;
            header_length++; //for split symbol
            header_length += package.sor_ip.ip.Length;
            header_length++; //for split symbol
            header_length += package.des_ip.ip.Length;
            header_length++; //for split symbol
            str_data_length = data_length.ToString();
            header_length += str_data_length.Length;
            header_length++; //for split symbol

            if(header_length <= 8)
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
            package.header_length = header_length;
        }

        public int encode_package(char[] buffer, IoT_Package packageInfo)
        {
            int i = 0, idx = 0;
            UInt32 sum = 0;
            string str_head_len;
            string str_data_len;
            
            //version 
            for (i = 0; i < packageInfo.ver_length; i++)
            {
                buffer[idx] = packageInfo.ver[i];
                idx++;
            }
            buffer[idx] = splitSymble;
            idx++;

            //header length	
            //sprintf(str_head_len, "%d", packageInfo.header_length);
            str_head_len = packageInfo.header_length.ToString();

            for (i = 0; i < str_head_len.Length; i++)
            {
                buffer[idx] = str_head_len[i];
                idx++;
            }
            buffer[idx] = splitSymble;
            idx++;

            //data length
            //sprintf(str_data_len, "%d", packageInfo->data_length);

            str_data_len = packageInfo.data_length.ToString();
            for (i = 0; i < str_data_len.Length; i++)
            {
                buffer[idx] = str_data_len[i];
                idx++;
            }
            buffer[idx] = splitSymble;
            idx++;

            //source ip
            IoTIp sor_ip = packageInfo.sor_ip;
            for (i = 0; i < sor_ip.ip.Length; i++)
            {
                buffer[idx] = sor_ip.ip[i];
                idx++;
            }
            buffer[idx] = splitSymble;
            idx++;

            //destination ip
            IoTIp des_ip = packageInfo.des_ip;
            for (i = 0; i < des_ip.ip.Length; i++)
            {
                buffer[idx] = des_ip.ip[i];
                idx++;
            }
            buffer[idx] = splitSymble;
            idx++;

            //buffer[idx] = splitSymble;
            //data
            for (i = 0; i < packageInfo.data_length; i++)
            {
                buffer[idx] = packageInfo.data[i];
                idx++;
            }

            //checksum
            for (i = 0; i < idx; i++)
            {
                sum += (byte)buffer[i];
            }

            while ((sum >> 16) > 0)
            {
                sum = (sum & 0xFFFF) + (sum >> 16);
            }


            byte sendByte = 0;
            sendByte = (byte)(sum >> 8);
            buffer[idx] = (char)sendByte;
            idx++;

            sendByte = (byte)sum;
            buffer[idx] = (char)sendByte;
            idx++;

            /*
            UInt16 receive_sum = 0;
            receive_sum |= (byte)buffer[idx-2];
            receive_sum = (UInt16)(receive_sum << 8);
            receive_sum |= (byte)buffer[idx-1];
            */
            return idx;
        }

        public void decode_only_header(char[] buffer, ref IoT_Package packageInfo)
        {
            const int tempBufferSize = 200;
            int i = 0, idx = 0;

            char[] temp = new char[tempBufferSize];
            //version
            i = 0;
            //memset(temp, '\0', tempBufferSize);
            while (buffer[idx] != splitSymble)
            {
                temp[i] = buffer[idx];
                idx++;
                i++;
            }
            packageInfo.ver = this.jsParser.getFixString(temp);
            packageInfo.ver_length = i;

            //header length
            idx++;
            i = 0;
            Array.Clear(temp, 0, tempBufferSize);
            while (buffer[idx] != splitSymble)
            {
                temp[i] = buffer[idx];
                idx++;
                i++;
            }
            packageInfo.header_length = int.Parse(this.jsParser.getFixString(temp));

            //data length
            idx++;
            i = 0;
            Array.Clear(temp, 0, tempBufferSize);
            while (buffer[idx] != splitSymble)
            {
                temp[i] = buffer[idx];
                idx++;
                i++;
            }
            packageInfo.data_length = int.Parse(this.jsParser.getFixString(temp));

            //source ip
            idx++;
            i = 0;
            Array.Clear(temp, 0, tempBufferSize);
            while (buffer[idx] != splitSymble)
            {
                temp[i] = buffer[idx];
                idx++;
                i++;
            }

            packageInfo.sor_ip.ip = this.jsParser.getFixString(temp);
            /*
	packageInfo.sor_ip.ip = (char*)malloc(sizeof(char)*i + 1);
	packageInfo->sor_ip.ipLength = i;
	memcpy(packageInfo->sor_ip.ip, temp, i + 1);
            */

            //destination ip
            idx++;
            i = 0;
            Array.Clear(temp, 0, tempBufferSize);
            while (buffer[idx] != splitSymble)
            {
                temp[i] = buffer[idx];
                idx++;
                i++;
            }

            packageInfo.des_ip.ip = this.jsParser.getFixString(temp);
            /*
	packageInfo->des_ip.ip = (char*)malloc(sizeof(char)*i + 1);
	packageInfo->des_ip.ipLength = i;
	memcpy(packageInfo->des_ip.ip, temp, i + 1);
             */
        }

        public int decode_package(char[] buffer, ref IoT_Package packageInfo)
        {
            int i = 0, offset = 0;
            UInt32 sum = 0;
            UInt16 receive_sum = 0;
            char[] str_head_len = new char[5];
            char[] str_data_len = new char[5];

            this.decode_only_header(buffer, ref packageInfo);

            //data	
            offset = packageInfo.header_length;
            packageInfo.data = new char[packageInfo.data_length];

            for (i = 0; i < packageInfo.data_length; i++)
            {
                packageInfo.data[i] = buffer[offset];
                offset++;
            }

            //checksum
            for (i = 0; i < offset; i++)
            {
                sum += (byte)buffer[i];
            }

            while ((sum >> 16) > 0)
            {
                sum = (sum & 0xFFFF) + (sum >> 16);
            }

            receive_sum = (byte)buffer[offset];
            receive_sum = (UInt16)(receive_sum << 8);
            receive_sum |= (byte)buffer[offset + 1];

            if (receive_sum == sum)
            {
                return 1;
            }
            else
            {                
                //Console.WriteLine("Checksum Error!");
                //throw new Exception("Checksum error!");
                return 0;
            }

            //packageInfo->data[i] = '\0';
            //int a = 10;

        }

        public bool isCompleteHeader(char[] package)
        {            
            int splitSymbleCount = 0, dataCount = 0, idx = 0;
            bool headerComplete = false;            

            //Check header is completed
            while (true)
            {
                if (package[idx] == splitSymble && dataCount > 0)
                {
                    splitSymbleCount++;
                    if (splitSymbleCount == 5)
                    {
                        headerComplete = true;
                        break;
                    }
                    dataCount = 0;
                }
                else if (package[idx] == splitSymble && dataCount == 0)
                {
                    headerComplete = false;
                    break;
                }
                else
                {
                    dataCount++;
                }
                idx++;
            }

            if (headerComplete)
            {
                idx = 0;
            }

            return headerComplete;
        }

        public recv_result getCompletedPackage(char[] buffer,ref int buffer_length,ref IoT_Package packageInfo)
        {
            if(buffer_length < CurrentVersion.Length)
            {
                return recv_result.NOERROR;
            }
            //if the package is not valid, just drop it
            else if (!isVaildPackage(buffer, buffer_length))
            {
                //memset(buffer, '\0', *buffer_length);
                Array.Clear(buffer,0, buffer.Length);
                buffer_length = 0;
                return recv_result.INVALID_PACKAGE;
            }

            int i = 0;
            bool headerCompleted = this.isCompleteHeader(buffer);
            if (headerCompleted)
            {
                //IoT_Package iot_pack = generate_iot_package();
                decode_only_header(buffer,ref packageInfo);
                int package_total_len = packageInfo.header_length + packageInfo.data_length;
                if (buffer_length >= package_total_len)
                {                    
                    if(decode_package(buffer,ref packageInfo)==1)
                    {                        
                        int another_package_len = buffer_length - package_total_len;
                        int idx = package_total_len;
                        for (i = 0; i < another_package_len; i++)
                        {
                            buffer[i] = buffer[idx];
                            idx++;
                        }
                        for (; i < buffer_length; i++)
                        {
                            buffer[i] = '\0';
                        }
                        buffer_length = another_package_len;
                        return recv_result.COMPLETED;
                    }
                    else
                    {
                        Array.Clear(buffer,0, buffer_length);
                        buffer_length = 0;
                        return recv_result.CHECKSUM_ERROR;
                    }
                    /*
                    if (strcmp(packageInfo->data, "Ready") != 0)
                    {
                        puts("In Completed:\n");
                        printAllChar(buffer, package_total_len);
                        puts("\n");
                    }
                    */                    

                    /*
                    if (*buffer_length > 0)
                    {
                        puts("Left data:\n");
                        printAllChar(buffer, *buffer_length);
                    }
                    */
                }
            }

            return recv_result.NOERROR;
        }

        private bool isVaildPackage(char[] buffer, int buffer_length)
        {
            bool isVaild = true;
            int versionStringLength = CurrentVersion.Length;
            string packageVersion = new string(buffer).Substring(0, versionStringLength);
            //Check version
            if (buffer_length < versionStringLength)
            {
                isVaild = false;
            }            
            else if (CurrentVersion!= packageVersion)
            {
                isVaild = false;
            }

            return isVaild;
        }

        #endregion

        #region Utility
        public void charcat(char[] base_buf, char[] target, int starIdx, int length)
        {
            int i = 0, idx = starIdx;

            for (i = 0; i < length; i++)
            {
                base_buf[idx] = target[i];
                idx++;
            }
        }
        #endregion
    }
}
