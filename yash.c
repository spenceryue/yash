#include <signal.h>			// kill, signal
#include <stdio.h>			// printf, fflush, setvbuf, perror
#include <unistd.h>			// isatty, setpgid, tcgetpgrp, tcsetpgrp, getpgid, getpid
#include <stdlib.h>			// exit, atexit
#include "tokenize.h"
#include "job.h"
#include "job_control.h"
#include "faces.h"
#include <string.h>			// strcmp


void exit_handler ()
{
	for (Job* j = current_Job; j != NULL; j = j->next)
		kill (- j->pgid, SIGHUP);
	destroy_Job(current_Job);
	printf("exit\n");
}


void signal_handler (int signo)
{
	switch(signo)
	{
		case SIGINT:
		case SIGTSTP:
		printf("\n# ");
		fflush(stdout);
	}
}


int main(int argc, char* argv[])
{
	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);


	if (argc == 2 && strcmp(argv[1], "pikachu") == 0)
		fprintf(stderr,
			"\n"
			"         YASH!"
			pikachu "\n"
			"(...and his best friend ^)\n\n"
		);


	if (!isatty(STDIN_FILENO))
	{
		fprintf(stderr, flip_table " yash: abort reason: Job control won't work because yash is not executing from a tty\n");
		exit(1);
	}


	if (setpgid(0,0) == -1)
	{
		perror (flip_table " yash: abort reason: Couldn't put yash in its own process group");
		exit (1);
	}
	// printf("My pid: %d, pgid: %d\n", getpid(), getpgid(0));


	while (tcgetpgrp (STDIN_FILENO) != getpgid(0))
		kill (- getpgid(0), SIGTTIN);


	if (tcsetpgrp(STDIN_FILENO, getpid()) == -1)
	{
		perror(flip_table " yash: abort reason: Couldn't obtain control of the terminal");
		exit (1);
	}


	atexit(exit_handler);


	if (signal(SIGINT, signal_handler) == SIG_ERR)	perror(flip_table " yash: signal");
	if (signal(SIGTSTP, signal_handler) == SIG_ERR) perror(flip_table " yash: signal");

	if (signal (SIGCHLD, SIG_DFL) == SIG_ERR)		perror(flip_table " yash: signal");

	if (signal (SIGQUIT, SIG_IGN) == SIG_ERR)		perror(flip_table " yash: signal");
	if (signal (SIGTTIN, SIG_IGN) == SIG_ERR)		perror(flip_table " yash: signal");
	if (signal (SIGTTOU, SIG_IGN) == SIG_ERR)		perror(flip_table " yash: signal");


	Job* j = NULL;
	while (tcsetpgrp(STDIN_FILENO, getpid()),
		   printf("# "),
		   read_line(stdin) != NULL)
	{
		print_Jobs(0);
		char** tokens = set_tokens(" \t");

		if (no_tokens(tokens))
			continue;

		if (launch_builtin(tokens))
			continue;

		j = make_Job(tokens);
		if (j == NULL)
			continue;
		j->next = current_Job;
		current_Job = j;

		launch_Job(current_Job);
		if (current_Job->foreground)
			wait_Job(current_Job);
	}


	return 0;
}