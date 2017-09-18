#ifndef JOB_CONTROL_H
#define JOB_CONTROL_H


#include <unistd.h>			// fork, pid_t, execvp
#include <signal.h>			// SIGINT, SIGTSTP, signal, SIG_ERR
#include <sys/wait.h>		// wait
#include <stdio.h>			// fprintf, perror
#include <errno.h>			// ECHILD
#include "job.h"
// #define NDEBUG
#include <assert.h>			// assert

int is_State (Job* j, State s)
{
	if (s == Error_State)
	{
		for (Process* p= j->p; p != NULL; p = p->next)
			if (p->state == s)
				return 1;
		return 0;
	}
	else if (s == Running_State)
	{
		for (Process* p= j->p; p != NULL; p = p->next)
			if (p->state != s)
				return 0;
		return 1;
	}
	else
	{
		for (Process* p= j->p; p != NULL; p = p->next)
			if (p->state < s)
			{
				// fprintf(stderr, "Job (%s) not %s: %s\n", j->command, get_state_string(s), get_state_string(p->state));
				return 0;
			}
		return 1;
	}
}

int is_Error (Job* j)
{
	return is_State(j, Error_State);
}

int is_Running (Job* j)
{
	return is_State(j, Running_State);
}

int is_Stopped (Job* j)
{
	return is_State(j, Stopped_State);
}

int is_Done (Job* j)
{
	return is_State(j, Done_State);
}

Process* find_Process (pid_t pid)
{
	for (Job* j = current_Job; j != NULL; j = j->next)
		for (Process* p = j->p; p != NULL; p = p->next)
			if (p->pid == pid)
				return (Process*) p;

	return NULL;
}

void update_Process (Process* p, int status)
{
	assert (p != NULL);

	if (WIFSTOPPED(status))
	{
		// fprintf(stderr, "Marking (%d): %s\n", p->pid, get_state_string(Stopped_State));
		p->state = Stopped_State;
	}
	else if (WIFEXITED(status) || WIFSIGNALED(status))
	{
		// fprintf(stderr, "Marking (%d): %s\n", p->pid, get_state_string(Done_State));
		p->state = Done_State;
	}
	else
	{
		// fprintf(stderr, "Marking (%d): %s\n", p->pid, get_state_string(Error_State));
		p->state = Error_State;
	}
}

void get_Job_status (Job* j, int WAIT)
{
	if (j == NULL)
		return;

	pid_t pid;
	int status;
	int options = (WAIT) ? WUNTRACED : WUNTRACED|WNOHANG;

	while (!is_Stopped(j))
	{
		pid = waitpid(WAIT_ANY, &status, options);

		if ((!WAIT && pid == 0) ||
			(pid == -1 && errno == ECHILD))
			return; // Still Running

		if (pid == -1)
		{
			perror("yash: waitpid");
			return;
		}

		if (WIFSIGNALED(status))
		{
			printf("killed by signal %d\n", WTERMSIG(status));
			// j->foreground = 0; // hacky way to make the Job print
							   // (should make a dedicated display variable...)
		}

		Process* p = find_Process(pid);
		update_Process(p, status);
		if (p->state == Error_State)
		{
			j->state = Error_State;
			return;
		}
	}

	if (is_Done(j))
		j->state = Done_State;
	else
		j->state = Stopped_State;
}

void update_Job (Job* j)
{
	get_Job_status(j, 0);
}

void wait_Job (Job* j)
{
	get_Job_status(j, 1);
}

void update_Jobs ()
{
	for (Job* j = current_Job; j != NULL; j = j->next)
		update_Job(j);
}

void print_Job_Compared (Job* j, Job* current)
{
	printf("[%d]%c  %-24s%s\n",
			j->index,
			(j == current) ? '+' : '-',
			get_state_string(j->state),
			j->command);
}

void print_Job (Job* j)
{
	print_Job_Compared(j, current_Job);
}

void print_Jobs (int LIST_ALL)
{
	update_Jobs();

	Job* saved_current_Job = current_Job;

	Job* last = NULL;
	Job *j = current_Job;
	while (j != NULL)
	{
		switch (j->state)
		{
			case Running_State:
			case Stopped_State:
				if (LIST_ALL)
					print_Job_Compared(j, saved_current_Job);
				last = j;
				j = j->next;
				break;
			case Error_State:
			case Done_State:
				if (LIST_ALL || !j->foreground)
					print_Job_Compared(j, saved_current_Job);
				if (last != NULL)
				{
					last->next = j->next;
					destroy_Job(j);
					j = last;
				}
				else
				{
					current_Job = j->next;
					destroy_Job(j);
					j = current_Job;
				}
		}
	}
}

void mark_Processes (Job* j, State s)
{
	for (Process* p = j->p; p != NULL; p = p->next)
		p->state = s;
}

void fg ()
{
	Job* j;

	for (j = current_Job; j != NULL; j = j->next)
	{
		if (j->state == Running_State)
			if (!j->foreground)
				break;
		if (j->state == Stopped_State)
			break;
	}

	if (j == NULL)
	{
		fprintf(stderr, "yash: fg: current: no such job\n");
		return;
	}

	j->foreground = 1;
	j->state = Running_State;
	mark_Processes(j, Running_State);

	print_Job(j);
	tcsetpgrp(STDIN_FILENO, current_Job->pgid);
	kill(-current_Job->pgid, SIGCONT);
	wait_Job(current_Job);
}

void bg ()
{
	Job* j;

	for (j = current_Job; j != NULL; j = j->next)
		if (j->state == Stopped_State)
			break;

	if (j == NULL)
	{
		fprintf(stderr, "yash: bg: current: no such job\n");
		return;
	}

	j->foreground = 0;
	j->state = Running_State;
	mark_Processes(j, Running_State);

	print_Job(j);
	kill(-current_Job->pgid, SIGCONT);
}

int launch_builtin (char** tokens)
{
	static const char* special[] = {"fg", "bg", "jobs", "exit"};

	if (!no_tokens(tokens+1))
		return 0;

	if(strcmp(tokens[0], special[0]) == 0)
		fg();
	else if(strcmp(tokens[0], special[1]) == 0)
		bg();
	else if(strcmp(tokens[0], special[2]) == 0)
		print_Jobs(1);
	else if(strcmp(tokens[0], special[3]) == 0)
		exit(0);
	else
		return 0;

	return 1;
}


#endif /* JOB_CONTROL_H */



/* Test JOB_CONTROL */
#if __INCLUDE_LEVEL__ == 0 && defined __INCLUDE_LEVEL__
#include <stdio.h>			// setvbuf, freopen, printf
#include <fcntl.h>			// open
#include <unistd.h>			// usleep, close
#include "tokenize.h"
#include "job.h"

void exit_handler ()
{
	for (Job* j = current_Job; j != NULL; j = j->next)
		kill (- j->pgid, SIGHUP);
	destroy_Job(current_Job);
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

	if (!isatty(STDIN_FILENO))
	{
		fprintf(stderr, "yash: abort reason: Job control won't work because yash is not executing from a tty\n");
		exit(1);
	}

	if (setpgid(0,0) == -1)
	{
		perror ("yash: abort reason: Couldn't put yash in its own process group");
		exit (1);
	}
	// printf("My pid: %d, pgid: %d\n", getpid(), getpgid(0));

	while (tcgetpgrp (STDIN_FILENO) != getpgid(0))
		kill (- getpgid(0), SIGTTIN);


	tcsetpgrp(STDIN_FILENO, getpid());

	atexit(exit_handler);
	if (signal(SIGINT, signal_handler) == SIG_ERR)
		perror("yash: signal");
	if (signal(SIGTSTP, signal_handler) == SIG_ERR)
		perror("yash: signal");

	if (signal (SIGCHLD, SIG_DFL) == SIG_ERR)
		perror("yash: signal");

	if (signal (SIGQUIT, SIG_IGN) == SIG_ERR)
		perror("yash: signal");
	if (signal (SIGTTIN, SIG_IGN) == SIG_ERR)
		perror("yash: signal");
	if (signal (SIGTTOU, SIG_IGN) == SIG_ERR)
		perror("yash: signal");


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
	printf("exit\n");

	return 0;
}
#endif
/* Test JOB_CONTROL */