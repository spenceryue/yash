/*input
ls
*/
#include <signal.h>			// kill, signal
#include <stdio.h>			// printf, fflush, setvbuf, perror
#include <unistd.h>			// isatty, setpgid, tcgetpgrp, tcsetpgrp, getpgid, getpid
#include <stdlib.h>			// exit, atexit
#include "tokenize.h"
#include "job.h"
#include "job_control.h"
#include "faces.h"


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


int prompt ()
{
	print_Jobs(0);
	tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_tmodes); // restore shell terminal modes
	tcsetpgrp(STDIN_FILENO, shell_pid);

	printf("# ");
	return read_line(stdin) != NULL;
}


int main(int argc, char* argv[])
{
	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
	shell_pid = getpid();


/*	if (!isatty(STDIN_FILENO))
	{
		fprintf(stderr, blank_face " Warning: isatty: Job control won't work because yash is not executing from a tty\n");
		// return 0;
	}


	if (setpgid(0,0) == -1)
	{
		perror (blank_face " Warning: setpgid: Couldn't put yash in its own process group");
		// return 0;
	}
	// printf("My pid: %d, pgid: %d\n", getpid(), getpgid(0));


	for (int tries=0; tcgetpgrp (STDIN_FILENO) != getpgid(0); tries++)
	{
		if (tries > 40)
		{
			fprintf(stderr, blank_face " Warning: SIGTTIN: Couldn't put yash in foreground\n");
			break;
		}
		kill (- getpgid(0), SIGTTIN);
		usleep(50000);
	}


	if (tcsetpgrp(STDIN_FILENO, getpid()) == -1)
	{
		perror(blank_face " Warning: tcsetpgrp: Couldn't obtain control of the terminal");
		// return 0;
	}*/


	tcgetattr (STDIN_FILENO, &shell_tmodes);


	atexit(exit_handler);


	if (signal(SIGINT, signal_handler) == SIG_ERR)	perror(blank_face " yash: signal");
	if (signal(SIGTSTP, signal_handler) == SIG_ERR) perror(blank_face " yash: signal");

	if (signal (SIGCHLD, SIG_DFL) == SIG_ERR)		perror(blank_face " yash: signal");

	if (signal (SIGQUIT, SIG_IGN) == SIG_ERR)		perror(blank_face " yash: signal");
	if (signal (SIGTTIN, SIG_IGN) == SIG_ERR)		perror(blank_face " yash: signal");
	if (signal (SIGTTOU, SIG_IGN) == SIG_ERR)		perror(blank_face " yash: signal");

	Job* j = NULL;
	while (prompt())
	{
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
