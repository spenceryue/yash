#ifndef TOKENIZE_H
#define TOKENIZE_H

#include <stdio.h>			// FILE, printf
#include <stdlib.h>			// malloc
#include <string.h>			// strtok, strchr

#define MAX_CHARS 2000
#define MAX_TOKENS MAX_CHARS/2
static char input_buffer[MAX_CHARS+1] = {0};
static char* token_array[MAX_TOKENS+1] = {0};


char* read_line (FILE* input_stream)
{
	char* response = fgets(input_buffer, MAX_CHARS, input_stream);
	if (response == NULL)
		return NULL;

	char* end = strchr(input_buffer, '\n');
	if (end == NULL) { // example: "my_command arg0 arg1 ar^D" (EOF encountered, line non-empty)
		input_buffer[0] = 0;
		putchar('\n');
	}
	else
		*end = 0; // remove new line character from input_buffer

	return response;
}

char** set_tokens (const char* delimiters)
{
	token_array[0] = strtok(input_buffer, delimiters);
	if (token_array[0] == NULL)
		return token_array;

	for (int i=1; i<MAX_TOKENS; i++)
	{
		token_array[i] = strtok(NULL, delimiters);

		if (token_array[i] == NULL)
			break;
	}

	return token_array;
}

int count_tokens(char** tokens)
{
	if (tokens == NULL)
		return -1;

	int count = 0;
	while (tokens[count] != NULL)
		count++;

	return count;
}

int no_tokens(char** tokens)
{
	if (tokens == NULL)
		return 1;

	return tokens[0] == NULL;
}

char* concat_tokens (char** tokens, const char* sep)
{
	if (tokens == NULL)
		return NULL;

	int N = count_tokens(tokens);
	if (N == 0)
		return NULL;

	size_t* length = (size_t*) malloc(N * sizeof(size_t));
	size_t sep_len = strlen(sep);
	size_t final_len = 0;

	for (int i=0; i < N; i++)
	{
		length[i] = strlen(tokens[i]);
		final_len += length[i] + sep_len;
	}
	final_len = final_len - sep_len + 1;

	char* result = (char*) malloc(final_len * sizeof(char));

	for (int i=0, j=0; i < N; i++)
	{
		strcpy(&result[j], tokens[i]);
		j += length[i];
		if (i < N-1)
		{
			strcpy(&result[j], sep);
			j += sep_len;
		}
	}

	free(length);

	return result;
}

void print_tokens(char** tokens)
{
	if (tokens == NULL)
		return;

	char* str = concat_tokens(tokens, "\", \"");
	printf("(%d) \t[\"%s\"]\n", count_tokens(tokens), str ? str : "");
	free(str);
}

#endif /* TOKENIZE_H */



/* Test TOKENIZE */
#if __INCLUDE_LEVEL__ == 0 && defined __INCLUDE_LEVEL__
#include <stdio.h>
#include <stdlib.h>
#include "parse_tokens.h"
#include <unistd.h>
#include <signal.h>
int main(int argc, char* argv[])
{
	printf("%d %d\n", MAX_CHARS, MAX_TOKENS);
	char* name = ttyname(STDIN_FILENO);
	printf("%s\n",(name != NULL) ? name : "what");
	printf("%d\n",isatty(STDIN_FILENO));

	if (argc == 1)
		freopen("parse_tokens_input.txt", "r", stdin);

	if (read_line(stdin) == NULL) {
		fprintf(stderr, "Gotta send some input.\n");
		exit(1);
	}

	char** tokens = set_tokens(" \t");
	print_tokens(tokens);
	char** child_tokens = set_pipe_start(tokens);

	printf("\n%d\n", 0);
	get_redirect_in(tokens);
	get_redirect_out(tokens);
	get_redirect_error(tokens);
	has_ampersand(tokens);
	set_args_end(tokens);

	get_redirect_in(child_tokens);
	get_redirect_out(child_tokens);
	get_redirect_error(child_tokens);
	has_ampersand(child_tokens);
	set_args_end(child_tokens);

	print_tokens(tokens);
	print_tokens(child_tokens);

	for (int i=1; read_line(stdin) != NULL; i++) {
		printf("\n%d\n", i);
		tokens = set_tokens(" \t");
		print_tokens(tokens);
		child_tokens = set_pipe_start(tokens);

		get_redirect_in(tokens);
		get_redirect_out(tokens);
		get_redirect_error(tokens);
		has_ampersand(tokens);
		set_args_end(tokens);

		get_redirect_in(child_tokens);
		get_redirect_out(child_tokens);
		get_redirect_error(child_tokens);
		has_ampersand(child_tokens);
		set_args_end(child_tokens);

		print_tokens(tokens);
		print_tokens(child_tokens);
	}

	return 0;
}
#endif
/* Test TOKENIZE */