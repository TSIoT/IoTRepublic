using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using IOTProtocol.JSMN;

namespace IOTProtocol.JsonUtility
{
    public class JsonPacket
    {
        private int packet_i=0, info_i=0, in_i=0;
        //private int MAXPACKETSIZE = 1024;
        private JsFileParser jsFileParser = new JsFileParser();

        public int build_packet(char[] input,token_info info, char[] packet, int tab_num)
        {
	        if(info_i == info.num)	return packet_i;
            int packet_size = 0;
	        int i;
	        int son;
            if (tab_num != 0) Coder.arr_putc('\n', packet, ref packet_i);
            for (i = 0; i < tab_num; i++) Coder.arr_putc('\t', packet, ref packet_i);
	        if(info.name_lens[info_i] != 0)
	        {
                Coder.arr_putc('"', packet, ref packet_i);
                Coder.arr_puts(info.names[info_i], packet, ref packet_i);
                Coder.arr_putc('"', packet, ref packet_i);
	        }
	        switch(info.types[info_i])
	        {
                case jsmntype_t.JSMN_OBJECT:
			        son = info.sons[info_i];
                    Coder.arr_putc('{', packet, ref packet_i);
			        for(i=0;i<son;i++)
			        {
				        info_i++;
				        build_packet(input, info, packet, tab_num+1);
                        if (i != son - 1) Coder.arr_putc(',', packet, ref packet_i);
			        }
                    Coder.arr_putc('\n', packet, ref packet_i);
                    for (i = 0; i < tab_num; i++) Coder.arr_putc('\t', packet, ref packet_i);
                    Coder.arr_putc('}', packet, ref packet_i);
                    
                    if (tab_num == 0)
                    {
                        packet_size = this.packet_i;
                        this.packet_i = 0;
                        this.info_i = 0;
                        this.in_i = 0;
                    }
                    
			        break;
                case jsmntype_t.JSMN_ARRAY:
			        son = info.sons[info_i];
                    Coder.arr_putc('[', packet, ref packet_i);
			        for(i=0;i<son;i++)
			        {
				        info_i++;
				        build_packet(input, info, packet, tab_num+1);
                        if (i != son - 1) Coder.arr_putc(',', packet, ref packet_i);
			        }
                    Coder.arr_putc('\n', packet, ref packet_i);
                    for (i = 0; i < tab_num; i++) Coder.arr_putc('\t', packet, ref packet_i);
                    Coder.arr_putc(']', packet, ref packet_i);
			        break;
		        default:
			        if(info.sons[info_i] == 1)
                    {
                        Coder.arr_putc(':', packet, ref packet_i);
                        if (info.types[info_i + 1] == jsmntype_t.JSMN_OBJECT || info.types[info_i + 1] == jsmntype_t.JSMN_ARRAY)
                        {
					        info_i++;
					        build_packet(input, info, packet, tab_num);
				        }
				        else
                        {
                            Coder.in_arr_puts(input, ref in_i, packet, ref packet_i);
					        info_i++;
				        }
			        }
                    break;
	        }
            return packet_size;
        }

        public void init_json_packet(ref JsonData jsonData)
        {            
	        char[] zero_values=new char[JsFileParser.MAXPACKETSIZE*2];
	        int i;	        
	        token_info info=new token_info();
            token_info.init_token_info(ref info);
            
	        for(i=0;i<JsFileParser.MAXPACKETSIZE;i++)
	        {
		        zero_values[2*i] = '0';
		        zero_values[2*i+1] = '\0';
	        }
            //this.build_packetPrepare();
            jsonData.content_size = this.build_packet(zero_values, info, jsonData.content, 0);

            jsonData.content[jsonData.content_size] = '\0';
            this.jsFileParser.init_token(ref jsonData);
            //this.jsFileParser.init_token(packet_buffer,  ref token_size, ref token_buffer);
          
        }

        public void get_packet_value(char[][] key, int key_size, JsonData jsonData, ref char[] result)
        {

            this.jsFileParser.get_jsfile_value(key, key_size, jsonData, ref  result);      
        }

        public void set_packet_single_value(char[] value, char[][] key, int key_size, ref JsonData jsonData)
        {
            this.jsFileParser.set_jsfile_single_value(value, key, key_size,ref jsonData);         
        }

    }
}
