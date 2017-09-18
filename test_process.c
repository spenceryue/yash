#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include <signal.h>			// SIGINT, SIGTSTP, signal, SIG_ERR
#include <unistd.h>


double GetTime() {
	struct timeval t;
	int rc = gettimeofday(&t, NULL);
	assert(rc == 0);
	return (double)t.tv_sec + (double)t.tv_usec/1e6;
}

void Spin(int howlong) {
	double t = GetTime();
	while ((GetTime() - t) < (double)howlong)
	; // do nothing in loop
}

void handler (int signum)
{
	signal (SIGTSTP, SIG_DFL);

	raise(SIGTSTP);
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);

	while (tcgetpgrp (STDIN_FILENO) != getpgid(0))
		kill (- getpgid(0), SIGTTIN);

	struct sigaction temp;

	sigaction (SIGTSTP, NULL, &temp);

	if (temp.sa_handler != SIG_DFL)
	{
		fprintf(stderr, "%s\n", "Warning! sigtstp isn't default handled.");
	}
	else
		fprintf(stderr, "%s\n", "All happy!");

	char* str;
	if (argc == 1)
		str = "A";
	else
		str = argv[1];

	while (printf("%s ", str), Spin(1), 1);

	return 0;
}

