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

#define MAX_PIPE_MEMBERS 2


typedef struct Process
{
	pid_t pid;
	int in, out, err;
	int status;
	struct Process* next;
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
	Process* p;
	int process_count;
	struct Job* next;
} Job;


static char** token_starts[MAX_PIPE_MEMBERS] = {0};
volatile static int job_count = 0;
volatile static Job* first_job = NULL;


static void destroy_Job (volatile Job* j)
{
	if (j == NULL)
		return;

	if (j->p != NULL)
	{
		Process* p = j->p;
		for (Process* next = p->next;
			 p != NULL;
			 next = (next == NULL) ? NULL : next->next)
		{
			free(p);
			p = next;
		}
	}

	free(j->command);
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
	char* path;


	/* Parse input redirect */
	path = get_redirect_in(tokens);
	if (path != NULL)
	{
		p->in = open(path, O_RDONLY);
		if (p->in == -1)
		{
			free(p);
			fprintf(stderr, "yash: ");
			perror(path);
			return NULL;
		}
	}
	else
		p->in = -1;


	/* Parse out redirect */
	path = get_redirect_out(tokens);
	if (path != NULL)
	{
		p->out = creat(path, (S_IRUSR|S_IWUSR) | (S_IRGRP|S_IWGRP) | (S_IROTH|S_IWOTH));
		if (p->out == -1)
		{
			free(p);
			fprintf(stderr, "yash: ");
			perror(path);
			return NULL;
		}
	}
	else
		p->out = -1;


	/* Parse error redirect */
	path = get_redirect_error(tokens);
	if (path != NULL)
	{
		p->err = open(path, O_RDONLY);
		if (p->err == -1)
		{
			free(p);
			fprintf(stderr, "yash: ");
			perror(path);
			return NULL;
		}
	}
	else
		p->err = -1;


	p->next = NULL;


	return p;
}


Job* make_Job (char* input_str, char** tokens)
{
	/* Allocate space for Job */
	Job* j = (Job*) malloc(sizeof(Job));
	if (j == NULL)
	{
		perror("yash: make_Job: malloc");
		return NULL;
	}


	j->index = job_count++;
	j->pgid = 0;
	j->command = strdup(input_str);
	j->background = 0;
	j->state = Running;


	int i = 0;
	Process** p = &(j->p);
	do
	{
		char** next_tokens = set_pipe_start(tokens);
		j->background |= has_ampersand(tokens);

		*p = make_Process(tokens);
		if (*p == NULL)
		{
			destroy_Job(j);
			return NULL;
		}

		/* Clip command args at first special symbol */
		set_args_end(tokens);

		Process** next = &((*p)->next);
		p = next;

		token_starts[i] = tokens;
		tokens = next_tokens;
		i++;
	}
	while (tokens != NULL && i < MAX_PIPE_MEMBERS);


	j->process_count = i;
	j->next = NULL;


	return j;
}


/* Called in forked child. */
static void launch_Process (Process* p, pid_t pgid, char** tokens)
{
	/* Place in Process Group */
	p->pid = getpid();
	if (pgid == 0)
		pgid = p->pid;
	setpgid(p->pid, pgid);


	/* Reset Signals */
	signal(SIGINT, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);


	fprintf(stderr, "<%d> pid: %d, pgid: %d\n", pgid, getpid(), getpgid(0));
	/* Init Pipes */
	if (p->in != -1)
	{
		fprintf(stderr,"> > > dup2(%d, STDIN_FILENO) < < < \n", p->in);
		if (dup2(p->in, STDIN_FILENO) == -1)
			perror("\tduping IN failed!");
		close(p->in);
	}
	if (p->out != -1)
	{
		fprintf(stderr,"> > > dup2(%d, STDOUT_FILENO) < < < \n", p->out);
		if (dup2(p->out, STDOUT_FILENO) == -1) {
			perror("\tduping OUT failed!");
		}
		close(p->out);
	}
	if (p->err != -1)
	{
		fprintf(stderr,"> > > dup2(%d, STDERR_FILENO) < < < \n", p->err);
		if (dup2(p->err, STDERR_FILENO) == -1)
			perror("\tduping ERR failed!");
		close(p->err);
	}
printf(">>>hi there...\n");
		printf("pid: %d, pgid: %d\n", getpid(), getpgid(0));
		print_tokens(tokens);
printf("<<<<bye there...\n");

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
	int mypipe[2];

	int N = 2*(j->process_count - 1);
	int* opened = (int*) malloc(N*sizeof(int));


	int i = 0;
	Process* p = j->p;
	fprintf(stderr, "<Parent> pid: %d, pgid: %d\n", getpid(), getpgid(0));
	do
	{
		if (p->next != NULL)
		{
			printf("hello %s\n", token_starts[i][0]);
			if (pipe(mypipe) == -1)
			{
				perror("yash: pipe");
				return -1;
			}
			printf("mypipe[0] = %d, mypipe[1] = %d\n", mypipe[0], mypipe[1]);
			p->out = opened[2*i] = mypipe[1];
			p->next->in = opened[2*i+1] = mypipe[0];
		}

		pid = fork();
		if (pid == -1)
		{
			perror("yash: fork");
			return -1;
		}

		/* Child */
		else if (pid == 0)
		{
			printf("---my pid is %d---\n",getpid());
			if (p->next != NULL)
				close(mypipe[0]);
			launch_Process(p, j->pgid, token_starts[i]);
		}

		/* Parent */
		if (j->pgid == 0)
			j->pgid = pid;
		printf("j->pgid: %d\n", j->pgid);
		setpgid(pid, j->pgid);

		p = p->next;
		i++;
	}
	while (p != NULL);

	for (int k=0; k<N; k++)
		close(opened[k]);

	free(opened);

	return 0;
}

#endif /* JOB_H */



/* Test JOB */
#if __INCLUDE_LEVEL__ == 0 && defined __INCLUDE_LEVEL__
#include <stdio.h>


int main(int argc, char* argv[])
{
	if (argc == 1)
		freopen("input.txt", "r", stdin);

	char* input_str;
	for (int i=0; (input_str = read_line(stdin)) != NULL; i++) {
		printf("\n%d\n", i);
		char** tokens = get_tokens(" \t");

		if (no_tokens(tokens))
			continue;

		print_tokens(tokens);

		first_job = make_Job(input_str, tokens);
		launch_Job(first_job);
		destroy_Job(first_job);
		first_job = NULL;
	}
}
#endif
/* Test JOB */