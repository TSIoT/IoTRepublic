using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;


using IOTProtocol.JSMN;

namespace IOTProtocol.JsonUtility
{
    public class token_info
    {
        public int num;
        public int[] name_lens=new int[23];
        public char[][] names=new char[23][];
        public jsmntype_t[] types=new jsmntype_t[23];
        public int[] sons=new int[23];

        public static void init_token_info(ref token_info info)
        {
	        int num = 23;
            int[] name_lens = {0,12,0,11,0,6,0,7,0,8,0,5,0,6,0,6,0,15,0,4,0,4,0};
            string str_long_name = "magic numberUDP SegmentHeaderVersionData_LenH_LenIP_SrcIP_DesHeader_ChecksumFlagData";
            char[] long_name=str_long_name.ToCharArray();
            jsmntype_t[] types = {jsmntype_t.JSMN_OBJECT,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_OBJECT,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_OBJECT,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING,jsmntype_t.JSMN_STRING};
            int[] sons = {2,1,0,1,2,1,7,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0};
            int name_pos = 0;
            int name_i, i;
            info.num = num;
            for (i = 0; i < info.num; i++)
            {
                info.name_lens[i] = name_lens[i];
                info.types[i] = types[i];
                info.sons[i] = sons[i];
            }
            for (name_i = 0; name_i < num; name_i++)
            {
                info.names[name_i] = new char[24];
                for (i = 0; i < name_lens[name_i]; i++)
                {
                    info.names[name_i][i] = long_name[name_pos];
                    name_pos++;
                }
                info.names[name_i][name_lens[name_i]] = '\0';
            }
	    
        }
    }
        
	
}
