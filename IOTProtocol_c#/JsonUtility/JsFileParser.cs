using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using IOTProtocol.JSMN;

namespace IOTProtocol.JsonUtility
{
    public class JsFileParser
    {        
        public static int MAXPACKETSIZE = 1024;
        public static int MAXTOKENSIZE = 128;

        private Jsmn jsmn = new Jsmn();        

        public void init_token(char[] data, ref int token_size,ref jsmntok_t[] tokens)
        {            
            jsmn_parser p=new jsmn_parser();            
            this.jsmn.jsmn_init(ref p);
            token_size = this.jsmn.jsmn_parse(ref p, data, (uint)data.Length, ref tokens,(uint)JsFileParser.MAXTOKENSIZE);
            if (token_size < 0 )
            {
                throw new Exception("Initial json file failed! ErrorCode:" + token_size.ToString());
            }            
        }

        public void init_token(ref JsonData jsonData)
        {
            jsmn_parser p = new jsmn_parser();
            this.jsmn.jsmn_init(ref p);
            jsmntok_t[] tokens = jsonData.tokens;
            jsonData.token_size = this.jsmn.jsmn_parse(ref p, jsonData.content, (uint)jsonData.content.Length, ref tokens, (uint)JsFileParser.MAXTOKENSIZE);
            if (jsonData.token_size < 0)
            {
                throw new Exception("Initial json file failed! ErrorCode:" + jsonData.token_size.ToString());
            }
            else
            {
                jsonData.parsed = true;
            }
        }
        
        public void getPatteren(range_t range, char[] content,ref char[] result)
        {
            if (result == null)
                return;

            //result = new char[range.end - range.start + 1];
            int charIndex, i;
            for (charIndex = 0, i = range.start; i < range.end; charIndex++, i++)
            {
                result[charIndex] = content[i];
            }
            result[charIndex] = '\0';            
        }
        /*
        public int searchJsonDataInRange(char[] pattern, range_t range, char[] content, 
            jsmntok_t[] token, int token_size,ref char[] result,ref range_t new_range)
        {
            int i=0,hitIndex=-1;           
            for (i=0;i<token_size;i++)
            {
                if (token[i].start< range.start || token[i].end>range.end)
                    continue;


                if (token[i].type==jsmntype_t.JSMN_STRING || token[i].type==jsmntype_t.JSMN_PRIMITIVE)
                {
                    range_t tempRange=new range_t();
                    tempRange.start = token[i].start;
                    tempRange.end = token[i].end;

                    char[] temp=new char[JsFileParser.MAXPACKETSIZE];
                    //char temp[MAXPACKETSIZE];
                    this.getPatteren(tempRange,content,ref temp);
                    //string str1 = new string(pattern);
                    //string str2 = new string(temp);

                    if (this.strcmp(pattern, temp) == 0)
                    {
                        hitIndex=i+1;
                        new_range.start=token[hitIndex].start;
                        new_range.end=token[hitIndex].end;
				        getPatteren(new_range, content,ref result);
                    }
                    //free(temp);

                    if (hitIndex>=0)
                    {
                        break;
                    }
                }
		        else if( token[i].type==jsmntype_t.JSMN_ARRAY && token[i].start==range.start )
		        {
			        int token_i = i+1;
			        int start;
			        int end;
			        int array_i = 0;
			        int target_i = 0;
			        int pattern_i = 0;
			        while( pattern[pattern_i] != '\0' )
			        {
				        target_i *= 10;
				        target_i += pattern[pattern_i]-'0';
				        pattern_i++;
			        }
			        while(array_i != target_i)
			        {
				        start = token[token_i].start;
				        end = token[token_i].end;
				        while(token[token_i].start <= end)	token_i++;
				        array_i+=1; 
			        }
                    new_range.start=token[token_i].start;
                    new_range.end=token[token_i].end;
			        this.getPatteren(new_range, content,ref result);
			        hitIndex = token_i;
			        break;
		        }
            }
	        if (hitIndex<0)
	        {
                throw new Exception("Json data not found");
		        //printf("Json data not found\n");
	        }

            return hitIndex;

        }
        */
        public int searchJsonDataInRange(char[] pattern, range_t range, JsonData jsonData, ref char[] result, ref range_t new_range)
        {
            int i = 0, hitIndex = -1;
            int token_size = jsonData.token_size;
            jsmntok_t[] token = jsonData.tokens;
            char[] content = jsonData.content;

            for (i = 0; i < token_size; i++)
            {
                if (token[i].start < range.start || token[i].end > range.end)
                    continue;


                if (token[i].type == jsmntype_t.JSMN_STRING || token[i].type == jsmntype_t.JSMN_PRIMITIVE)
                {
                    range_t tempRange = new range_t();
                    tempRange.start = token[i].start;
                    tempRange.end = token[i].end;

                    char[] temp = new char[JsFileParser.MAXPACKETSIZE];
                    //char temp[MAXPACKETSIZE];
                    this.getPatteren(tempRange, content, ref temp);
                    //string str1 = new string(pattern);
                    //string str2 = new string(temp);

                    if (this.strcmp(pattern, temp) == 0)
                    {
                        hitIndex = i + 1;
                        new_range.start = token[hitIndex].start;
                        new_range.end = token[hitIndex].end;
                        getPatteren(new_range, content, ref result);
                    }
                    //free(temp);

                    if (hitIndex >= 0)
                    {
                        break;
                    }
                }
                else if (token[i].type == jsmntype_t.JSMN_ARRAY && token[i].start == range.start)
                {
                    int token_i = i + 1;
                    int start;
                    int end;
                    int array_i = 0;
                    int target_i = 0;
                    int pattern_i = 0;
                    while (pattern[pattern_i] != '\0')
                    {
                        target_i *= 10;
                        target_i += pattern[pattern_i] - '0';
                        pattern_i++;
                    }
                    while (array_i != target_i)
                    {
                        start = token[token_i].start;
                        end = token[token_i].end;
                        while (token[token_i].start <= end) token_i++;
                        array_i += 1;
                    }
                    new_range.start = token[token_i].start;
                    new_range.end = token[token_i].end;
                    this.getPatteren(new_range, content, ref result);
                    hitIndex = token_i;
                    break;
                }
            }
            if (hitIndex < 0)
            {
                throw new Exception("Json data not found");
                //printf("Json data not found\n");
            }

            return hitIndex;

        }
        
        public void PrintAllToken(char[] data, jsmntok_t[] tokens,int tokenSize)
        {

            string content = new string(data);
            for (int i = 0; i < tokenSize; i++)
            {
                //Console.Write("Type:" + (int)(this.tokens[i].type) + ",Start=" +
                        //this.tokens[i].start + ",End=" + this.tokens[i].end + ",Size" + this.tokens[i].size);
                Console.Write(i.ToString()+":Type:" + (int)(tokens[i].type) + ",Start=" +
                        tokens[i].start + ",End=" + tokens[i].end + ",Size=" + tokens[i].size);
                if (tokens[i] != null && (tokens[i].type == jsmntype_t.JSMN_STRING || tokens[i].type == jsmntype_t.JSMN_PRIMITIVE))
                {

                    string str = content.Substring(tokens[i].start, tokens[i].end - tokens[i].start);
                    Console.WriteLine(",Content:" + str);
                }
                else
                {
                    Console.WriteLine("\n");
                }
            }
            
        }

        public void PrintAllToken(JsonData jsonData)
        {

            string content = new string(jsonData.content);
            jsmntok_t[] tokens = jsonData.tokens;
            for (int i = 0; i < jsonData.token_size; i++)
            {
                //Console.Write("Type:" + (int)(this.tokens[i].type) + ",Start=" +
                //this.tokens[i].start + ",End=" + this.tokens[i].end + ",Size" + this.tokens[i].size);
                Console.Write(i.ToString() + ":Type:" + (int)(tokens[i].type) + ",Start=" +
                        tokens[i].start + ",End=" + tokens[i].end + ",Size=" + tokens[i].size);
                if (tokens[i] != null && (tokens[i].type == jsmntype_t.JSMN_STRING || tokens[i].type == jsmntype_t.JSMN_PRIMITIVE))
                {

                    string str = content.Substring(tokens[i].start, tokens[i].end - tokens[i].start);
                    Console.WriteLine(",Content:" + str);
                }
                else
                {
                    Console.WriteLine("\n");
                }
            }

        }
        
        public int getArrayLength(char[][] key, int key_size, JsonData jsonData)
        {
            int hitIndex = this.get_hit_index(key, key_size, jsonData);
            int arrayLength = jsonData.tokens[hitIndex].size;
            return arrayLength;

        }
        
        public void get_jsfile_value(char[][] key, int key_size, JsonData jsonData, ref char[] result)
        {

            range_t range = this.getAllRange(jsonData.tokens);
            range_t temp_range = new range_t();
            int i;
            for (i = 0; i < key_size; i++)
            {
                temp_range.start = range.start;
                temp_range.end = range.end;
                if (i == key_size - 1)
                {
                    this.searchJsonDataInRange(key[i], temp_range, jsonData, ref result, ref range);
                }
                else
                {
                    char[] noUse = null;
                    this.searchJsonDataInRange(key[i], temp_range, jsonData, ref noUse, ref range);
                }
                //this.jsFileParser.searchJsonDataInRange(key[i], temp_range, content, token, token_size,ref result, ref range);
            }
        }

        public void set_jsfile_single_value(char[] value, char[][] key, int key_size,ref JsonData jsonData)
        {            
           
            jsmntok_t[] token=jsonData.tokens;
            char[] content=jsonData.content;
            int token_size = jsonData.token_size;

            range_t range = this.getAllRange(token);
            range_t temp_range = new range_t();
            int i = 0, i2 = 0, i_value = 0;
            int shift = 0;
            int data_flag = 0;


            for (i = 0; i < key_size; i++)
            {
                temp_range.start = range.start;
                temp_range.end = range.end;
                char[] noUse = null;
                this.searchJsonDataInRange(key[i], temp_range, jsonData, ref noUse, ref range);
            }
            string str = new string(key[i - 1]);

            if (str == "Data" && value[0] == '{')
            {
                data_flag = 1;
            }
            char[] temp_content = new char[JsFileParser.MAXPACKETSIZE];
            i = 0;
            i2 = 0;
            i_value = 0;
            while (true)
            {
                if (i < range.start || i >= range.end)
                {
                    temp_content[i2] = content[i];
                    if (content[i] == '\0') break;
                    i++;
                    i2++;
                }
                else
                {
                    if (value[i_value] == '\0')
                    {
                        i = range.end;
                    }
                    else
                    {
                        /*
                        if (data_flag == 1 && (i_value == 0 || value[i_value - 1] == '\n'))
                        {
                            if (i_value == 0)
                            {
                                temp_content[i2] = '\n';
                                i2++;
                            }
                            temp_content[i2] = '\t';
                            temp_content[i2 + 1] = '\t';
                            i2 += 2;
                        }
                        */
                        temp_content[i2] = value[i_value];
                        i2++;
                        i_value++;
                    }
                }
            }
            shift = i2 - token[0].end;
            i = 0;
            Coder.arr_puts(temp_content, content, ref i);
            content[i] = '\0';
            for (i = 0; i < token_size; i++)
            {
                if (token[i].start > range.start)
                {
                    token[i].start += shift;
                }
                if (token[i].end >= range.start)
                {
                    token[i].end += shift;
                }
            }
            //content_size = token[0].end;
            jsonData.content_size = token[0].end;

            return;

        }

        public string getFixString(char[] charArray)
        {
            StringBuilder sb = new StringBuilder();
            int index = 0;
            while(charArray[index]!='\0')
            {
                sb.Append(charArray[index]);
                index++;
            }

            return sb.ToString();

        }

        public int strcmp(char[] str1, char[] str2)
        {
            int index = 0;
            int different = 0;
            while(str1[index]!='\0' && str2[index]!='\0')
            {
                if(str1[index]!=str2[index])
                {
                    different++;
                }
                index++;
            }

            if(str1[index]!='\0' || str2[index]!='\0')
            {
                different++;
            }
            
            return different;

        }

        private int get_hit_index(char[][] key, int key_size, JsonData jsonData)
        {
            int hitIndex = -1;

            range_t range = this.getAllRange(jsonData.tokens);
            range_t temp_range = new range_t();
            int i;
            for (i = 0; i < key_size; i++)
            {
                temp_range.start = range.start;
                temp_range.end = range.end;

                char[] noUse = null;
                hitIndex = this.searchJsonDataInRange(key[i], temp_range, jsonData, ref noUse, ref range);
            }

            return hitIndex;
        }

        private range_t getAllRange(jsmntok_t[] token)
        {
            range_t allRange = new range_t();
            allRange.start = token[0].start;
            allRange.end = token[0].end;
            return allRange;
        }

        /*
        public void encodeInRange(range_t range, char[] content, jsmntok_t[] token, int token_size,out char[] encoded)
        {            
            encoded = new char[JsFileParser.MAXPACKETSIZE];
            int i = 0, t = 0, r = 0;
            int tempLength = 0;
            for (i = 0; i < token_size; i++)
            {
                if (token[i].size == 0 && (token[i].type == jsmntype_t.jsmntype_t.JSMN_STRING || token[i].type == jsmntype_t.JSMN_PRIMITIVE))
                {
                    range_t range_temp = new range_t();
                    range_temp.start = token[i].start;
                    range_temp.end = token[i].end;
                    //char* temp = (char*)malloc(token[i].end - token[i].start + 1);
                    char[] temp = new char[(token[i].end - token[i].start) + 1];
                    this.getPatteren(range_temp, content, out temp);
                    tempLength += (temp.Length + 1);

                    for (t = 0; t < (temp.Length); t++)
                    {
                        encoded[r] = temp[t];
                        r++;
                    }
                }
            }
            return;
        }
        */

    }
}
