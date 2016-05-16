#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jsFileParser.h"


#define MAXVALUESIZE 50


void arr_putc(char c, char *des_str, int *pos)
{
	des_str[*pos] = c;
	(*pos)++;
	return;
}

void arr_puts(char *src_str, char *des_str, int *pos)
{

	int i = 0;
	while (src_str[i] != '\0')
	{
		des_str[*pos] = src_str[i];
		i++;
		(*pos)++;
	}
	return;
}

void in_arr_puts(char *src_str, int *src_i, char *des_str, int *pos)
{
	int i = *src_i;
	while (src_str[i] != '\0')
	{
		des_str[*pos] = src_str[i];
		i++;
		(*pos)++;
	}
	*src_i = i + 1;
	return;
}

void token_copy(jsmntok_t *dst, jsmntok_t *src)
{
	dst->type = src->type;
	dst->start = src->start;
	dst->end = src->end;
	dst->size = src->size;
#ifdef JSMN_PARENT_LINKS
	dst->parent = src->size;
#endif
	return;
}

void init_token(JsonData *jsonData)
{
	jsmn_parser p;
	jsmn_init(&p);
	jsonData->content_size = strlen(jsonData->content);
	jsonData->token_size = jsmn_parse(&p, jsonData->content, jsonData->content_size, jsonData->tokens, MAXTOKENSIZE);
	if (jsonData->token_size<0)
	{

		puts("Json Init Error");
		//Do something here, or we will exit without warning

		//exit(EXIT_SUCCESS);
	}
	return;
}

void getPatteren(range_t range,char *content, char *result)
{
	int charIndex,i;

	if(result == NULL)	return;

	for (charIndex=0,i=range.start;i<range.end;charIndex++,i++)
	{
		result[charIndex]=content[i];
	}
	result[charIndex]='\0';
}

int searchJsonDataInRange(char *pattern, range_t range, JsonData jsonData, char *result, range_t *new_range)
{
	int i = 0, hitIndex = -1;
	char temp[MAXCONTENTSIZE];
	int token_size = jsonData.token_size;
	jsmntok_t *token = jsonData.tokens;
	char *content = jsonData.content;

	for (i = 0; i<token_size; i++)
	{
		if (token[i].start< range.start || token[i].end>range.end)
			continue;

		if (token[i].type == JSMN_STRING || token[i].type == JSMN_PRIMITIVE)
		{
			range_t range;
			range.start = token[i].start;
			range.end = token[i].end;

			//char *temp = (char *)malloc(MAXPACKETSIZE*sizeof(char));
			getPatteren(range, content, temp);
			if (strcmp(pattern, temp) == 0)
			{
				hitIndex = i + 1;
				new_range->start = token[hitIndex].start;
				new_range->end = token[hitIndex].end;
				getPatteren(*new_range, content, result);
			}
			if (hitIndex >= 0)
			{
				break;
			}
		}
		else if (token[i].type == JSMN_ARRAY && token[i].start == range.start)
		{
			int token_i = i + 1;
			//int start;
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
				//start = token[token_i].start;
				end = token[token_i].end;
				while (token[token_i].start <= end)	token_i++;
				array_i += 1;
			}
			new_range->start = token[token_i].start;
			new_range->end = token[token_i].end;
			getPatteren(*new_range, content, result);
			hitIndex = token_i;
			break;
		}
	}
	if (hitIndex<0)
	{
		printf("Json data not found\n");
	}
	return hitIndex;

}

range_t getAllRange(jsmntok_t *token)
{
	range_t allRange;
	allRange.start = token[0].start;
	allRange.end = token[0].end;
	return allRange;
}

int get_hit_index(char *key[], int key_size, JsonData jsonData)
{
	int hitIndex = -1;

	range_t range = getAllRange(jsonData.tokens);
	range_t temp_range;
	int i;
	for (i = 0; i < key_size; i++)
	{
		temp_range.start = range.start;
		temp_range.end = range.end;


		hitIndex = searchJsonDataInRange(key[i], temp_range, jsonData,NULL, &range);
	}

	return hitIndex;
}

int getArrayLength(char *key[], int key_size, JsonData jsonData)
{
	int hitIndex = get_hit_index(key, key_size, jsonData);
	int arrayLength = jsonData.tokens[hitIndex].size;
	return arrayLength;
}

void get_jsfile_value(char *key[], int key_size, JsonData jsonData, char *result)
{
	range_t range = getAllRange(jsonData.tokens);
	range_t temp_range;
	//char *result_cpy;
	int i;
	for (i = 0; i<key_size; i++)
	{
		temp_range.start = range.start;
		temp_range.end = range.end;
		if (i == key_size - 1)	searchJsonDataInRange(key[i], temp_range, jsonData, result, &range);
		else	searchJsonDataInRange(key[i], temp_range, jsonData, NULL, &range);
	}
	return;
}

void set_jsfile_single_value(char *value, char *key[], int key_size, JsonData *jsonData)
{

	jsmntok_t *token = jsonData->tokens;
	char *content = jsonData->content;
	int token_size = jsonData->token_size;

	range_t range = getAllRange(token);
	range_t temp_range;
	int i, i2, i_value;
	int shift;
	//int data_flag;


	for (i = 0; i<key_size; i++)
	{
		temp_range.start = range.start;
		temp_range.end = range.end;
		searchJsonDataInRange(key[i], temp_range, *jsonData, NULL, &range);
	}
	/*
	if (strcmp(key[i - 1], "Data") == 0 && value[0] == '{')
	{
		data_flag = 1;
	}
	*/

	char temp_content[MAXCONTENTSIZE];
	i = 0;
	i2 = 0;
	i_value = 0;
	while (1)
	{
		if (i<range.start || i >= range.end)
		{
			temp_content[i2] = content[i];
			if (content[i] == '\0')	break;
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
	arr_puts(temp_content, content, &i);
	content[i] = '\0';
	for (i = 0; i<token_size; i++)
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
	jsonData->content_size = token[0].end;


	return;
}

//used
void PrintAllToken(JsonData jsonData)
{

	//char content[MAXPACKETSIZE];
	char tempStr[MAXCONTENTSIZE];
	jsmntok_t *tokens = jsonData.tokens;
	for (int i = 0; i < jsonData.token_size; i++)
	{
		printf("%d:Type:%d,Start=%d,End=%d,Size=%d", i, (tokens[i].type), tokens[i].start, tokens[i].end, tokens[i].size);

		if (tokens[i].type == JSMN_STRING || tokens[i].type == JSMN_PRIMITIVE)
		{
			range_t tempRange;
			tempRange.start = tokens[i].start;
			tempRange.end = tokens[i].end;
			getPatteren(tempRange, jsonData.content, tempStr);
			printf(",Content:%s\n", tempStr);
		}
		else
		{
			printf("\n");
		}


	}
}




