#ifndef JOB_H
#define JOB_H


#include <unistd.h>			// fork, pid_t, execvp
#include <fcntl.h>			// open
#include <signal.h>			// SIGINT, SIGTSTP, signal, SIG_ERR
#include <stdlib.h>			// malloc, free
#include <stdio.h>			// fprintf, perror
#include <errno.h>			// ENOENT
#include <string.h>			// strdup
#include <termios.h>		// struct termios, tcsetattr, tcgetattr
#include "tokenize.h"
#include "parse_tokens.h"
#include <assert.h>			// assert
#include "faces.h"

#define MAX_PIPE_MEMBERS 100


typedef enum
{
	Error_State = -1,
	Running_State,
	Stopped_State,
	Done_State
} State;

const char* state_strings[] =
{
	"Error",
	"Running",
	"Stopped",
	"Done",
};

const char* get_state_string (State state)
{
	return state_strings[state + 1];
}

typedef struct Process
{
	pid_t pid;
	int in, out, err;
	int close_me[3];
	State state;
	struct Process* next;
} Process;


typedef struct Job
{
	int index;
	pid_t pgid;
	int foreground;
	char* command;
	State state;
	Process* p;
	struct termios tmodes;
	struct Job* next;
} Job;


static char** tmp_process_tokens[MAX_PIPE_MEMBERS] = {NULL};
int Job_count = 1;
Job* current_Job = NULL;
struct termios shell_tmodes;
pid_t shell_pid = -1;


static void destroy_Process (Process* p)
{
	if (p == NULL)
		return;

	if (p->in != -1 && p->close_me[0])
		close(p->in);

	if (p->out != -1 && p->close_me[1])
		close(p->out);

	if (p->err != -1 && p->close_me[2])
		close(p->err);

	free(p);
}


static void destroy_Job (Job* j)
{
	if (j == NULL)
		return;

	if (j->p != NULL)
	{
		Process* p = j->p;
		Process* next = p->next;
		while (p != NULL)
		{
			destroy_Process(p);
			p = next;
			if (next != NULL)
				next = next->next;
		}
	}

	free(j->command);

	free(j);
}


static int set_redirect (Process* p, char* path, int which)
{
	if (path == NULL)
		return 0;

	int success = -1;
	if (which == 0)
		success = p->in = open(path, O_RDONLY);
	else if (which == 1)
		success = p->out = creat(path, (S_IRUSR|S_IWUSR) | (S_IRGRP|S_IWGRP) | (S_IROTH|S_IWOTH));
	else if (which == 2)
		success = p->err = creat(path, (S_IRUSR|S_IWUSR) | (S_IRGRP|S_IWGRP) | (S_IROTH|S_IWOTH));

	if (success == -1)
	{
		destroy_Process(p);
		fprintf(stderr, "yash: ");
		perror(path);
		return -1;
	}

	p->close_me[which] = 1;

	return 0;
}


static Process* make_Process (char** tokens)
{
	assert(tokens != NULL);


	/* Allocate space for process */
	Process* p = (Process*) malloc(sizeof(Process));
	if (p == NULL)
	{
		perror(flip_table " yash: make_Process: malloc");
		return NULL;
	}


	/* Default initialization */
	p->in = -1;
	p->out = -1;
	p->err = -1;
	for (int i=0; i<3; i++)
		p->close_me[i] = 0;
	p->state = Running_State;
	p->next = NULL;


	/* Get redirect paths */
	char* path[3];
	enum {in, out, err};
	path[in] = get_redirect_in(tokens);
	path[out] = get_redirect_out(tokens);
	path[err] = get_redirect_error(tokens);


	/* Get order of redirects inputted */
	int index[3] = {0,1,2};
	for (int i=0; i<2; i++)
	{
		int least = i;
		for (int j=i+1; j<3; j++)
			if (path[index[j]] < path[index[least]])
				least = j;

		if (least == index[i])
			continue;

		int tmp = index[i];
		index[i] = index[least];
		index[least] = tmp;
	}


	/* Parse redirects (in received order) */
	for (int i=0; i<3; i++)
		if (set_redirect(p, path[index[i]], index[i]) == -1)
			return NULL;


	return p;
}


Job* make_Job (char** tokens)
{
	assert(tokens != NULL);


	/* Allocate space for Job */
	Job* j = (Job*) malloc(sizeof(Job));
	if (j == NULL)
	{
		perror(flip_table " yash: make_Job: malloc");
		return NULL;
	}


	/* Default initialization */
	j->index = Job_count++;
	j->pgid = 0;
	j->foreground = !clear_ampersand(tokens);
	j->command = concat_tokens(tokens," ");
	j->state = Running_State;
	j->tmodes = shell_tmodes;
	j->next = NULL;


	/* Make processes */
	int i = 0;
	Process* last = NULL;
	while (tokens != NULL && i < MAX_PIPE_MEMBERS)
	{
		char** next_tokens = set_pipe_start(tokens);

		Process* p = NULL;
		if (i == 0)
			p = j->p = make_Process(tokens);
		else
			p = last->next = make_Process(tokens);

		if (p == NULL)
		{
			destroy_Job(j);
			return NULL;
		}

		/* Clip command args at first special symbol */
		set_args_end(tokens);

		last = p;
		tmp_process_tokens[i] = tokens;
		tokens = next_tokens;
		i++;
	}


	return j;
}


/* Called in forked child. */
static void launch_Process (Process* p, int pipe_in, int pipe_out, char** tokens)
{
	// printf("My pid: %d, pgid: %d\n", getpid(), getpgid(0));

	/* Reset Signals */
	if (signal (SIGINT, SIG_DFL) == SIG_ERR)  perror(flip_table " yash: signal");
	if (signal (SIGQUIT, SIG_DFL) == SIG_ERR) perror(flip_table " yash: signal");
	if (signal (SIGTSTP, SIG_DFL) == SIG_ERR) perror(flip_table " yash: signal");
	if (signal (SIGTTIN, SIG_DFL) == SIG_ERR) perror(flip_table " yash: signal");
	if (signal (SIGTTOU, SIG_DFL) == SIG_ERR) perror(flip_table " yash: signal");
	if (signal (SIGCHLD, SIG_DFL) == SIG_ERR) perror(flip_table " yash: signal");


	/* Init Pipes/Redirects */
	if (p->in != -1)
	{
		if (dup2(p->in, STDIN_FILENO) == -1)
			perror(flip_table " yash: redirecting stdin");
		close(p->in);
	}
	else if (pipe_in != -1)
	{
		if (dup2(pipe_in, STDIN_FILENO) == -1)
			perror(flip_table " yash: pipe in");
		close(pipe_in);
	}

	if (p->out != -1)
	{
		if (dup2(p->out, STDOUT_FILENO) == -1)
			perror(flip_table " yash: redirecting stdout");
		close(p->out);
	}
	else if (pipe_out != -1)
	{
		if (dup2(pipe_out, STDOUT_FILENO) == -1)
		{
			fprintf(stderr, "(%d) ", pipe_out);
			perror(flip_table " yash: pipe out");
		}
		close(pipe_out);
	}

	if (p->err != -1)
	{
		if (dup2(p->err, STDERR_FILENO) == -1)
			perror(flip_table " yash: redirecting stderr");
		close(p->err);
	}


	/* Execute Process */
	execvp(tokens[0], tokens);

	if (errno == ENOENT)
		fprintf(stderr, "%s: command not found\n", tokens[0]);
	else
	{
		fprintf(stderr, flip_table " yash: exec: ");
		perror(tokens[0]);
	}


	_exit(1);
}


int launch_Job (Job* j)
{
	pid_t pid = 0, pgid = 0;

	struct{
		union{
			int array[2];
			struct{
			int next_in,
				out;
			};
		};
		int in;
	} Pipe = {{{-1, -1}}, -1};


	int i = 0;
	Process* p = j->p;
	while (p != NULL)
	{
		/* Cleaning pipes */
		if (i > 0)
		{
			close(Pipe.out); // close pipe-out of previous process
			Pipe.in = Pipe.next_in; // save pipe-in to close after launching process
		}

		/* Pipe (if not last process) */
		if (p->next != NULL)
		{
			if (pipe(Pipe.array) == -1)
			{
				perror(flip_table " yash: pipe");
				return -1;
			}
		}
		else
			Pipe.out=-1;

		pid = fork();
		pgid = j->pgid;

		/* Fork Error */
		if (pid == -1)
		{
			perror(flip_table " yash: fork");
			return -1;
		}

		/* Child */
		else if (pid == 0)
		{
			/* Place Child Process in Job process group.
			   Repeated x2 to avoid race condition.
			   Controlling Terminal set at every child,
			   also to avoid race condition. */
			pid = getpid();
			if (pgid == 0)
				pgid = pid;
			setpgid(pid, pgid);
			if (j->foreground)
				if (tcsetpgrp (STDIN_FILENO, pgid) == -1)
					perror(blank_face " Warning: tcsetpgrp");

			launch_Process(p, Pipe.in, Pipe.out, tmp_process_tokens[i]);
		}

		/* Parent */
		else
		{
			/* Place Child Process in Job process group.
			   Repeated x2 to avoid race condition.
			   Controlling Terminal set at every child,
			   also to avoid race condition. */
			p->pid = pid;
			if (pgid == 0)
				j->pgid = pgid = pid;
			setpgid(pid, pgid);
			if (j->foreground)
				if (tcsetpgrp (STDIN_FILENO, pgid) == -1)
					perror(blank_face " Warning: tcsetpgrp");
		}

		if (i > 0)
			close(Pipe.in); // close pipe-in of child

		p = p->next;
		i++;
	}


	return 0;
}

#endif /* JOB_H */



/* Test JOB */
#if __INCLUDE_LEVEL__ == 0 && defined __INCLUDE_LEVEL__
#include <stdio.h>			// setvbuf, freopen, printf
#include <fcntl.h>			// open
#include <unistd.h>			// usleep, close
#include "faces.h"


int main(int argc, char* argv[])
{
	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

	if (argc == 1)
		freopen("input.txt", "r", stdin);

	if (!isatty(STDIN_FILENO))
		printf(blank_face " Warning: Job Control won't work because the program is not executing from a tty.\n");

	for (int i=0; read_line(stdin) != NULL; i++) {
		char** tokens = set_tokens(" \t");

		print_tokens(tokens);
		if (no_tokens(tokens))
			continue;

		current_Job = make_Job(tokens);
		if (current_Job == NULL)
			continue;

		launch_Job(current_Job);
		destroy_Job(current_Job);
		current_Job = NULL;
	}

	/* Display for debug */
	usleep(150000);
	int o = open("out.txt",O_RDONLY);
	int o2 = open("out2.txt",O_RDONLY);
	int e = open("error.txt",O_RDONLY);

	char c[64];
	ssize_t bytes = 0;
	printf("out.txt:\n");
	while ((bytes = read(o, c, 64)))
		write(STDOUT_FILENO, c, bytes);
	printf("\n\n");

	printf("out2.txt:\n");
	while ((bytes = read(o2, c, 64)))
		write(STDOUT_FILENO, c, bytes);
	printf("\n\n");

	printf("error.txt:\n");
	while ((bytes = read(e, c, 64)))
		write(STDOUT_FILENO, c, bytes);

	close(o);
	close(o2);
	close(e);
}
#endif
/* Test JOB */