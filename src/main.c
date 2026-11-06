
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "history.h"
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

    /* Initialize terminal process-group control */
    initialize_shell_terminal();

    /* Initialize signal handling */
    setup_signal_handling();

    /* Load persistent history */
    history_load();

    while (1)
    {
        /* Check for completed background processes */
        reap_background_processes();

        /* Display shell prompt */
        display_prompt();

        /* Read user input */
        input = read_input();

        /* Handle EOF */
        if (input == NULL)
        {
            printf("\n");
            history_cleanup();
            break;
        }

        /* Ignore empty commands */
        if (input[0] == '\n' || input[0] == '\0')
        {
            free(input);
            continue;
        }

        /* Handle previous command execution (!!) */
        if (strcmp(input, "!!\n") == 0 ||
            strcmp(input, "!!") == 0)
        {
            const char *last_command = history_get_last();

            if (last_command == NULL)
            {
                printf("mini-shell: no commands in history\n");
                free(input);
                continue;
            }

            char *expanded = malloc(strlen(last_command) + 1);

            if (expanded == NULL)
            {
                perror("malloc");
                free(input);
                continue;
            }

            strcpy(expanded, last_command);

            free(input);
            input = expanded;

            printf("%s", input);
        }

        /* Handle history number execution (!n) */
        else if (input[0] == '!')
        {
            char *endptr;

            long number = strtol(input + 1, &endptr, 10);

            if (endptr == input + 1 ||
                (*endptr != '\n' && *endptr != '\0') ||
                number < 1 ||
                number > 2147483647)
            {
                printf("mini-shell: invalid history command\n");
                free(input);
                continue;
            }

            const char *previous_command =
                history_get_command((int)number);

            if (previous_command == NULL)
            {
                printf("mini-shell: history entry not found\n");
                free(input);
                continue;
            }

            char *expanded = malloc(strlen(previous_command) + 1);

            if (expanded == NULL)
            {
                perror("malloc");
                free(input);
                continue;
            }

            strcpy(expanded, previous_command);

            free(input);
            input = expanded;

            printf("%s", input);
        }

        /* Save command to persistent history */
        history_add(input);

        /* Reap background processes */
        reap_background_processes();

        /* Parse command line into words and operators. */
        int token_count = parse_input(input, args, MAX_ARGS, &background);

        if (token_count < 0)
        {
            free(input);
            continue;
        }

        /* Execute command line, including ;, && and || operators. */
        execute_line(args, background);

        free(input);
    }

    return 0;
}
