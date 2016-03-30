#ifndef JSFILEPARSER_H_INCLUDED
#define JSFILEPARSER_H_INCLUDED

#include"jsmn.h"
//#include "IoTUtility.h"

#define MAXTOKENSIZE 128
#define MAXCONTENTSIZE 1024

typedef struct
{
	int parsed;		
	jsmntok_t tokens[MAXTOKENSIZE];
	char content[MAXCONTENTSIZE];
	int token_size;
	int content_size;
} JsonData;

typedef struct
{
	int start;
	int end;
} range_t;
	
	void token_copy(jsmntok_t *dst, jsmntok_t *src);
	void init_token(JsonData *jsonData);
	void getPatteren(range_t range, char *content, char *result);
	int searchJsonDataInRange(char *pattern, range_t range, JsonData jsonData, char *result, range_t *new_range);
	range_t getAllRange(jsmntok_t *token);
	void PrintAllToken(JsonData jsonData);	
	void get_jsfile_value(char *key[], int key_size, JsonData jsonData, char *result);
	int getArrayLength(char *key[], int key_size, JsonData jsonData);
	void set_jsfile_single_value(char *value, char *key[], int key_size, JsonData *jsonData);


#endif // JSFILEPARSER_H_INCLUDED
