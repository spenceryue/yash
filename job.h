#ifndef JOB_H
#define JOB_H


#include <unistd.h>			// fork, pid_t, execvp
#include <fcntl.h>			// open
#include <signal.h>			// SIGINT, SIGTSTP, signal, SIG_ERR
#include <stdlib.h>			// malloc, free
#include <stdio.h>			// fprintf, perror
#include <errno.h>			// ENOENT
#include <string.h>			// strdup
#include "tokenize.h"
#include "parse_tokens.h"
// #define NDEBUG
#include <assert.h>			// assert

#define MAX_PIPE_MEMBERS 100


typedef struct Process
{
	pid_t pid;
	int in, out, err;
	int close_me[3];
	int status;
	volatile struct Process* next;
} Process;

typedef enum
{
	Running,
	Stopped,
	Done
} JobState;

typedef struct Job
{
	int index;
	pid_t pgid;
	char* command;
	int background;
	JobState state;
	volatile Process* p;
	struct Job* next;
} Job;


static char** token_starts[MAX_PIPE_MEMBERS] = {0};
volatile static int job_count = 1;
volatile static Job* first_job = NULL;


static void destroy_Process (volatile Process* p)
{
	if (p == NULL)
		return;

	if (p->in != -1 && p->close_me[0])
		close(p->in);

	if (p->out != -1 && p->close_me[1])
		close(p->out);

	if (p->err != -1 && p->close_me[2])
		close(p->err);

	free((Process*) p);
}


static void destroy_Job (volatile Job* j)
{
	if (j == NULL)
		return;

	if (j->p != NULL)
	{
		volatile Process* p = j->p;
		volatile Process* next = p->next;
		while (p != NULL)
		{
			destroy_Process(p);
			p = next;
			if (next != NULL)
				next = next->next;
		}
	}

	free(j->command);

	free((Job*) j);
}


static int redirect_it (Process* p, char* path, int which)
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
		perror("yash: make_Process: malloc");
		return NULL;
	}


	/* Default initialization */
	p->in = -1;
	p->out = -1;
	p->err = -1;
	for (int i=0; i<3; i++)
		p->close_me[i] = 0;
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
		if (redirect_it(p, path[index[i]], index[i]) == -1)
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
		perror("yash: make_Job: malloc");
		return NULL;
	}


	/* Default initialization */
	j->index = job_count++;
	j->pgid = 0;
	j->command = concat_tokens(tokens," ");
	j->background = 0;
	j->state = Running;
	j->next = NULL;


	/* Make processes */
	int i = 0;
	volatile Process* last;
	do
	{
		char** next_tokens = set_pipe_start(tokens);
		j->background |= has_ampersand(tokens);

		volatile Process* p;
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
		token_starts[i] = tokens;
		tokens = next_tokens;
		i++;
	}
	while (tokens != NULL && i < MAX_PIPE_MEMBERS);


	return j;
}


/* Called in forked child. */
static void launch_Process (volatile Process* p, int background, int pipe_in, int pipe_out, char** tokens)
{
	/* Reset Signals */
	signal(SIGINT, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);


	/* Init Pipes/Redirects */
	if (p->in != -1)
	{
		if (dup2(p->in, STDIN_FILENO) == -1)
			perror("yash: redirecting stdin");
		close(p->in);
	}
	else if (pipe_in != -1)
	{
		if (dup2(pipe_in, STDIN_FILENO) == -1)
			perror("yash: pipe in");
		close(pipe_in);
	}

	if (p->out != -1)
	{
		if (dup2(p->out, STDOUT_FILENO) == -1)
			perror("yash: redirecting stdout");
		close(p->out);
	}
	else if (pipe_out != -1)
	{
		if (dup2(pipe_out, STDOUT_FILENO) == -1)
		{
			fprintf(stderr, "(%d) ", pipe_out);
			perror("yash: pipe out");
		}
		close(pipe_out);
	}

	if (p->err != -1)
	{
		if (dup2(p->err, STDERR_FILENO) == -1)
			perror("yash: redirecting stderr");
		close(p->err);
	}


	/* Execute Process */
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


int launch_Job (volatile Job* j)
{
	pid_t pid;

	struct{
		union{
			int array[2];
			struct{
			int next_in,
				out;
			};
		};
		int in;
	} Pipe = {{.next_in=-1, .out=-1}, .in=-1};


	int i = 0;
	volatile Process* p = j->p;
	do
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
				perror("yash: pipe");
				return -1;
			}
		}
		else
			Pipe.out=-1;

		pid = fork();

		/* Fork Error */
		if (pid == -1)
		{
			perror("yash: fork");
			return -1;
		}

		/* Child */
		else if (pid == 0)
		{
			/* Place Child Process in Job process group.
			   Repeated x2 for guarantee.			 */
			pid_t pid = getpid();
			if (j->pgid == 0)
				j->pgid = pid;
			setpgid(pid, j->pgid);

			launch_Process(p, j->background, Pipe.in, Pipe.out, token_starts[i]);
		}

		/* Parent */
		else
		{
			/* Place Child Process in Job process group.
			   Repeated x2 for guarantee.			 */
			p->pid = pid;
			if (j->pgid == 0)
				j->pgid = pid;
			setpgid(pid, j->pgid); // Put child in the job process group
		}

		if (i > 0)
			close(Pipe.in); // close pipe-in of child

		p = p->next;
		i++;
	}
	while (p != NULL);


	return 0;
}

#endif /* JOB_H */



/* Test JOB */
#if __INCLUDE_LEVEL__ == 0 && defined __INCLUDE_LEVEL__
#include <stdio.h>


int main(int argc, char* argv[])
{
	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

	if (argc == 1)
		freopen("input.txt", "r", stdin);

	for (int i=0; read_line(stdin) != NULL; i++) {
		char** tokens = set_tokens(" \t");

		print_tokens(tokens);
		if (no_tokens(tokens))
			continue;

		first_job = make_Job(tokens);
		if (first_job == NULL)
			continue;

		launch_Job(first_job);
		destroy_Job(first_job);
		first_job = NULL;
	}

	/* Display for debug */
	/*usleep(150000);
	int f = open("/home/spenceryue/out.txt",O_RDONLY);

	char c[64];
	ssize_t bytes = 0;
	while ((bytes = read(f, c, 64)))
		write(STDOUT_FILENO, c, bytes);*/
}
#endif
/* Test JOB */