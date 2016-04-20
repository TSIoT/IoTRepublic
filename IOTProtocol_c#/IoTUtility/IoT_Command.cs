using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace IOTProtocol.Utility
{
    public enum command_t
    {
        None = 0,
        ReadRequest = 1,
        ReadResponse = 2,
        Write = 3,
        Management = 4,
    }

    public class IoT_Command
    {
        public command_t cmd_type{get;set;}
        public string ID{get;set;}
        public string Value { get; set; }

        public IoT_Command()
        {
            this.cmd_type = command_t.None;
            this.ID = null;
            this.Value = null;
        }
    }
}
