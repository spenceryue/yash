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


Job* make_Job (char* input_text, char** tokens)
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
	j->command = input_text; // input_text now belongs to the Job
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


	j->next = NULL;


	return j;
}


/* Called in forked child. */
static void launch_Process (Process* p, pid_t pgid, int background, char** tokens)
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


	/* Init Pipes/Redirects */
	if (p->in != -1)
	{
		if (dup2(p->in, STDIN_FILENO) == -1)
			perror("yash: redirect stdin");
		close(p->in);
	}
	if (p->out != -1)
	{
		if (dup2(p->out, STDOUT_FILENO) == -1)
			perror("yash: redirect stdout");
		close(p->out);
	}
	if (p->err != -1)
	{
		if (dup2(p->err, STDERR_FILENO) == -1)
			perror("yash: redirect stderr");
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
	int mypipe[2], infile;


	int i = 0;
	Process* p = j->p;
	do
	{
		if (i > 0)
			close(mypipe[1]); // close pipe out of previous process

		if (p->next != NULL)
		{
			if (i > 0)
				infile = mypipe[0]; // save pipe in and close after launching process
			if (pipe(mypipe) == -1)
			{
				perror("yash: pipe");
				return -1;
			}
			p->out = mypipe[1]; // hook up pipe out
			p->next->in = mypipe[0]; // hook up next process's pipe in
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
			if (p->next != NULL)
				close(mypipe[0]);
			launch_Process(p, j->pgid, j->background, token_starts[i]);
		}

		/* Parent */
		if (j->pgid == 0)
			j->pgid = pid;
		setpgid(pid, j->pgid); // Put child in the job process group

		if (i > 0)
			close(infile); // close pipe in of child

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
	if (argc == 1)
		freopen("input.txt", "r", stdin);

	char* input_text;
	for (int i=0; (input_text = strdup(read_line(stdin))) != NULL; i++) {
		char** tokens = set_tokens(" \t");

		printf("%s\n", input_text);
		if (no_tokens(tokens))
			continue;

		first_job = make_Job(input_text, tokens); // input_text now belongs to the Job
		launch_Job(first_job);
		destroy_Job(first_job);
		first_job = NULL;
	}
}
#endif
/* Test JOB */