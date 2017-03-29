#define _GNU_SOURCE
#include <stdio.h>
#include <ctype.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <signal.h>


#define DEBUG_PRINTF

#define _GNU_SOURCE

#ifdef DEBUG_PRINTF
#define debug_printf printf
#else
#define debug_printf(s, ...) do { } while ( 0 )
#endif

#ifdef DEBUG_TOKENS
#define debug_tokens debug_tokens_fun
#else
#define debug_tokens(s, ...) do { } while ( 0 )
#endif


// for now variables and functions are treated the same way!
struct {
	struct {
		char *name;
		char *value;
#ifdef DEBUG_PERF
		int no_writes;
		int no_reads;
#endif
	} *namevalue_pairs;
	int no;
} vars;

struct {
	struct {
		char *name;
		char *value;
#ifdef DEBUG_PERF
		int no_writes;
		int no_reads;
#endif
	} *namevalue_pairs;
	int no;
} opers;

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
#ifdef DEBUG_PERF
	vars.namevalue_pairs[i].no_writes++;
#endif
}

void delvar(const char *name) {
    // for now we like it
    setvar(name,0,strlen(name),"", 0, 0);
}

void setoper(const char *name, int name_start, int name_len, const char *value, int value_start, int value_len)
{
	char *v = malloc(1+value_len);
	memcpy(v, value+value_start, value_len);
	v[value_len] = '\0';

	int i;
	for(i = 0; i < opers.no; i++)
		if(   strlen(opers.namevalue_pairs[i].name) == name_len
		   && memcmp(opers.namevalue_pairs[i].name, name+name_start, name_len) == 0)
		{
			free(opers.namevalue_pairs[i].value);
			goto set_value;
		}

	char *n = malloc(1+name_len);
	memcpy(n, name+name_start, name_len);
	n[name_len] = '\0';

	opers.namevalue_pairs = realloc(opers.namevalue_pairs, ++opers.no * sizeof(*(opers.namevalue_pairs)));
	opers.namevalue_pairs[i].name = n;
set_value:
	opers.namevalue_pairs[i].value = v;
#ifdef DEBUG_PERF
	opers.namevalue_pairs[i].no_writes++;
#endif
}

char *getvar(const char *name, int name_start, int name_len)
{
	for(int i = 0; i < vars.no; i++)
		if(   strlen(vars.namevalue_pairs[i].name) == name_len
		   && memcmp(vars.namevalue_pairs[i].name, name+name_start, name_len) == 0) {
#ifdef DEBUG_PERF
			vars.namevalue_pairs[i].no_reads++;
#endif
			return vars.namevalue_pairs[i].value;
		}
	
	return NULL;
}

char *getoper(const char *name, int name_start, int name_len)
{
	for(int i = 0; i < opers.no; i++)
		if(   strlen(opers.namevalue_pairs[i].name) == name_len
		   && memcmp(opers.namevalue_pairs[i].name, name+name_start, name_len) == 0) {
#ifdef DEBUG_PERF
			opers.namevalue_pairs[i].no_reads++;
#endif
			return opers.namevalue_pairs[i].value;
		}

	return NULL;
}


//char *eval_function

void dumpvar()
{
	for(int i = 0; i < vars.no; i++) {
		printf("%s: '%s'\n", vars.namevalue_pairs[i].name, vars.namevalue_pairs[i].value);
#ifdef DEBUG_PERF
		printf("%s: '%s' (reads/writes: %d/%d)\n", vars.namevalue_pairs[i].name, vars.namevalue_pairs[i].value,
				vars.namevalue_pairs[i].no_reads, vars.namevalue_pairs[i].no_writes);
#endif
	}
}

void dumpoper()
{
	for(int i = 0; i < opers.no; i++) {
		printf("%s: '%s'\n", opers.namevalue_pairs[i].name, opers.namevalue_pairs[i].value);
#ifdef DEBUG_PERF
		printf("%s: '%s' (reads/writes: %d/%d)\n", opers.namevalue_pairs[i].name, opers.namevalue_pairs[i].value,
				opers.namevalue_pairs[i].no_reads, opers.namevalue_pairs[i].no_writes);
#endif
	}
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

		// condition for "new token"
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

			if(command[current_start] != ' ')
			{
				*token_starts = realloc(*token_starts, ++*no_tokens * sizeof(int));
				(*token_starts)[*no_tokens-1] = current_start;
				*token_lengths = realloc(*token_lengths, *no_tokens * sizeof(int));
				(*token_lengths)[*no_tokens-1] = i - current_start;
#ifdef DEBUG_TOKENS
				debug_printf("(%d,%d)\n",current_start, i-current_start);
#endif
			}

			current_start = i;
		}
		
store_char_and_continue:
		last_char = command[i];
	}
	
	*token_starts = realloc(*token_starts, ++*no_tokens * sizeof(int));
	(*token_starts)[*no_tokens-1] = current_start;
	*token_lengths = realloc(*token_lengths, *no_tokens * sizeof(int));
	(*token_lengths)[*no_tokens-1] = i - current_start;
#ifdef DEBUG_TOKENS
    debug_printf("*(%d,%d)\n",current_start, i-current_start);
#endif
}

void debug_tokens_fun(char *command, int no_tokens, int *token_starts, int *token_lengths)
{
	for(int i = 0; i < no_tokens; i++)
	{
		printf("token %d: (%d,%d)'", i, token_starts[i], token_lengths[i]);
#ifdef DEBUG_PRINTF
		putsubstr(command, token_starts[i], token_lengths[i]);
#endif
		printf("'\n");
	}
}

int isoper(char *command, int start, int len)
{
	if(!isalpha(command[start]) && !isdigit(command[start]) && command[start] != '(') {
		// todo what a terrible waste of memory!
		return 1;
	}
	if(getoper(command, start, len) != NULL)
		return 1;
	return 0;
}

void operator_priority(char *command, int no_tokens, int *token_starts, int *token_lengths, int **oper_prio)
{
	*oper_prio = calloc(sizeof(int), no_tokens);
	
	// pre: first token is not operator!
	for(int i = 0; i < no_tokens; i++)
	{
		if(!isoper(command, token_starts[i], token_lengths[i]))
			(*oper_prio)[i] = 1e9;
		else
		{
			if(command[token_starts[i]] == '|' || command[token_starts[i]] == '?')
				(*oper_prio)[i] = 1000;
			else if(command[token_starts[i]] == '=' || command[token_starts[i]] == ':')
				(*oper_prio)[i] = 100;
			else if(command[token_starts[i]] == '.')
				(*oper_prio)[i] = 500;
			else if(command[token_starts[i]] == '[' || command[token_starts[i]] == ']')
				(*oper_prio)[i] = 500;
			else if(command[token_starts[i]] == '*' || command[token_starts[i]] == '/' || command[token_starts[i]] == '%')
				(*oper_prio)[i] = 500;
			else if(command[token_starts[i]] == '+' || command[token_starts[i]] == '-')
				(*oper_prio)[i] = 400;
			else if(command[token_starts[i]] == '<' || command[token_starts[i]] == '>')
				(*oper_prio)[i] = 300;
			else if(command[token_starts[i]] == ',')
				(*oper_prio)[i] = 50;
			else
				(*oper_prio)[i] = 10;

			if(isoper(command, token_starts[i-1], token_lengths[i-1]))
			{
				(*oper_prio)[i-1] += 1000; // last was one-argument op
			}
			
			if(i == no_tokens-1) (*oper_prio)[i] = 1000; // one-argument op	
		}
	}
}

char *evaluate(char *command, int start, int len)
{
        static int debug_recur = 0;
        static char debug_padding[] = "|||||||||||||||||||||||||||||||||||||||";
        debug_recur++;
	int no_tokens, *token_starts, *token_lengths;
	char *ret = NULL;
	debug_printf("%.*sevaluate: \"%.*s\"        [%p + %d]\n", debug_recur, debug_padding, len, command+start, command, len);
	parse(command, start, len, &no_tokens, &token_starts, &token_lengths);
	debug_tokens(command, no_tokens, token_starts, token_lengths);

	if(no_tokens > 3 || (no_tokens == 3 && isoper(command, token_starts[2], token_lengths[2])))
	{
		int *oper_prio = NULL;
		operator_priority(command, no_tokens, token_starts, token_lengths, &oper_prio);
		// let's find biggest prio
		int minprio = 1e9, minprio_index = -1;
		for(int i = 0; i < no_tokens; i++)
			if(oper_prio[i] <= minprio)
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
			new_token_lengths[2] = len - (new_token_lengths[0]+new_token_lengths[1]);
		}
			
		free(token_lengths);
		free(token_starts);
			
		token_starts = new_token_starts;
		token_lengths = new_token_lengths;
	}

#ifdef DEBUG_TOKENS
	debug_printf("AFTER MERGE\n");
#endif
	debug_tokens(command, no_tokens, token_starts, token_lengths);
	
	if(signal_requested)
	{
		signal_requested = 0;
		printf("Interrupt: exit last frame.\n");
		if(in_loop) loop_exit_requested = 1;
                debug_recur--;
		return strdup("0");
	}
	
back_after_join:
	switch(no_tokens) {
		
		case 1:
		{
			// good: T
			// bad:  *
			if(isoper(command, token_starts[0], token_lengths[0]))
			{
				printf("ERROR has occured => 1 arg * (operator only)!\n");
			}
			if(command[token_starts[0]] == '(' && command[token_starts[0]+token_lengths[0]-1] == ')')
			{
			//	debug_printf("!!!!! %d %d !!!!!!\n", token_starts[0]+1, token_lengths[0]-2);
				ret = evaluate(command, token_starts[0]+1, token_lengths[0]-2);
			}
			else
			{
				// do protect numbers from overwrite :)
				char *val = getvar(command, start, len);
				if(val)
				{
					ret = strdup(val);
				}
				else
				{
					ret = malloc(1+len);
					memcpy(ret, command+start, len);
					ret[len] = '\0';
				}
			}
		}
		break;
		case 2:
		{
			if(isoper(command, token_starts[0], token_lengths[0]))
			{
				printf("ERROR has occured => 2 arg *T or ** (operator first) !\n");
				ret = strdup("");
				break;
			}
			if(!isoper(command, token_starts[1], token_lengths[1]))
			{
				printf("ERROR has occured => 2 arg TT (no operator) !\n");
				ret = strdup("");
				break;				
			}
			
			if(token_lengths[1]==1 && command[token_starts[1]] == '~')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				int val = atoi(le);
				if(val == 0)
					ret = strdup("1");
				else
					ret = strdup("0");

				free(le);
			}
			else if(token_lengths[1]==1 && command[token_starts[1]] == '~')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				int val = atoi(le);
				if(val == 0)
					ret = strdup("1");
				else
					ret = strdup("0");

				free(le);
			}
			else if(token_lengths[1]==1 && command[token_starts[1]] == '\'')
			{
                                // eval-haneun got!
				ret = malloc(token_lengths[0]+1);
				memcpy(ret, command+token_starts[0], token_lengths[0]);
                                ret[token_lengths[0]] = '\0';
                            //ret = strdup("OPERATOR_WITH_NO_MEANING");
			}
			else if(token_lengths[1]==1 && command[token_starts[1]] == '#')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				char buf[16];
				sprintf(buf, "%d", strlen(le));
				ret = strdup(buf);
			}
			else if(token_lengths[1]==1 && command[token_starts[1]] == '*')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
                                char *nexte = evaluate(le, 0, strlen(le));
				ret = nexte;
                                free(le);
			}
			else if(token_lengths[1]==1 && command[token_starts[1]] == '>')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				printf("%s\n", le);
				ret = strdup(le);
			}
			else if(token_lengths[1]==1 && command[token_starts[1]] == '<')
			{
				ret = NULL;
				int size;
				getline(&ret, &size, stdin);
				strtok(ret, "\r\n");

				setvar(command, token_starts[0], token_lengths[0],
						ret, 0, strlen(ret)); // todo subopt
			}
		}
		break;
		case 3:
		{
			if(isoper(command, token_starts[0], token_lengths[0]))
			{
				printf("ERROR has occured => 3 arg *xx (operator first) !\n");
				break;
			}
			else if(!isoper(command, token_starts[1], token_lengths[1]))
			{
				printf("ERROR has occured => 3 arg TTx (second is not operator) !\n");
				break;
			}
			if(token_lengths[1]==1 && command[token_starts[1]] == '=')
			{
				char *re = evaluate(command, token_starts[2], token_lengths[2]);
				int len = strlen(re);
			
				setvar(command, token_starts[0], token_lengths[0],
				       re, 0, len);
				
				ret = malloc(1+len);
				memcpy(ret, re, len);
				ret[len] = '\0';
				free(re);
			}
			else if(token_lengths[1]==1 && command[token_starts[1]] == ':')
			{

				setoper(command, token_starts[0], token_lengths[0],
				       command, token_starts[2], token_lengths[2]);

				ret = malloc(1+token_lengths[2]);
				memcpy(ret, command+token_starts[2], token_lengths[2]);
				ret[token_lengths[2]] = '\0';
			}
			else
			if(token_lengths[1]==1 && command[token_starts[1]] == '|')
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
			if(token_lengths[1]==1 && command[token_starts[1]] == '?')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				int true_val = atoi(le);
				free(ret);
				free(le);
					
				if(!true_val)
				{
					ret = strdup("");
					break;
				}
				else
				{
					ret = evaluate(command, token_starts[2], token_lengths[2]);
				}
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
			if(token_lengths[1]==1 && command[token_starts[1]] == ']')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				char *re = evaluate(command, token_starts[2], token_lengths[2]);

				le[atoi(re)] = '\0';
				ret = strdup(le);
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
			else if(token_lengths[1]==1 && command[token_starts[1]] == '+')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				char *re = evaluate(command, token_starts[2], token_lengths[2]);

				int res = atoi(le) + atoi(re);
				char _resbuf[16];
				sprintf(_resbuf, "%d", res);

				ret = strdup(_resbuf);
			}
			else if(token_lengths[1]==1 && command[token_starts[1]] == '-')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				char *re = evaluate(command, token_starts[2], token_lengths[2]);

				int res = atoi(le) - atoi(re);
				char _resbuf[16];
				sprintf(_resbuf, "%d", res);

				ret = strdup(_resbuf);
			}
			else if(token_lengths[1]==1 && command[token_starts[1]] == '*')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				char *re = evaluate(command, token_starts[2], token_lengths[2]);

				int res = atoi(le) * atoi(re);
				char _resbuf[16];
				sprintf(_resbuf, "%d", res);

				ret = strdup(_resbuf);
			}
			else if(token_lengths[1]==1 && command[token_starts[1]] == '/')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				char *re = evaluate(command, token_starts[2], token_lengths[2]);

				int res = atoi(le) / atoi(re);
				char _resbuf[16];
				sprintf(_resbuf, "%d", res);

				ret = strdup(_resbuf);
			}
			else if(token_lengths[1]==1 && command[token_starts[1]] == '%')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				char *re = evaluate(command, token_starts[2], token_lengths[2]);

				int res = atoi(le) % atoi(re);
				char _resbuf[16];
				sprintf(_resbuf, "%d", res);

				ret = strdup(_resbuf);
			}
			else if(token_lengths[1]==1 && command[token_starts[1]] == '<')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				char *re = evaluate(command, token_starts[2], token_lengths[2]);

				if(atoi(le) < atoi(re))
					ret = strdup("1");
				else
					ret = strdup("0");
			}
			else if(token_lengths[1]==1 && command[token_starts[1]] == '>')
			{
				char *le = evaluate(command, token_starts[0], token_lengths[0]);
				char *re = evaluate(command, token_starts[2], token_lengths[2]);

				if(atoi(le) > atoi(re))
					ret = strdup("1");
				else
					ret = strdup("0");
			}
			else {
				// todo restore vars!
				char *lsave = getvar("l", 0, 1);
				char *rsave = getvar("r", 0, 1);
//				char *Lsave = getvar("L", 0, 1);
//				char *Rsave = getvar("R", 0, 1);
                                
                                if(lsave) lsave = strdup(lsave);
                                if(rsave) rsave = strdup(rsave);
 //                               if(Lsave) Lsave = strdup(Lsave);
  //                              if(Rsave) Rsave = strdup(Rsave);
                                
				setvar("l", 0, 1, command, token_starts[0], token_lengths[0]);
				setvar("r", 0, 1, command, token_starts[2], token_lengths[2]);
  //                              delvar("L"); // todo should not be made up unless existed already
  //                              delvar("R");
				char *oper = getoper(command, token_starts[1], token_lengths[1]);
				ret = evaluate(oper, 0, strlen(oper));
//                                char *newl = getvar("L", 0, 1);
 //                               char *newr = getvar("R", 0, 1);
				if(lsave) {
					setvar("l", 0, 1, lsave, 0, strlen(lsave));
					free(lsave);
				}
				if(rsave) {
					setvar("r", 0, 1, rsave, 0, strlen(rsave));
					free(rsave);
				}
/*				if(Lsave) {
					setvar("L", 0, 1, Lsave, 0, strlen(lsave));
					free(Lsave);
				}
				if(Rsave) {
					setvar("R", 0, 1, Rsave, 0, strlen(rsave));
					free(Rsave);
				}*/
   /*                             if(newl && *newl) {
					setvar(command, token_starts[0], token_lengths[0], newl, 0, strlen(newl));
                                }
                                if(newr && *newr) {
					setvar(command, token_starts[2], token_lengths[2], newr, 0, strlen(newr));
                                } */
                                
			}
		}
		break;
	}
        
        debug_printf("%.*s<= '%s'\n", debug_recur, debug_padding, ret);
        debug_recur--;
	
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
		old_sighandler = signal(SIGINT, sighandler);
		if(command[0] == ':') {
			if(strcmp(command, ":vars") == 0) {
				dumpvar();
				continue;
			}
			else if(strcmp(command, ":opers") == 0) {
				dumpoper();
				continue;
			}
		}
		char *res = evaluate(command, 0, strlen(command));
		//char *res = strdup("");
		signal(SIGINT, old_sighandler);
		printf("<< '%s'\n", res);
		free(res);
	}
}
