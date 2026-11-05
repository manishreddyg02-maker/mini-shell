#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"

int execute_builtin(char **args)
{
    if (strcmp(args[0], "exit") == 0)
    {
        exit(EXIT_SUCCESS);
    }

    if (strcmp(args[0], "cd") == 0)
    {
        const char *path = args[1];

        if (path == NULL)
        {
            path = getenv("HOME");
        }

        if (path == NULL || chdir(path) == -1)
        {
            perror("cd");
        }

        return 1;
    }

    if (strcmp(args[0], "pwd") == 0)
    {
        char cwd[1024];

        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            printf("%s\n", cwd);
        }
        else
        {
            perror("pwd");
        }

        return 1;
    }

    return 0;
}