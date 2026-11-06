#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "builtins.h"
#include "history.h"
#include "process.h"

/* Convert %1 or 1 into a job ID. */
static int parse_job_id(const char *argument, int *job_id)
{
    if (argument == NULL)
        return 0;

    if (argument[0] == '%')
        argument++;

    if (*argument == '\0')
        return 0;

    char *endptr;
    errno = 0;

    long value = strtol(argument, &endptr, 10);

    if (errno != 0 ||
        *endptr != '\0' ||
        value < 1 ||
        value > INT_MAX)
    {
        return 0;
    }

    *job_id = (int)value;
    return 1;
}

int execute_builtin(char **args)
{
    if (args == NULL || args[0] == NULL)
        return 0;

    /* EXIT */
    if (strcmp(args[0], "exit") == 0)
    {
        exit(EXIT_SUCCESS);
    }

    /* CD */
    if (strcmp(args[0], "cd") == 0)
    {
        const char *path = args[1];

        if (path == NULL)
            path = getenv("HOME");

        if (path == NULL)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }

        if (chdir(path) == -1)
        {
            perror("cd");
            return 1;
        }

        return 0;
    }

    /* PWD */
    if (strcmp(args[0], "pwd") == 0)
    {
        char cwd[1024];

        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            printf("%s\n", cwd);
            return 0;
        }

        perror("pwd");
        return 1;
    }

    /* HISTORY */
    if (strcmp(args[0], "history") == 0)
    {
        history_display();
        return 0;
    }

    /* JOBS */
    if (strcmp(args[0], "jobs") == 0)
    {
        list_background_jobs();
        return 0;
    }

    /* FG */
    if (strcmp(args[0], "fg") == 0)
    {
        int job_id;

        if (!parse_job_id(args[1], &job_id))
        {
            fprintf(stderr, "Usage: fg %%job_id\n");
            return 1;
        }

        return foreground_job(job_id) == 0 ? 0 : 1;
    }

    /* BG */
    if (strcmp(args[0], "bg") == 0)
    {
        int job_id;

        if (!parse_job_id(args[1], &job_id))
        {
            fprintf(stderr, "Usage: bg %%job_id\n");
            return 1;
        }

        return background_job(job_id) == 0 ? 0 : 1;
    }

    /* Not a built-in command. */
    return 0;
}