#include <stdio.h>
#include <unistd.h>

#include "prompt.h"

void display_prompt(void)
{
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("mini-shell:%s$ ", cwd);
    }
    else
    {
        printf("mini-shell$ ");
    }

    fflush(stdout);
}