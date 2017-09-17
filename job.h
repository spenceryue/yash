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
	int index;
	char* command;
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


void destroy_Process (Process* p)
{
	if (p == NULL)
		return;
	fprintf(stderr, "destroying: %d, \"%s\"\n", p->pid, p->command);
	if (p->in != -1)
	{
		fsync(p->in);
		int result = close(p->in);
		fprintf(stderr, "[%d] \tClosing p->in \t(%d) \tresult: %2d\n", p->index, p->in, result);
		if (result == -1)
		{
			perror("\t\t\toh dear");
			fprintf(stderr, "\n");
		}
	}

	if (p->out != -1)
	{
		fsync(p->out);
		int result = close(p->out);
		fprintf(stderr, "[%d] \tClosing p->out \t(%d) \tresult: %2d\n", p->index, p->out, result);
		if (result == -1)
		{
			perror("\t\t\toh dear");
			fprintf(stderr, "\n");
		}
	}

	if (p->err != -1)
	{
		fsync(p->err);
		int result = close(p->err);
		fprintf(stderr, "[%d] \tClosing p->err \t(%d) \tresult: %2d\n", p->index, p->err, result);
		if (result == -1)
		{
			perror("\t\t\toh dear");
			fprintf(stderr, "\n");
		}
	}

	free(p->command);

	free(p);

	fprintf(stderr, "\n\n");
}


static void destroy_Job (volatile Job* j)
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

	free((Job*) j);
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


	j->index = job_count++;
	j->pgid = 0;
	j->command = concat_tokens(tokens," ");
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
		(*p)->index = i;
		(*p)->command = concat_tokens(tokens, " ");

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
	pid_t pid = getpid();
	if (pgid == 0)
		pgid = pid;
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
		{
			int result = close(mypipe[1]); // close pipe-out of previous process
			fprintf(stderr, ">>[%d] \tClosing mypipe[1] \t(%d) \tresult: %2d\n", p->index, mypipe[1], result);
			if (result == -1)
			{
				perror("\t\t\toh dear");
				fprintf(stderr, "\n");
			}

			infile = mypipe[0]; // save pipe-in and close after launching process
			fprintf(stderr, "++[%d] \tSaving infile   \t(%d)\n", p->index, infile);
		}
		if (p->next != NULL)
		{
			if (pipe(mypipe) == -1)
			{
				perror("yash: pipe");
				return -1;
			}
			p->out = mypipe[1]; // hook up pipe-out
			p->next->in = mypipe[0]; // hook up next process's pipe-in
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
		p->pid = pid;
		if (j->pgid == 0)
			j->pgid = pid;
		setpgid(pid, j->pgid); // Put child in the job process group

		if (i > 0)
		{
			int result = close(infile); // close pipe-in of child
			fprintf(stderr, "::[%d] \tClosing infile   \t(%d) \tresult: %2d\n", p->index, infile, result);
			if (result == -1)
			{
				perror("\t\t\toh dear");
				fprintf(stderr, "\n");
			}
		}

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

	for (int i=0; read_line(stdin) != NULL; i++) {
		char** tokens = set_tokens(" \t");

		print_tokens(tokens);
		if (no_tokens(tokens))
			continue;

		first_job = make_Job(tokens);
		launch_Job(first_job);
		destroy_Job(first_job);
		first_job = NULL;
	}

	/* Display for debug */
	usleep(150000);
	int f = open("out.txt",O_RDONLY);

	char c[64];
	ssize_t bytes = 0;
	while ((bytes = read(f, c, 64)))
		write(STDOUT_FILENO, c, bytes);
}
#endif
/* Test JOB */