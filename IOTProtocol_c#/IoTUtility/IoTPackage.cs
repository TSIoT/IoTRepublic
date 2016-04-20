using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace IOTProtocol.Utility
{
    public class IoT_Package
    {
        public int completed_package;
        public string ver;
        public int ver_length;
        public int header_length;
        public int data_length;
        public IoTIp sor_ip=new IoTIp();
        public IoTIp des_ip=new IoTIp();
        public int checksum;
        public char[] data=null;
    }
}
