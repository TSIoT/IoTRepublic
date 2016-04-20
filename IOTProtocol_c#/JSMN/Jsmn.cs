using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;


namespace IOTProtocol.JSMN
{
    public class Jsmn
    {


        /**
         * Allocates a fresh unused token from the token pull.
         */
        private jsmntok_t jsmn_alloc_token(ref jsmn_parser parser, ref jsmntok_t[] tokens, uint num_tokens)
        {
            jsmntok_t tok = new jsmntok_t(); ;
            if (parser.toknext >= num_tokens)
            {
                return null;
            }
            //tok = tokens[parser.toknext++];
            tok.start = tok.end = -1;
            tok.size = 0;
            tokens[parser.toknext++] = tok;
            return tok;
        }

        /**
         * Fills token type and boundaries.
         */
        private void jsmn_fill_token(ref jsmntok_t token, jsmntype_t type, int start, int end)
        {
            token.type = type;
            token.start = start;
            token.end = end;
            token.size = 0;
        }

        /**
         * Fills next available token with JSON primitive.
         */
        private int jsmn_parse_primitive(ref jsmn_parser parser, char[] js, uint len, jsmntok_t[] tokens, uint num_tokens)
        {
            jsmntok_t token = new jsmntok_t();
            int start;

            start = parser.pos;

            for (; parser.pos < len && js[parser.pos] != '\0'; parser.pos++)
            {
                switch (js[parser.pos])
                {

                    case '\t':
                    case '\r':
                    case '\n':
                    case ' ':
                    case ',':
                    case ']':
                    case '}':
                        goto found;
                }
                if (js[parser.pos] < 32 || js[parser.pos] >= 127)
                {
                    parser.pos = start;
                    return (int)jsmnerr_t.JSMN_ERROR_INVAL;
                }
            }


        found:
            if (tokens == null)
            {
                parser.pos--;
                return 0;
            }
            token = jsmn_alloc_token(ref parser, ref tokens, num_tokens);
            if (token == null)
            {
                parser.pos = start;
                return (int)jsmnerr_t.JSMN_ERROR_NOMEM;
            }
            jsmn_fill_token(ref token, jsmntype_t.JSMN_PRIMITIVE, start, parser.pos);

            parser.pos--;
            return 0;
        }

        /**
         * Filsl next token with JSON string.
         */
        private int jsmn_parse_string(ref jsmn_parser parser, char[] js, uint len, jsmntok_t[] tokens, uint num_tokens)
        {
            jsmntok_t token;

            int start = parser.pos;

            parser.pos++;

            /* Skip starting quote */
            for (; parser.pos < len && js[parser.pos] != '\0'; parser.pos++)
            {

                char c = js[parser.pos];

                /* Quote: end of string */
                if (c == '\"')
                {
                    if (tokens == null)
                    {
                        return 0;
                    }
                    token = jsmn_alloc_token(ref parser, ref tokens, num_tokens);
                    if (token == null)
                    {
                        parser.pos = start;
                        return (int)jsmnerr_t.JSMN_ERROR_NOMEM;
                    }
                    jsmn_fill_token(ref token, jsmntype_t.JSMN_STRING, start + 1, parser.pos);

                    return 0;
                }

                /* Backslash: Quoted symbol expected */
                if (c == '\\' && parser.pos + 1 < len)
                {
                    int i;
                    parser.pos++;
                    //char temp = js[parser.pos-1];
                    switch (js[parser.pos-1])
                    {
                        /* Allowed escaped symbols */
                        case '\"':
                        case '/':
                        case '\\':
                        case 'b':
                        case 'f':
                        case 'r':
                        case 'n':
                        case 't':
                            //Console.WriteLine("Enter");
                            break;
                        /* Allows escaped symbol \uXXXX */
                        case 'u':
                            parser.pos++;
                            for (i = 0; i < 4 && parser.pos < len && js[parser.pos] != '\0'; i++)
                            {
                                /* If it isn't a hex character we have an error */
                                if (!((js[parser.pos] >= 48 && js[parser.pos] <= 57) || /* 0-9 */
                                            (js[parser.pos] >= 65 && js[parser.pos] <= 70) || /* A-F */
                                            (js[parser.pos] >= 97 && js[parser.pos] <= 102)))
                                { /* a-f */
                                    parser.pos = start;
                                    return (int)jsmnerr_t.JSMN_ERROR_INVAL;
                                }
                                parser.pos++;
                            }
                            parser.pos--;
                            break;
                        /* Unexpected symbol */
                        default:
                            parser.pos = start;
                            return (int)jsmnerr_t.JSMN_ERROR_INVAL;
                    }
                }
            }
            parser.pos = start;
            return (int)jsmnerr_t.JSMN_ERROR_PART;
        }

        /**
         * Parse JSON string and fill tokens.
         */
        public int jsmn_parse(ref jsmn_parser parser, char[] js, uint len, ref jsmntok_t[] tokens, uint num_tokens)
        {
            int r;
            int i;
            jsmntok_t token;
            int count = 0;

            for (; parser.pos < len && js[parser.pos] != '\0'; parser.pos++)
            {
                char c;
                jsmntype_t type;

                c = js[parser.pos];
                switch (c)
                {
                    case '{':
                    case '[':
                        count++;
                        if (tokens == null)
                        {
                            break;
                        }
                        token = jsmn_alloc_token(ref parser, ref tokens, num_tokens);
                        if (token == null)
                            return (int)jsmnerr_t.JSMN_ERROR_NOMEM;
                        if (parser.toksuper != -1)
                        {
                            tokens[parser.toksuper].size++;
                        }
                        token.type = (c == '{' ? jsmntype_t.JSMN_OBJECT : jsmntype_t.JSMN_ARRAY);
                        token.start = parser.pos;
                        parser.toksuper = parser.toknext - 1;
                        break;
                    case '}':
                    case ']':
                        if (tokens == null)
                            break;
                        type = (c == '}' ? jsmntype_t.JSMN_OBJECT : jsmntype_t.JSMN_ARRAY);

                        for (i = parser.toknext - 1; i >= 0; i--)
                        {
                            //token = tokens[i];
                            if (tokens[i].start != -1 && tokens[i].end == -1)
                            {
                                if (tokens[i].type != type)
                                {
                                    return (int)jsmnerr_t.JSMN_ERROR_INVAL;
                                }
                                parser.toksuper = -1;
                                tokens[i].end = parser.pos + 1;
                                break;
                            }
                        }
                        /* Error if unmatched closing bracket */
                        if (i == -1)
                        {
                            return (int)jsmnerr_t.JSMN_ERROR_INVAL;
                        }
                        for (; i >= 0; i--)
                        {
                            //token = &tokens[i];
                            if (tokens[i].start != -1 && tokens[i].end == -1)
                            {
                                parser.toksuper = i;
                                break;
                            }
                        }

                        break;
                    case '\"':
                        r = jsmn_parse_string(ref parser, js, len, tokens, num_tokens);
                        if (r < 0) return r;
                        count++;
                        if (parser.toksuper != -1 && tokens != null)
                            tokens[parser.toksuper].size++;
                        break;
                    case '\t':
                    case '\r':
                    case '\n':
                    case ' ':
                        break;
                    case ':':
                        parser.toksuper = parser.toknext - 1;
                        break;
                    case ',':
                        if (tokens != null &&
                                tokens[parser.toksuper].type != jsmntype_t.JSMN_ARRAY &&
                                tokens[parser.toksuper].type != jsmntype_t.JSMN_OBJECT)
                        {

                            for (i = parser.toknext - 1; i >= 0; i--)
                            {
                                if (tokens[i].type == jsmntype_t.JSMN_ARRAY || tokens[i].type == jsmntype_t.JSMN_OBJECT)
                                {
                                    if (tokens[i].start != -1 && tokens[i].end == -1)
                                    {
                                        parser.toksuper = i;
                                        break;
                                    }
                                }
                            }

                        }
                        break;

                    /* In non-strict mode every unquoted value is a primitive */
                    default:

                        r = jsmn_parse_primitive(ref parser, js, len, tokens, num_tokens);
                        if (r < 0) return r;
                        count++;
                        if (parser.toksuper != -1 && tokens != null)
                            tokens[parser.toksuper].size++;
                        break;


                }
            }

            for (i = parser.toknext - 1; i >= 0; i--)
            {
                /* Unmatched opened object or array */
                if (tokens[i].start != -1 && tokens[i].end == -1)
                {
                    return (int)jsmnerr_t.JSMN_ERROR_PART;
                }
            }

            return count;
        }

        /**
         * Creates a new parser based over a given  buffer with an array of tokens
         * available.
         */
        public void jsmn_init(ref jsmn_parser parser)
        {
            parser.pos = 0;
            parser.toknext = 0;
            parser.toksuper = -1;
        }

    }
}
