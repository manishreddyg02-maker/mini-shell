
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

#include "executor.h"
#include "builtins.h"

#define MAX_PIPE_COMMANDS 16

/* Handle input and output redirection */
static int apply_redirection(char **args)
{
    int i = 0;

    while (args[i] != NULL)
    {
        if (strcmp(args[i], "<") == 0 ||
            strcmp(args[i], ">") == 0 ||
            strcmp(args[i], ">>") == 0)
        {
            if (args[i + 1] == NULL)
            {
                fprintf(stderr, "mini-shell: missing filename\n");
                return -1;
            }

            int fd = -1;

            if (strcmp(args[i], "<") == 0)
            {
                fd = open(args[i + 1], O_RDONLY);
            }
            else if (strcmp(args[i], ">>") == 0)
            {
                fd = open(args[i + 1],
                          O_WRONLY | O_CREAT | O_APPEND,
                          0644);
            }
            else
            {
                fd = open(args[i + 1],
                          O_WRONLY | O_CREAT | O_TRUNC,
                          0644);
            }

            if (fd < 0)
            {
                perror("open");
                return -1;
            }

            int target_fd = (strcmp(args[i], "<") == 0)
                           ? STDIN_FILENO : STDOUT_FILENO;

            if (dup2(fd, target_fd) < 0)
            {
                perror("dup2");
                close(fd);
                return -1;
            }

            close(fd);

            /* Remove redirection operator and filename */
            int j = i;

            while (args[j + 2] != NULL)
            {
                args[j] = args[j + 2];
                j++;
            }

            args[j] = NULL;
        }
        else
        {
            i++;
        }
    }

    return 0;
}

/* Execute commands connected through pipes */
static void execute_pipeline(char **args, int background)
{
    char **commands[MAX_PIPE_COMMANDS];
    pid_t pids[MAX_PIPE_COMMANDS];

    int command_count = 1;
    commands[0] = args;

    /* Split arguments using pipe operator */
    for (int i = 0; args[i] != NULL; i++)
    {
        if (strcmp(args[i], "|") == 0)
        {
            if (i == 0 || args[i + 1] == NULL ||
                strcmp(args[i - 1], "|") == 0)
            {
                fprintf(stderr, "mini-shell: invalid pipe syntax\n");
                return;
            }

            if (command_count >= MAX_PIPE_COMMANDS)
            {
                fprintf(stderr, "mini-shell: too many pipeline commands\n");
                return;
            }

            args[i] = NULL;
            commands[command_count++] = &args[i + 1];
        }
    }

    int previous_read = -1;
    int started = 0;

    for (int i = 0; i < command_count; i++)
    {
        int pipefd[2] = {-1, -1};

        /* Create pipe unless this is the last command */
        if (i < command_count - 1)
        {
            if (pipe(pipefd) < 0)
            {
                perror("pipe");
                break;
            }
        }

        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");

            if (pipefd[0] != -1)
                close(pipefd[0]);

            if (pipefd[1] != -1)
                close(pipefd[1]);

            break;
        }

        if (pid == 0)
        {
            /* Connect previous pipe to standard input */
            if (previous_read != -1)
            {
                if (dup2(previous_read, STDIN_FILENO) < 0)
                {
                    perror("dup2");
                    _exit(EXIT_FAILURE);
                }

                close(previous_read);
            }

            /* Connect current pipe to standard output */
            if (i < command_count - 1)
            {
                close(pipefd[0]);

                if (dup2(pipefd[1], STDOUT_FILENO) < 0)
                {
                    perror("dup2");
                    _exit(EXIT_FAILURE);
                }

                close(pipefd[1]);
            }

            /* Apply redirection if specified */
            if (apply_redirection(commands[i]) < 0)
                _exit(EXIT_FAILURE);

            if (commands[i][0] == NULL)
                _exit(EXIT_FAILURE);

            execvp(commands[i][0], commands[i]);

            perror("execvp");
            _exit(EXIT_FAILURE);
        }
        else
        {
            pids[started++] = pid;

            /* Parent closes previous read descriptor */
            if (previous_read != -1)
                close(previous_read);

            /* Parent keeps only the read end */
            if (i < command_count - 1)
            {
                close(pipefd[1]);
                previous_read = pipefd[0];
            }
        }
    }

    if (previous_read != -1)
        close(previous_read);

    /* Handle foreground and background pipelines */
    if (background)
    {
        if (started > 0)
        {
            printf("[Background] Pipeline started with PID: %d\n",
                   pids[0]);
        }
    }
    else
    {
        for (int i = 0; i < started; i++)
        {
            int status;

            while (waitpid(pids[i], &status, 0) == -1)
            {
                if (errno != EINTR)
                {
                    perror("waitpid");
                    break;
                }
            }
        }
    }
}

/* Execute a command */
void execute_command(char **args, int background)
{
    if (args[0] == NULL)
        return;

    /* Check whether the command contains a pipe */
    for (int i = 0; args[i] != NULL; i++)
    {
        if (strcmp(args[i], "|") == 0)
        {
            execute_pipeline(args, background);
            return;
        }
    }

    /* Execute built-in commands */
    if (execute_builtin(args))
        return;

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return;
    }

    /* Child process */
    if (pid == 0)
    {
        if (apply_redirection(args) < 0)
            _exit(EXIT_FAILURE);

        execvp(args[0], args);

        perror("execvp");
        _exit(EXIT_FAILURE);
    }

    /* Parent process */
    else
    {
        if (background)
        {
            printf("[Background] Process started with PID: %d\n", pid);
        }
        else
        {
            int status;

            while (waitpid(pid, &status, 0) == -1)
            {
                if (errno != EINTR)
                {
                    perror("waitpid");
                    break;
                }
            }
        }
    }
}
