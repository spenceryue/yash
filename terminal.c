#include <stdio.h>			// printf, fgets
#include <stdlib.h>			// exit
#include <unistd.h>			// fork, pid_t, execlp
#include <sys/wait.h>		// wait
#include <signal.h>			// SIGINT, SIGTSTP, signal, SIG_ERR
#include "tokenize.h"		// read_line, get_tokens, count_tokens

#include <errno.h>
// extern int errno;

static void signal_handler (int signo)
{
	switch(signo)
	{
		case SIGINT:
		printf("\n# ");
		fflush(stdout);
		// printf("caught SIGINT\n");
		// exit(0);
		break;

		case SIGTSTP:
		printf("\n# ");
		fflush(stdout);
		// printf("caught SIGTSTP\n");
		break;
	}
}

int main(void)
{
	if (signal(SIGINT, signal_handler) == SIG_ERR)
		printf("signal(SIGINT) error");
	if (signal(SIGTSTP, signal_handler) == SIG_ERR)
		printf("signal(SIGTSTP) error");

	pid_t pid;
	int status;

	do {
		printf("# ");
		if (read_line(stdin) == NULL) // "^D" (EOF) entered
			break;

		char** tokens = get_tokens("\t ");
		if (count_tokens(tokens) == 0)
			continue;

		pid = fork();
		if (pid  == -1)
			perror("fork error");

		/* child */
		else if (pid == 0) {
			execvp(tokens[0], tokens);

			if (errno == ENOENT)
				fprintf(stderr, "%s: command not found\n", tokens[0]);
			else
			{
				fprintf(stderr, "yash: exec: ");
				perror(tokens[0]);
			}

			exit(1);
		}

		/* parent */
		else do {
            pid = waitpid(pid, &status, WUNTRACED | WCONTINUED);
            if (pid == -1) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            }

           if (WIFEXITED(status)) {
                printf("exited, status=%d\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("killed by signal %d\n", WTERMSIG(status));
            } else if (WIFSTOPPED(status)) {
                printf("stopped by signal %d\n", WSTOPSIG(status));
            } else if (WIFCONTINUED(status)) {
                printf("continued\n");
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

		// if ((pid = waitpid(pid, &status, 0)) == -1)
		// 	perror("waitpid error");
	} while(1);

	printf("\n");
	exit(0);
}