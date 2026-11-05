#include <stdio.h>
#include <stdlib.h>

#include "prompt.h"
#include "input.h"
#include "parser.h"
#include "executor.h"
#include "process.h"

int main(void)
{
    char *input;
    char *args[MAX_ARGS];

    int background;

    setup_signal_handling();

    while (1)
    {
        reap_background_processes();

        display_prompt();

        input = read_input();

        if (input == NULL)
        {
            printf("\n");
            break;
        }

        reap_background_processes();

        parse_input(input, args, MAX_ARGS, &background);

        execute_command(args, background);

        free(input);
    }

    return 0;
}