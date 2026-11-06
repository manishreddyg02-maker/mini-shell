#include <stdio.h>
#include <stdlib.h>

#include "input.h"

char *read_input(void)
{
    char *input = NULL;
    size_t size = 0;

    if (getline(&input, &size, stdin) == -1)
    {
        free(input);
        return NULL;
    }

    return input;
}