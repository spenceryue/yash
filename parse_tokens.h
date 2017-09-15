#ifndef PARSE_TOKENS_H
#define PARSE_TOKENS_H
#include <string.h>
#include "tokenize.h"


int get_token_index (char** tokens, const char* key)
{
	if (tokens == NULL)
		return -1;

	for (int i=0; tokens[i] != NULL; i++)
		if (strcmp(key, tokens[i]) == 0)
			return i;

	return -1;
}

char* get_token_after (char** tokens, const char* key)
{
	int index = get_token_index(tokens, key);

	if (index == -1)
		return NULL;
	else
		return tokens[index+1];
}

int first_of_tokens_index (char** tokens, const char** keys)
{
	if (tokens == NULL)
		return -1;

	for (int i=0; tokens[i] != NULL; i++)
		for (int j=0; keys[j] != NULL; j++)
			if (strcmp(keys[j], tokens[i]) == 0)
				return i;

	return -1;
}

char** set_pipe_start (char** tokens)
{
	int index = get_token_index(tokens, "|");

	if (index == -1)
		return NULL;
	else
	{
		tokens[index] = NULL;
		return &tokens[index+1];
	}
}

char* get_redirect_in (char** tokens)
{
	return get_token_after(tokens, "<");
}

char* get_redirect_out (char** tokens)
{
	return get_token_after(tokens, ">");
}

char* get_redirect_error (char** tokens)
{
	return get_token_after(tokens, "2>");
}

int has_ampersand (char** tokens)
{
	return get_token_index(tokens, "&") != -1;
}

int set_args_end (char** tokens)
{
	static const char* special[] = {"<", ">", "2>", "&"};
	int index = first_of_tokens_index(tokens, special);

	if (index != -1)
		tokens[index] = NULL;

	return index;
}

#endif /* PARSE_TOKENS_H */



/* Test PARSE_TOKENS */
#if __INCLUDE_LEVEL__ == 0 && defined __INCLUDE_LEVEL__
#include <stdio.h>

int main(int argc, char* argv[])
{
	if (argc == 1)
		freopen("parse_tokens_input.txt", "r", stdin);

	for (int i=0; read_line(stdin) != NULL; i++) {
		printf("\n%d\n",i);
		char** tokens = get_tokens(" \t");
		char** child_tokens = set_pipe_start(tokens);

		printf("in: '%s'\n", get_redirect_in(tokens));
		printf("out: '%s'\n", get_redirect_out(tokens));
		printf("error: %s\n", get_redirect_error(tokens));
		printf("has ampersand: %d\n", has_ampersand(tokens) ? 1 : 0);
		printf("args end: %d\n", set_args_end(tokens));

		if (child_tokens != NULL) {
			// printf("child_tokens: \n");
			// print_tokens(child_tokens);
			printf("child in: '%s'\n", get_redirect_in(child_tokens));
			printf("child out: '%s'\n", get_redirect_out(child_tokens));
			printf("child error: %s\n", get_redirect_error(child_tokens));
			printf("has ampersand: %d\n", has_ampersand(child_tokens) ? 1 : 0);
			printf("args end: %d\n", set_args_end(child_tokens));
		}
	}
}
#endif
/* Test PARSE_TOKENS */