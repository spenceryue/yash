#ifndef JOB_CONTROL_H
#define JOB_CONTROL_H


#include <unistd.h>			// fork, pid_t, execvp
#include <signal.h>			// SIGINT, SIGTSTP, signal, SIG_ERR
#include <sys/wait.h>		// wait
#include <stdio.h>			// fprintf, perror
#include <errno.h>			// ECHILD
#include <string.h>			// strcpy
#include <termios.h>		// tcsetattr, tcgetattr
#include "job.h"
#include <assert.h>			// assert
#include "faces.h"


static int is_State (Job* j, State s)
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
		// fprintf(stderr, "Job (%s) is %s\n", j->command, get_state_string(s));
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
				return p;

	return NULL;
}

Job* find_Job (pid_t pid)
{
	for (Job* j = current_Job; j != NULL; j = j->next)
		for (Process* p = j->p; p != NULL; p = p->next)
			if (p->pid == pid)
				return j;

	return NULL;
}

int count_Jobs (Job* j)
{
	int result = 0;
	for (; j != NULL; j = j->next)
		result++;

	return result;
}

static void update_Process (Process* p, int status)
{
	assert (p != NULL);

	if (WIFSTOPPED(status))
	{
		// fprintf(stderr, "Marking (%d): %s\n", p->pid, get_state_string(Stopped_State));
		p->state = Stopped_State;
	}
	else if (WIFEXITED(status))
	{
		// fprintf(stderr, "Marking (%d): %s\n", p->pid, get_state_string(Done_State));
		p->state = Done_State;
	}
	else if (WIFSIGNALED(status))
	{
		// fprintf(stderr, "Marking (%d): %s\n", p->pid, get_state_string(Done_State));
		if (WTERMSIG(status) != 2) // hack: Signal 2 is Ctrl+C
		{
			Job* parent = find_Job(p->pid);
			parent->foreground = 0;	// exited while stopped from a signal,
									// means probably a "kill <pid>" was sent in the shell.
									// (in any case: want to print)
		}
		p->state = Done_State;
	}
	else
	{
		// fprintf(stderr, "Marking (%d): %s\n", p->pid, get_state_string(Error_State));
		p->state = Error_State;
	}
}

static void mark_Job (Job* j, State s)
{
	j->state = s;
	for (Process* p = j->p; p != NULL; p = p->next)
		p->state = s;
}

static void get_Job_status (Job* j, int WAIT)
{
	if (j == NULL)
		return;

	pid_t pid;
	int status;
	int options = (WAIT) ? WUNTRACED : WUNTRACED|WNOHANG;

	while (!is_Stopped(j))
	{
		pid = waitpid(WAIT_ANY, &status, options);

		if (!WAIT && pid == 0)
			return; // Still Running

		if (WAIT && pid == -1 && errno == ECHILD)
		{
			mark_Job(j, Done_State); // hack: Mark Job as Done... Somehow it skipped being a zombie
			break;
		}

		if (pid == -1)
		{
			perror(flip_table " yash: waitpid");
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
	{
		if (WAIT && j->foreground)
			tcgetattr(STDIN_FILENO, &j->tmodes); // save Job's terminal modes
		j->state = Stopped_State;
	}
}

void update_Job (Job* j)
{
	// fprintf(stderr, "Entering %s\n", __PRETTY_FUNCTION__);
	get_Job_status(j, 0);
}

void wait_Job (Job* j)
{
	// fprintf(stderr, "Entering %s\n", __PRETTY_FUNCTION__);
	get_Job_status(j, 1);
}

void update_Jobs ()
{
	for (Job* j = current_Job; j != NULL; j = j->next)
		update_Job(j);
}

#define JOB_STRING_SIZE 1+32+1+1+2+24+2+2+1
static char* get_Job_string (Job* j)
{
	static char buffer[JOB_STRING_SIZE+1];

	sprintf(buffer,
			"[%d]%c  %-24s%%s %c\n",
			j->index,
			(j == current_Job) ? '+' : '-',
			get_state_string(j->state),
			// fill in [ j->command ] using printf
			j->foreground ? ' ' : '&'
			);

	return buffer;
}

void print_Job (Job* j)
{
	printf(get_Job_string(j), j->command);
}

/* Delete Done/Error jobs */
void clean_Jobs(int UPDATE_FIRST)
{
	if (UPDATE_FIRST)
		update_Jobs();

	Job* last = NULL;
	Job* j = current_Job;
	while (j != NULL)
	{
		switch (j->state)
		{
			case Running_State:
			case Stopped_State:
				last = j;
				j = j->next;
				break;
			case Error_State:
			case Done_State:
				if (last != NULL)
				{
					last->next = j->next;
					destroy_Job(j);
					j = last;
					Job_count--;
				}
				else
				{
					current_Job = j->next;
					destroy_Job(j);
					j = current_Job;
					Job_count--;
				}
		}
	}
}

void print_Jobs (int LIST_ALL)
{
	update_Jobs();


	char messages[Job_count][JOB_STRING_SIZE+1];
	char* commands[Job_count];
	int index = 0;


	for (Job* j = current_Job; j != NULL; j = j->next)
	{
		switch (j->state)
		{
			case Running_State:
			case Stopped_State:
				if (LIST_ALL)
				{
					strcpy(messages[index], get_Job_string(j));
					commands[index++] = j->command;
				}
				break;
			case Error_State:
			case Done_State:
				if (LIST_ALL || !j->foreground)
				{
					strcpy(messages[index], get_Job_string(j));
					commands[index++] = j->command;
				}
		}
	}


	/* Print in reverse order (oldest to newest) */
	for (int i=index-1; 0<=i; --i)
		printf(messages[i], commands[i]);


	clean_Jobs(0);
}

static void fg ()
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


	int save_Stopped_State = j->state == Stopped_State;
	j->foreground = 1;
	mark_Job(j, Running_State);
	print_Job(j);


	if (save_Stopped_State)
		tcsetattr(STDIN_FILENO, TCSADRAIN, &j->tmodes); // restore Jobs terminal modes
	tcsetpgrp(STDIN_FILENO, j->pgid);


	kill(- j->pgid, SIGCONT);
	wait_Job(j);
}

static void bg ()
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
	mark_Job(j, Running_State);

	print_Job(j);
	kill(- j->pgid, SIGCONT);
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
		return 0;
	}


	shell_pid = getpid();
	if (setpgid(0,0) == -1)
	{
		perror (flip_table " yash: abort reason: Couldn't put yash in its own process group");
		return 0;
	}
	// printf("My pid: %d, pgid: %d\n", getpid(), getpgid(0));


	while (tcgetpgrp (STDIN_FILENO) != getpgid(0))
		kill (- getpgid(0), SIGTTIN);


	if (tcsetpgrp(STDIN_FILENO, getpid()) == -1)
	{
		perror(flip_table " yash: abort reason: Couldn't obtain control of the terminal");
		return 0;
	}


	tcgetattr (STDIN_FILENO, &shell_tmodes);


	atexit(exit_handler);


	if (signal(SIGINT, signal_handler) == SIG_ERR)	perror(flip_table " yash: signal");
	if (signal(SIGTSTP, signal_handler) == SIG_ERR) perror(flip_table " yash: signal");

	if (signal (SIGCHLD, SIG_DFL) == SIG_ERR)		perror(flip_table " yash: signal");

	if (signal (SIGQUIT, SIG_IGN) == SIG_ERR)		perror(flip_table " yash: signal");
	if (signal (SIGTTIN, SIG_IGN) == SIG_ERR)		perror(flip_table " yash: signal");
	if (signal (SIGTTOU, SIG_IGN) == SIG_ERR)		perror(flip_table " yash: signal");


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
#endif
/* Test JOB_CONTROL */