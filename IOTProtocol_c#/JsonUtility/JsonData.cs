using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using IOTProtocol.JSMN;
namespace IOTProtocol.JsonUtility
{
    public class JsonData
    {
        public bool parsed=false;
        public jsmntok_t[] tokens =new jsmntok_t[JsFileParser.MAXTOKENSIZE];
        public int token_size = 0;
        public char[] content =new char[JsFileParser.MAXPACKETSIZE];
        public int content_size = 0;
    }
}
