#define _GNU_SOURCE
#include <stdio.h>
#include <ctype.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <signal.h>


#define DEBUG_PRINTF

#ifdef DEBUG_PRINTF
#define debug_printf printf
#else
#define debug_printf(s, ...) do { } while ( 0 )
#endif

struct {
	struct {
		char *name;
		char *value;
	} *namevalue_pairs;
	int no;
} vars;

int signal_requested;
int loop_exit_requested;
int in_loop;

sighandler_t old_sighandler;

void sighandler(int sig)
{
	signal_requested = 1;
}


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
	int current_start = start, i;
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
			debug_printf("(%d,%d)\n",current_start, i-current_start);

			current_start = i;
		}
		
store_char_and_continue:
		last_char = command[i];
	}
	
	*token_starts = realloc(*token_starts, ++*no_tokens * sizeof(int));
	(*token_starts)[*no_tokens-1] = current_start;
	*token_lengths = realloc(*token_lengths, *no_tokens * sizeof(int));
	(*token_lengths)[*no_tokens-1] = i - current_start;
			debug_printf("*(%d,%d)\n",current_start, i-current_start);
}

void debug_tokens(char *command, int no_tokens, int *token_starts, int *token_lengths)
{
	for(int i = 0; i < no_tokens; i++)
	{
		debug_printf("token %d: (%d,%d)'", i, token_starts[i], token_lengths[i]);
		putsubstr(command, token_starts[i], token_lengths[i]);
		printf("'\n");
	}
}

int operator_shibboleth(char *command, int start, int len)
{
	if(!isalpha(command[start]) && !isdigit(command[start]) && command[start] != '(') return 1;
	return 0;
}

int operator_priority(char *command, int no_tokens, int *token_starts, int *token_lengths, int **oper_prio)
{
	*oper_prio = calloc(sizeof(int), no_tokens);
	
	// pre: first token is not operator!
	for(int i = 0; i < no_tokens; i++)
	{
		if(!operator_shibboleth(command, token_starts[i], token_lengths[i]))
			(*oper_prio)[i] = 1e9;
		else
		{
			if(command[token_starts[i]] == '?')
				(*oper_prio)[i] = 1;
			else if(command[token_starts[i]] == '!')
				(*oper_prio)[i] = 2;
			else if(command[token_starts[i]] == '.')
				(*oper_prio)[i] = 3;
			else if(command[token_starts[i]] == '[' || command[token_starts[i]] == ']')
				(*oper_prio)[i] = 4;
				
			if(operator_shibboleth(command, token_starts[i-1], token_lengths[i-1]))
			{
				(*oper_prio)[i-1] += 1000; // last was one-argument op
			}
			
			if(i == no_tokens-1) (*oper_prio)[i] = 1000; // one-argument op	
		}
	}
}

char *evaluate(char *command, int start, int len)
{
	int no_tokens, *token_starts, *token_lengths;
	char *ret = NULL;
	printf("evaluate(%d, %d)\n", start, len);
	parse(command, start, len, &no_tokens, &token_starts, &token_lengths);
	debug_tokens(command, no_tokens, token_starts, token_lengths);
		
	if(no_tokens > 3 || no_tokens == 3 && operator_shibboleth(command, token_starts[2], token_lengths[2]))
	{
		int *oper_prio = NULL;
		operator_priority(command, no_tokens, token_starts, token_lengths, &oper_prio);
		// let's find biggest prio
		int minprio = 1e9, minprio_index = -1;
		for(int i = 0; i < no_tokens; i++)
			if(oper_prio[i] < minprio)
			{
				minprio = oper_prio[i];
				minprio_index = i;
			}
			// rejoin tokens;
			
		int *new_token_starts;
		int *new_token_lengths;
		if(minprio_index == no_tokens-1)
		{
				// two ops
			no_tokens = 2;
			new_token_starts = calloc(2, sizeof *new_token_starts);
			new_token_lengths = calloc(2, sizeof *new_token_starts);
				
			new_token_starts[0] = token_starts[0];
			new_token_lengths[0] = token_starts[minprio_index] - token_starts[0];

			new_token_starts[1] = token_starts[minprio_index];
			new_token_lengths[1] = len - new_token_lengths[0];				
		}
		else
		{
			no_tokens = 3;
			new_token_starts = calloc(3, sizeof *new_token_starts);
			new_token_lengths = calloc(3, sizeof *new_token_starts);

			new_token_starts[0] = token_starts[0];
			new_token_lengths[0] = token_starts[minprio_index] - token_starts[0];

			new_token_starts[1] = token_starts[minprio_index];
			new_token_lengths[1] = token_lengths[minprio_index];				
				
			new_token_starts[2] = new_token_starts[1] + new_token_lengths[1];
			new_token_lengths[2] = len - (new_token_lengths[0]+new_token_starts[1]);
		}
			
		free(token_lengths);
		free(token_starts);
			
		token_starts = new_token_starts;
		token_lengths = new_token_lengths;
	}

	printf("AFTER MERGE\n");	
		debug_tokens(command, no_tokens, token_starts, token_lengths);

	
	if(signal_requested)
	{
		signal_requested = 0;
		printf("Interrupt: exit last frame.\n");
		if(in_loop) loop_exit_requested = 1;
		return strdup("0");
	}
	
back_after_join:
	switch(no_tokens) {
		
		case 1:
		{
			// good: T
			// bad:  *
			if(operator_shibboleth(command, token_starts[0], token_lengths[0]))
			{
				printf("ERROR has occured => 1 arg * (operator only)!\n");
			}
			if(command[token_starts[0]] == '(' && command[token_starts[0]+token_lengths[0]-1] == ')')
			{
				printf("!!!!! %d %d !!!!!!\n", token_starts[0]+1, token_lengths[0]-2);
				ret = evaluate(command, token_starts[0]+1, token_lengths[0]-2);
			}
			else
			{
				ret = malloc(1+len);
				memcpy(ret, command+start, len);
				ret[len] = '\0';
			}
		}
		break;
		case 2:
		{
			// good:
			// T*
			// bad:
			// *x
			// TT
			if(operator_shibboleth(command, token_starts[0], token_lengths[0]))
			{
				printf("ERROR has occured => 2 arg *T or ** (operator first) !\n");
				ret = strdup("");
				signal_requested = 1;
				break;
			}
			if(!operator_shibboleth(command, token_starts[1], token_lengths[1]))
			{
				printf("ERROR has occured => 2 arg TT (no operator) !\n");
				ret = strdup("");
				signal_requested = 1;
				break;				
			}
			
			// T*
			if(token_lengths[1]==1 && command[token_starts[1]] == '?')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				char *r = getvar(le, 0, strlen(le));
				if(r) ret = strdup(r);
				free(le);
			}
			else
			{}
			
		}
		break;
		// good:
		// T*T
		// T**
		// bad:
		// *xx
		// TTx
		case 3:
		{
			if(operator_shibboleth(command, token_starts[0], token_lengths[0]))
			{
				printf("ERROR has occured => 3 arg *xx (operator first) !\n");
				signal_requested = 1;
				break;
			}
			else if(!operator_shibboleth(command, token_starts[1], token_lengths[1]))
			{
				printf("ERROR has occured => 3 arg TTx (second is not operator) !\n");
				signal_requested = 1;
				break;
			}
			if(token_lengths[1]==1 && command[token_starts[1]] == '!')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				int len = strlen(le);
			
				setvar(command, token_starts[2], token_lengths[2],
				       le, 0, len);
				
				ret = malloc(1+len);
				memcpy(ret, le, len);
				ret[token_lengths[0]] = '\0';
				free(le);
			}
			else
			if(token_lengths[1]==1 && command[token_starts[1]] == '?')
			{
				in_loop = 1;
				for(;;)
				{
					char *le = evaluate(command, token_starts[0], token_lengths[0]);
					int true_val = atoi(le);
					free(ret);
					free(le);
					
					if(!true_val || loop_exit_requested)
					{
						ret = strdup("");
						break;
					}
					else
					{
						ret = evaluate(command, token_starts[2], token_lengths[2]);
					}

				}
				in_loop = 0;
				loop_exit_requested = 0;
			}
			else
			if(token_lengths[1]==1 && command[token_starts[1]] == ',')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				free(le);
				
				ret = evaluate(command, token_starts[2], token_lengths[2]);
			}
			else
			if(token_lengths[1]==1 && command[token_starts[1]] == '[')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				char *re = evaluate(command, token_starts[2], token_lengths[2]);

				char *le2 = le+atoi(re);
				ret = strdup(le2);
				free(le);
				free(re);
			}
			else
			if(token_lengths[1]==1 && command[token_starts[1]] == '.')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				char *re = evaluate(command, token_starts[2], token_lengths[2]);
			
				int len1 = strlen(le), len2 = strlen(re);
				ret = malloc(1+len1+len2);
				memcpy(ret, le, len1);
				memcpy(ret+len1, re, len2);
				ret[len1+len2] = '\0';
				
				free(le);
				free(re);
			}
		}
		break;
		break;
	}
	
cleanup:
	free(token_starts);
	free(token_lengths);
	return ret ? ret : calloc(1,1);
}

int main()
{
	char command[4096];
	
	
	while(!signal_requested)
	{
		printf(">> ");
		fgets(command, 4096, stdin);
		if(command != strtok(command, "\r\n")) *command = 0;
		debug_printf("GOT: '%s'\n", command);
		old_sighandler = signal(SIGINT, sighandler);
		char *res = evaluate(command, 0, strlen(command));
		signal(SIGINT, old_sighandler);
		printf("<< '%s'\n", res);
		free(res);
	}
}
