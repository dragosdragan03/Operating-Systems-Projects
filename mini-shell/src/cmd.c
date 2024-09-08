// SPDX-License-Identifier: BSD-3-Clause
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	/* TODO: Execute cd. */
	if (chdir(dir->string) != 0) {
		perror("cd");
		return 1; // nu s a putut executa
	}
	return 0; // s a putut executa
}

static bool shell_pwd(simple_command_t *s)
{
	char cwd[1024];

	int orignal_stdout = dup(STDOUT_FILENO);

	getcwd(cwd, sizeof(cwd));

	if (s->out != NULL) {
		char *file_output = get_word(s->out);
		int file_out = open(file_output, O_WRONLY | O_CREAT | O_TRUNC, 0666);
		// creez fisierul unde vreau sa afisez
		if (file_out == -1) { // programare defensiva ca nu s a putut deschide fisierul
			perror("open");
			exit(EXIT_FAILURE);
		}
		dup2(file_out, STDOUT_FILENO);
		printf("%s\n", cwd);

		fflush(stdout);
		close(file_out);
		free(file_output);
	} else {
		printf("%s\n", cwd);
	}
	dup2(orignal_stdout, STDOUT_FILENO);
	close(orignal_stdout);

	return 0;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* TODO: Execute exit/quit. */
	return SHELL_EXIT;
}

void read_write_files(simple_command_t *s)
{
	if (s->in != NULL) { // verific daca trebuie sa citesc dintr un fisier
		int fd = open(get_word(s->in), O_RDONLY); // deschid fisierul din care vreau sa citesc

		if (fd == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}
		dup2(fd, STDIN_FILENO);
		close(fd);
	}

	// inseamna ca am primit un fisier si vreau sa afisez in el
	if (s->out != NULL) {
		int file_out;

		if (s->io_flags == IO_REGULAR) { // inseamna ca urmeaza sa fie suprascris
			file_out = open(get_word(s->out), O_WRONLY | O_CREAT | O_TRUNC, 0666);
			close(file_out);
		}

		if (s->err != NULL || s->io_flags >= IO_OUT_APPEND) { // inseamna ca este semnul ">>"
			// adaug la fisier
			file_out = open(get_word(s->out), O_WRONLY | O_CREAT | O_APPEND, 0666);
		} else { //semnul ">"
			// suprascrie fisierul
			file_out = open(get_word(s->out), O_WRONLY | O_CREAT | O_TRUNC, 0666);
		}
		if (file_out == -1) { // programare defensiva ca nu s a putut deschide fisierul
			perror("open");
			exit(EXIT_FAILURE);
		}
		dup2(file_out, STDOUT_FILENO);
		close(file_out);
	}

	if (s->err != NULL) {
		int file_err;

		if (s->io_flags == IO_REGULAR) { // inseamna ca urmeaza sa fie suprascris
			file_err = open(get_word(s->err), O_WRONLY | O_CREAT | O_TRUNC, 0666);
			close(file_err);
		}
		if (s->out != NULL || s->io_flags >= IO_OUT_APPEND) { // inseamna ca este semnul ">>"
			// adauga la fisier
			file_err = open(get_word(s->err), O_WRONLY | O_CREAT | O_APPEND, 0666);
		} else if (s->io_flags == IO_REGULAR && s->out == NULL) { //semnul ">"
			// suprascrie
			file_err = open(get_word(s->err), O_WRONLY | O_CREAT | O_TRUNC, 0666);
		}
		if (file_err == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}
		dup2(file_err, STDERR_FILENO);
		close(file_err);
	}
}

void opening_files(simple_command_t *s)
{
	if (s->out != NULL) {
		char *file_output = get_word(s->out);
		int file_out = open(file_output, O_WRONLY | O_CREAT | O_TRUNC, 0666);

		if (file_out == -1) { // programare defensiva ca nu s a putut deschide fisierul
			perror("open");
			exit(EXIT_FAILURE);
		}
		close(file_out);
		free(file_output);
	}

	if (s->err != NULL) {
		int file_err = open(get_word(s->err), O_WRONLY | O_CREAT | O_TRUNC, 0666);

		if (file_err == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}
		close(file_err);
	}
}

void free_args(char **args)
{
	if (args == NULL)
		return;

	for (int i = 0; args[i] != NULL; ++i)
		free(args[i]);

	free(args);
}

int environment_variables(simple_command_t *s)
{
	char *env_var = get_word(s->verb);

	if (strchr(env_var, '=')) {
		char *aux = strdup(env_var);

		free(env_var);
		char *var_name = strtok_r(aux, "=", &aux);

		if (var_name != NULL) {
			char *var_value = strtok_r(NULL, "=", &aux);

			if (var_name != NULL && var_value != NULL) {
				if (setenv(var_name, var_value, 1) != 0) {
					free(aux);
					return 0;
				}
				free(aux);
				return 0;
			}
			free(aux);
			return -1;
		}
	} else { // inseamna ca trebuie sa extrag valoarea unei variabile
		char *value = getenv(s->verb->string); // verific daca exista numele respectiv

		if (value != NULL) { // inseamna ca exista variabila
			free(env_var);
			return 0;
		}
		free(env_var);
		return -1;
	}
	return -1;
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	if (environment_variables(s) == 0)
		return 0;

	int max_size = 10; // am setat un maxim de argumnte
	char **args = get_argv(s, &max_size); // creez vectorul de argumente
	// creez un proces
	pid_t pid = fork();

	if (pid == -1) {
		perror("fork");
		free_args(args);
		return -1;
	}

	if (pid == 0) {
		// procesul copil
		read_write_files(s);
		// execut comanda
		int status = execvp(s->verb->string, args);

		if (status)
			printf("Execution failed for '%s'\n", s->verb->string);

		free_args(args);
		exit(EXIT_FAILURE);
	} else if (pid > 0) {
		// procesul parinte
		int status;

		waitpid(pid, &status, 0); // astept procesul copil sa termine de executat
		free_args(args);
		if (WIFEXITED(status))
			return WEXITSTATUS(status);

		fprintf(stderr, "Child process did not terminate normally\n");
		return -1;
	}
	return 1;
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
	command_t *father)
{
	/* TODO: Execute cmd1 and cmd2 simultaneously. */

	pid_t pid = fork();

	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid == 0) {
		// procesul copil
		// execut comanda 1
		parse_command(cmd1, level, father);

		perror("execlp cmd1");
		exit(EXIT_FAILURE);
	} else {
		// procesul parinte
		// execut a doua comanda
		int status_parent_process = parse_command(cmd2, level, father);
		int status_child_process;

		waitpid(pid, &status_child_process, 0);
		// verific sa vad ca s au putut executa cu succes ambele procese
		if (!status_child_process && !status_parent_process)
			return 0;
		else
			return 1;
	}
	return 1;
	/* TODO: Replace with actual exit status. */
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
	command_t *father)
{
	/* TODO: Redirect the output of cmd1 to the input of cmd2. */
	int pipe_fd[2];

	if (pipe(pipe_fd) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();

	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid == 0) {
		// procesul copil
		close(pipe_fd[0]);
		dup2(pipe_fd[1], STDOUT_FILENO);
		close(pipe_fd[1]);

		// execut comanda 1
		parse_command(cmd1, level, father);

		perror("execlp cmd1");
		exit(EXIT_FAILURE);
	} else {
		// procesul parinte
		close(pipe_fd[1]);
		int original_stdin = dup(STDIN_FILENO);

		dup2(pipe_fd[0], STDIN_FILENO);

		close(pipe_fd[0]);

		int status_parent_process = parse_command(cmd2, level, father);

		dup2(original_stdin, STDIN_FILENO);
		close(original_stdin);

		int status;

		waitpid(pid, &status, 0);
		// returnez doar valoarea ultimului proces (daca s a putut executa sau nu)
		return status_parent_process;
	}
	/* TODO: Replace with actual exit status. */
}

int redirect_command(command_t *c, int level, command_t *father)
{
	if (!strcmp(c->scmd->verb->string, "cd")) {
		if (c->scmd->params == NULL) { // inseamna ca nu trebuie mutat nicaieri
			return -1; // inseamna ca nu s a putut executa
		}
		opening_files(c->scmd);
		return shell_cd(c->scmd->params);
	} else if (!strcmp(c->scmd->verb->string, "exit") || !strcmp(c->scmd->verb->string, "quit")) {
		return shell_exit();
	} else if (!strcmp(c->scmd->verb->string, "pwd")) {
		return shell_pwd(c->scmd);
	} else {
		return parse_simple(c->scmd, level, father); /* TODO: Replace with actual exit code of command. */
	}
	return 1;
}
/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* TODO: sanity checks */

	if (c->op == OP_NONE) {
		/* TODO: Execute a simple command. */
		return redirect_command(c, level, father);
	}

	// asta este pentru mai multe comenzi
	switch (c->op) {
	case OP_SEQUENTIAL:
		/* TODO: Execute the commands one after the other. */
		parse_command(c->cmd1, level, father);
		return parse_command(c->cmd2, level, father);

	case OP_PARALLEL:
		/* TODO: Execute the commands simultaneously. */
		return run_in_parallel(c->cmd1, c->cmd2, level, father);

	case OP_CONDITIONAL_NZERO:
		/* TODO: Execute the second command only if the first one
		 * returns non zero.
		 * este cazul cu ||
		 */
		if (parse_command(c->cmd1, level, father) == 0) // inseamna ca s a putut executa
			return 0;
		else
			return parse_command(c->cmd2, level, father);

	case OP_CONDITIONAL_ZERO:
		/* TODO: Execute the second command only if the first one
		 * returns zero.
		 * este cazul cu &&
		 */
		if (parse_command(c->cmd1, level, father) == 0) // inseamna ca a putut fi realizata
			return parse_command(c->cmd2, level, father);
		else  // inseamna ca prima nu s a putut realiza
			return -1;

	case OP_PIPE:
		/* TODO: Redirect the output of the first command to the
		 * input of the second.
		 */
		return run_on_pipe(c->cmd1, c->cmd2, level, father);  // inseamna ca s a putut executa

	default:
		return shell_exit();
	}

	return shell_exit(); /* TODO: Replace with actual exit code of command. */
}
