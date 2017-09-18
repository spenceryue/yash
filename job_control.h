#ifndef JOB_CONTROL_H
#define JOB_CONTROL_H


#include <unistd.h>			// fork, pid_t, execvp
#include <signal.h>			// SIGINT, SIGTSTP, signal, SIG_ERR
#include <sys/wait.h>		// wait
#include <stdio.h>			// fprintf, perror
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
				fprintf(stderr, ">>>pid not %s: %d, state: %s\n", get_state_string(s), p->pid, get_state_string(p->state));
				return 0;
			}
		fprintf(stderr, "<<<job is %s: %d\n", get_state_string(s), j->index);
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
		{fprintf(stderr, "<><><><>marking process (%d) as %s\n", p->pid, get_state_string(Stopped_State));
		p->state = Stopped_State;
		}
	else if (WIFEXITED(status))
		{fprintf(stderr, "<><><><>marking process (%d) as %s\n", p->pid, get_state_string(Done_State));
		p->state = Done_State;
		}
	else
		{fprintf(stderr, "<><><><>marking process (%d) as %s\n", p->pid, get_state_string(Error_State));
		p->state = Error_State;
		}
}

void get_Job_status (Job* j, int WAIT)
{
	if (j == NULL)
		return;

	pid_t pid;
	int status;
	int options = (WAIT) ? WUNTRACED : WUNTRACED|WNO_HANG;

	while (!is_Stopped(j))
	{
		pid = waitpid(WAIT_ANY, &status, options);

		if (!WAIT && pid == 0)
			return; // Still Running

		if (pid == -1)
		{
			perror("yash: waitpid");
			return;
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

void print_Job (Job* j)
{
	printf("[%d]%c  %-24s%s\n",
			j->index,
			(j == current_Job) ? '+' : '-',
			get_state_string(j->state),
			j->command);
}

void print_updates ()
{
	update_Jobs();

	Job* last = NULL;
	for (Job *j = current_Job; j != NULL; j = j->next)
	{
		last = j;

		switch (j->state)
		{
			case Running_State:
			case Stopped_State:
				continue;
			case Error_State:
			case Done_State:
				print_Job(j);
				if (last)
					last->next = j->next;
				else
					current_Job = j->next;
				destroy_Job(j);
		}
	}
}

void print_Jobs ()
{
	update_Jobs();

	Job* last = NULL;
	for (Job *j = current_Job; j != NULL; j = j->next)
	{
		last = j;

		print_Job(j);
		switch (j->state)
		{
			case Running_State:
			case Stopped_State:
				break;
			case Error_State:
			case Done_State:
				if (last)
					last->next = j->next;
				else
					current_Job = j->next;
				destroy_Job(j);
		}
	}
}

void fg ()
{

}

void bg ()
{

}

int launch_builtin (char** tokens)
{
	static const char* special[] = {"fg", "bg", "jobs", "exit"};

	if (!no_tokens(tokens+1))
		return 0;

	if(strcmp(tokens[0], special[0]))
		fg();
	else if(strcmp(tokens[0], special[1]))
		bg();
	else if(strcmp(tokens[0], special[1]))
		print_Jobs();
	else if(strcmp(tokens[0], special[1]))
	{
		destroy_Job(current_Job);
		exit(0);
	}
}


#endif /* JOB_CONTROL_H */



/* Test JOB_CONTROL */
#if __INCLUDE_LEVEL__ == 0 && defined __INCLUDE_LEVEL__
#include <stdio.h>			// setvbuf, freopen, printf
#include <fcntl.h>			// open
#include <unistd.h>			// usleep, close
#include "tokenize.h"
#include "job.h"

int main(int argc, char* argv[])
{
	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	if (argc == 1)
		freopen("job_control_input.txt", "r", stdin);

	while (puts("# "), read_line(stdin) != NULL)
	{
		print_updates();
		char** tokens = set_tokens(" \t");

		if (no_tokens(tokens))
			continue;

		if (launch_builtin(tokens))
			continue;

		current_Job = make_Job(tokens);
		if (current_Job == NULL)
			continue;

		launch_Job(current_Job);
		if (current_Job->foreground)
			wait_Job(current_Job);
	}
	putchar('\n');

	return 0;
}
#endif
/* Test JOB_CONTROL */