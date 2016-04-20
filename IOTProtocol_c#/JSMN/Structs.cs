using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;


namespace IOTProtocol.JSMN
{
    public class jsmn_parser
    {
        public int pos; /* offset in the JSON string */
        public int toknext; /* next token to allocate */
        public int toksuper; /* superior token node, e.g parent object or array */
    }

    public class jsmntok_t
    {
        public jsmntype_t type;
        public int start;
        public int end;
        public int size;
        public int parent;
    }
}
