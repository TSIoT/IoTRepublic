using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;


namespace IOTProtocol.JSMN
{
    public enum jsmntype_t
    {       
        JSMN_PRIMITIVE = 0,
        JSMN_OBJECT = 1,
        JSMN_ARRAY = 2,
        JSMN_STRING = 3
    }

    public enum jsmnerr_t
    {
        /* Not enough tokens were provided */
        JSMN_ERROR_NOMEM = -1,
        /* Invalid character inside JSON string */
        JSMN_ERROR_INVAL = -2,
        /* The string is not a full JSON packet, more bytes expected */
        JSMN_ERROR_PART = -3,

        JSMN_ERROR_NOERROR=0
    }
}
