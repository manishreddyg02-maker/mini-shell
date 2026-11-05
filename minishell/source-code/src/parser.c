#include <string.h>
#include "parser.h"

int parse_input(char *input, char **args, int max_args, int *background)
{
    int i = 0;

    *background = 0;

    char *token = strtok(input, " \t\n");

    while (token != NULL && i < max_args - 1)
    {
        size_t len = strlen(token);

        /* Check whether token ends with & */
        if (len > 0 && token[len - 1] == '&')
        {
            *background = 1;
            token[len - 1] = '\0';

            if (token[0] != '\0')
                args[i++] = token;
        }
        else
        {
            args[i++] = token;
        }

        token = strtok(NULL, " \t\n");
    }

    args[i] = NULL;

    return i;
}