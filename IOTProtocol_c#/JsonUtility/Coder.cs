using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using IOTProtocol.JSMN;


namespace IOTProtocol.JsonUtility
{
    public class Coder
    {
        private int decoded_i = 0, info_i = 0, in_i = 0;        
        private JsFileParser jsFileParser = new JsFileParser();

        public static void arr_putc(char c, char[] des_str,ref int pos)
        {
            
            des_str[pos] = c;
            pos++;            
        }

        public static void arr_puts(char[] src_str, char[] des_str,ref int pos)
        {
            int i = 0;
            while (src_str[i] != '\0')
            {
                des_str[pos] = src_str[i];
                i++;
                pos++;
            }            
        }

        public static void in_arr_puts(char[] src_str,ref int src_i, char[] des_str,ref int pos)
        {
            int i = src_i;
            while (src_str[i] != '\0')
            {
                des_str[pos] = src_str[i];
                i++;
                pos++;
            }
            src_i = i + 1;            
        }

        public void build_decodedPrepare()
        {
            this.decoded_i = 0;
            this.info_i = 0;
            this.in_i = 0;
        }

        //encode decode
        public int encode(JsonData jsonData,ref char[]output)
        {            
	        int encoded_size=0;
            encoded_size = encodeInRange(jsonData, ref output);	       
	        return encoded_size;
        }

        public void decode(char[] input, ref JsonData jsonData, ref char[] data)
        {
            int data_in_i = 0;

            char[][] key = new char[1][];

            token_info info = new token_info();
            token_info.init_token_info(ref info);
            this.build_decoded(input, ref data_in_i, info, ref jsonData, 0);

            this.jsFileParser.init_token(ref jsonData);

            int i = 0;
            while (input[data_in_i + i] != '\0')
            {
                data[i] = input[data_in_i + i];
                i++;
            }
            data[i] = '\0';
            key[0] = "Data\0".ToCharArray();



            this.jsFileParser.set_jsfile_single_value(data, key, 1, ref jsonData);


        }

        private int encodeInRange(JsonData jsonData, ref char[] encodetemp)
        {
            int i = 0, t = 0, r = 0;            
            int token_size = jsonData.token_size;
            jsmntok_t[] token = jsonData.tokens;
            char[] content = jsonData.content;

            range_t Range = new range_t();
            char[] temp;
            int loopEnd;

            for (i = 0; i < token_size; i++)
            {
                if (token[i].size == 0 && (token[i].type == jsmntype_t.JSMN_STRING || token[i].type == jsmntype_t.JSMN_PRIMITIVE))
                {
                    Range.start = token[i].start;
                    Range.end = token[i].end;

                    //temp = (char*)malloc(token[i].end - token[i].start + 1);
                    temp = new char[token[i].end - token[i].start + 1];
                    //char temp[token[i].end - token[i].start + 1];
                    //char temp[MAXVALUESIZE] = {0};

                    this.jsFileParser.getPatteren(Range, content, ref temp);
                    //tempLength += (strlen(temp)+1);
                    //encodeTemp = (char*)realloc(encodeTemp, sizeof(char)*tempLength);
                    loopEnd = temp.Length;
                    for (t = 0; t < loopEnd; t++)
                    {
                        encodetemp[r] = temp[t];
                        r++;
                    }
                    //free(temp);
                }
            }
            return r;
        }
                
        private void build_decoded(char[] input, ref int data_in_i, token_info info, ref JsonData jsonData, int tab_num)
        {
            //static int decoded_i = 0, info_i = 0, in_i = 0;

            //char[] decoded, ref int decoded_len

            char[] decoded = jsonData.content;

            int i;
            int son;

            if (info_i == info.num)
            {
                return;
            }

            if (tab_num != 0)
            {
                Coder.arr_putc('\n', decoded, ref decoded_i);
            }

            for (i = 0; i < tab_num; i++)
            {
                Coder.arr_putc('\t', decoded, ref decoded_i);
            }

            if (info.name_lens[info_i] != 0)
            {
                Coder.arr_putc('"', decoded, ref decoded_i);
                Coder.arr_puts(info.names[info_i], decoded, ref decoded_i);
                Coder.arr_putc('"', decoded, ref decoded_i);
            }

            switch (info.types[info_i])
            {
                case jsmntype_t.JSMN_OBJECT:
                    son = info.sons[info_i];
                    Coder.arr_putc('{', decoded, ref decoded_i);
                    for (i = 0; i < son; i++)
                    {
                        info_i++;
                        this.build_decoded(input, ref data_in_i, info, ref jsonData, tab_num + 1);
                        if (i != son - 1) Coder.arr_putc(',', decoded, ref decoded_i);
                    }
                    Coder.arr_putc('\n', decoded, ref decoded_i);
                    for (i = 0; i < tab_num; i++) Coder.arr_putc('\t', decoded, ref decoded_i);
                    Coder.arr_putc('}', decoded, ref decoded_i);
                    if (tab_num == 0)
                    {
                        jsonData.content_size = decoded_i;
                        data_in_i = in_i;
                        decoded_i = 0;
                        info_i = 0;
                        in_i = 0;
                    }
                    break;
                case jsmntype_t.JSMN_ARRAY:
                    son = info.sons[info_i];
                    Coder.arr_putc('[', decoded, ref decoded_i);
                    for (i = 0; i < son; i++)
                    {
                        info_i++;
                        build_decoded(input, ref data_in_i, info, ref jsonData, tab_num + 1);
                        if (i != son - 1) Coder.arr_putc(',', decoded, ref decoded_i);
                    }
                    Coder.arr_putc('\n', decoded, ref decoded_i);
                    for (i = 0; i < tab_num; i++) Coder.arr_putc('\t', decoded, ref decoded_i);
                    Coder.arr_putc(']', decoded, ref decoded_i);
                    break;
                default:
                    if (info.sons[info_i] == 1)
                    {
                        Coder.arr_putc(':', decoded, ref decoded_i);
                        if (info.types[info_i + 1] == jsmntype_t.JSMN_OBJECT || info.types[info_i + 1] == jsmntype_t.JSMN_ARRAY)
                        {
                            info_i++;
                            build_decoded(input, ref data_in_i, info, ref jsonData, tab_num);
                        }
                        else
                        {
                            if (jsFileParser.strcmp(info.names[info_i], "Data\0".ToCharArray()) == 0)
                            {
                                Coder.arr_putc('0', decoded, ref decoded_i);
                            }
                            else in_arr_puts(input, ref in_i, decoded, ref decoded_i);
                            info_i++;
                        }
                    }
                    break;
            }
            return;
        }	
                        
        /*
        private int build_decoded(char[] input, token_info info,ref char[] decoded, ref int decoded_len, int tab_num)
        {
	        //static int decoded_i = 0, info_i = 0, in_i = 0;
        
	        if(info_i == info.num) return decoded_i;
       
	        int i;
	        int son;
            if (tab_num != 0) Coder.arr_putc('\n', decoded, ref decoded_i);
	        for(i=0;i<tab_num;i++)	arr_putc('\t', decoded, ref decoded_i);
            //Coder.arr_puts(info.names[info_i], decoded, ref decoded_i);
            if (info.name_lens[info_i] != 0)
            {
                Coder.arr_putc('"', decoded, ref decoded_i);
                Coder.arr_puts(info.names[info_i], decoded, ref decoded_i);
                Coder.arr_putc('"', decoded, ref decoded_i);
            }
	        switch(info.types[info_i])
	        {
                case jsmntype_t.JSMN_OBJECT:
			        son = info.sons[info_i];
                    Coder.arr_putc('{', decoded, ref decoded_i);
			        for(i=0;i<son;i++)
			        {
				        info_i++;
				        build_decoded(input, info, decoded, tab_num+1);
                        if (i != son - 1) Coder.arr_putc(',', decoded, ref decoded_i);
			        }
                    Coder.arr_putc('\n', decoded, ref decoded_i);
                    for (i = 0; i < tab_num; i++) Coder.arr_putc('\t', decoded, ref decoded_i);
                    Coder.arr_putc('}', decoded, ref decoded_i);
                    if (tab_num == 0)
                    {
                        *decoded_len = decoded_i;
                        *data_in_i = in_i;
                        decoded_i = 0;
                        info_i = 0;
                        in_i = 0;
                    }
			        break;
                case jsmntype_t.JSMN_ARRAY:
			        son = info.sons[info_i];
                    Coder.arr_putc('[', decoded, ref decoded_i);
			        for(i=0;i<son;i++)
			        {
				        info_i++;
				        build_decoded(input, info, decoded, tab_num+1);
                        if (i != son - 1) Coder.arr_putc(',', decoded, ref decoded_i);
			        }
                    Coder.arr_putc('\n', decoded, ref decoded_i);
                    for (i = 0; i < tab_num; i++) Coder.arr_putc('\t', decoded, ref decoded_i);
                    Coder.arr_putc(']', decoded, ref decoded_i);
			        break;
		        default:
			        if(info.sons[info_i] == 1)
                    {
                        Coder.arr_putc(':', decoded, ref decoded_i);
                        if (info.types[info_i + 1] == jsmntype_t.JSMN_OBJECT || info.types[info_i + 1] == jsmntype_t.JSMN_ARRAY)
                        {
					        info_i++;
					        build_decoded(input, info, decoded, tab_num);
				        }
				        else
                        {
                            in_arr_puts(input,ref in_i, decoded, ref decoded_i);
					        info_i++;
				        }
			        }
                    break;
	        }
	        return decoded_i;
            
        }	
        */

        
    }
}
