#define _GNU_SOURCE
#include <stdio.h>
#include <ctype.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>

struct {
	struct {
		char *name;
		char *value;
	} *namevalue_pairs;
	int no;
} vars;

void setvar(const char *name, int name_start, int name_len, const char *value, int value_start, int value_len)
{
	char *v = malloc(1+value_len);
	memcpy(v, value+value_start, value_len);
	v[value_len] = '\0';

	int i;
	for(i = 0; i < vars.no; i++)
		if(   strlen(vars.namevalue_pairs[i].name) == name_len
		   && memcmp(vars.namevalue_pairs[i].name, name+name_start, name_len) == 0)
		{
			free(vars.namevalue_pairs[i].value);
			goto set_value;
		}
	
	char *n = malloc(1+name_len);
	memcpy(n, name+name_start, name_len);
	n[name_len] = '\0';
	
	vars.namevalue_pairs = realloc(vars.namevalue_pairs, ++vars.no * sizeof(*(vars.namevalue_pairs)));
	vars.namevalue_pairs[i].name = n;
set_value:
	vars.namevalue_pairs[i].value = v;
}

char *getvar(const char *name, int name_start, int name_len)
{
	for(int i = 0; i < vars.no; i++)
		if(   strlen(vars.namevalue_pairs[i].name) == name_len
		   && memcmp(vars.namevalue_pairs[i].name, name+name_start, name_len) == 0)
			return vars.namevalue_pairs[i].value;
	
	return NULL;
}

void putsubstr(char *s, int start, int len)
{
	for(s = s+start; len--; s++) putchar((int)*s);
}

// need to free result when no longer necessary
char *cstr(char *s, int start, int len)
{
	char *tmp = malloc(len+1);
	tmp[len] = '\0';
	memcpy(tmp,s+start,len);
	return tmp;
}

void parse(char *command, int start, int len, int *no_tokens, int **token_starts, int **token_lengths)
{
	*no_tokens = 0;
	*token_starts = NULL;
	*token_lengths = NULL;
	
	int sub_started = 0;
	char last_char='\0';
	int current_start = 0, i;
	for(i = start; i < start+len; i++)
	{
		char this_char = command[i];
		
		if(sub_started && this_char == '(')
			++sub_started;

		if( (
				(!sub_started)
				&& (   (isalpha(this_char) && !isalpha(last_char))
				   || (isdigit(this_char) && !isdigit(last_char))
				   || (!isalpha(this_char) && !isdigit(this_char))
				)
			)
			|| (last_char == ')' && !--sub_started)
		  )
		{
			if(this_char == '(') ++sub_started;
			if(last_char == '\0') goto store_char_and_continue;

			*token_starts = realloc(*token_starts, ++*no_tokens * sizeof(int));
			(*token_starts)[*no_tokens-1] = current_start;
			*token_lengths = realloc(*token_lengths, *no_tokens * sizeof(int));
			(*token_lengths)[*no_tokens-1] = i - current_start;
			printf("(%d,%d)\n",current_start, i-current_start);

			current_start = i;
		}
		
store_char_and_continue:
		last_char = command[i];
	}
	
	*token_starts = realloc(*token_starts, ++*no_tokens * sizeof(int));
	(*token_starts)[*no_tokens-1] = current_start;
	*token_lengths = realloc(*token_lengths, *no_tokens * sizeof(int));
	(*token_lengths)[*no_tokens-1] = i - current_start;
			printf("*(%d,%d)\n",current_start, i-current_start);
}

void debug_tokens(char *command, int no_tokens, int *token_starts, int *token_lengths)
{
	for(int i = 0; i < no_tokens; i++)
	{
		printf("token %d: (%d,%d)'", i, token_starts[i], token_lengths[i]);
		putsubstr(command, token_starts[i], token_lengths[i]);
		printf("'\n");
	}
}

char *evaluate(char *command, int start, int len)
{
	int no_tokens, *token_starts, *token_lengths;
	char *ret = NULL;
	parse(command, start, len, &no_tokens, &token_starts, &token_lengths);
	debug_tokens(command, no_tokens, token_starts, token_lengths);
	switch(no_tokens) {
		case 1:
		{
			ret = malloc(1+len);
			memcpy(ret, command+start, len);
			ret[len] = '\0';
		}
		break;
		case 2:
		{
			if(token_lengths[1]==1 && command[token_starts[1]] == '?')
			{
				char *r = getvar(command, token_starts[0], token_lengths[0]);
				if(r) ret = strdup(r);
			}
		}
		break;
		case 3:
		{
			if(token_lengths[1]==1 && command[token_starts[1]] == '!')
			{
				setvar(command, token_starts[2], token_lengths[2],
				       command, token_starts[0], token_lengths[0]);
				       
				ret = malloc(1+token_lengths[1]);
				memcpy(ret, command+token_starts[0], token_lengths[0]);
				ret[token_lengths[0]] = '\0';
			}
			else
			if(token_lengths[1]==1 && command[token_starts[1]] == '?')
			{
				for(;;)
				{
					char *le = evaluate(command, token_starts[0], token_lengths[0]);
					int true_val = atoi(le);
					free(ret);
					free(le);

					if(true_val)
					{
						ret = evaluate(command, token_starts[2], token_lengths[2]);
					}
					else
					{
						ret = strdup("");
						break;
					}
				}
			}
		}
		break;
		default:
		{
			ret = strdup("dunno");
		}
		break;
	}
	
	return ret ? ret : calloc(1,1);
}

int main()
{
	char command[4096];
	
	while(1)
	{
		printf(">> ");
		fgets(command, 4096, stdin);
		if(command != strtok(command, "\r\n")) *command = 0;
		printf("GOT: '%s'\n", command);
		char *res = evaluate(command, 0, strlen(command));
		printf("<< '%s'\n", res);
		free(res);
	}
}
