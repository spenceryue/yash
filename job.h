#ifndef JOB_H
#define JOB_H


#include <unistd.h>			// fork, pid_t, execvp
#include <stdlib.h>			// malloc, free
#include <stdio.h>			// fprintf, perror
#include <errno.h>			// ENOENT
#include "parse_tokens.h"
// #define NDEBUG
#include <assert.h>			// assert


typedef struct Process
{
	pid_t pid;
	int status;
	int in, out, err;
} Process;


typedef struct Job
{
	pid_t pgid;
	Process* p0;
	Process* p1;
	char background;
} Job;


Process* make_Process (char** tokens)
{
	assert(tokens != NULL);


	/* Allocate space for process */
	Process* p = (Process*) malloc(sizeof(Process));
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
		p->error = open(path, O_RDONLY);
		if (p->error == -1)
		{
			free(p);
			fprintf(stderr, "yash: ");
			perror(path);
			return NULL;
		}
	}
	else
		p->error = -1;


	/* Clip command args at first special symbol */
	set_args_end();


	return p;
}


void destroy_Job (Job* j)
{
	free(j->p0);
	free(j->p1);	//If j->p1 is a null pointer, free does nothing.
}


Job* make_Job (char** tokens, char** tokens_1)
{
	/* Allocate space for Job */
	Job* j = (Job*) malloc(sizeof(Job));
	if (j == NULL)
	{
		perror("yash: make_Job: malloc");
		return NULL;
	}


	/* make first Process */
	j->p0 = make_Process(tokens);
	if (j->p0 == NULL)
	{
		destroy_Job(j);
		return NULL;
	}


	/* make second Process (if the Job has a pipe) */
	if (tokens_1 == NULL)
		j->p1 = NULL;
	else
	{
		j->p1 = make_Process(tokens_1);
		if (j->p1 == NULL)
		{
			destroy_Job(j);
			return NULL;
		}
	}


	/* */
	j->pgid = 0;
	j->background = has_ampersand(tokens) || has_ampersand(tokens_1);


	return j;
}


/* Called in forked child. */
int launch_Process (Process* p, pid_t pgid, char** tokens)
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


	/* Init Pipes */
	if (p->in != -1)
	{
		dup2(p->in, STDIN_FILENO);
		close(p->in);
	}
	if (p->out != -1)
	{
		dup2(p->out, STDOUT_FILENO);
		close(p->out);
	}
	if (p->err != -1)
	{
		dup2(p->err, STDERR_FILENO);
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

int launch_Job (Job* j, char** tokens, char** child_tokens)
{

}

#endif /* JOB_H */



/* Test JOB */
#if __INCLUDE_LEVEL__ == 0 && defined __INCLUDE_LEVEL__
#include <stdio.h>


int main(int argc, char* argv[])
{
	Job j = {10,(Process*)1,(Process*)2};
	printf("%lu %lu\n", (unsigned long int)j.p0, (unsigned long int)j.p1);
}
#endif
/* Test JOB */