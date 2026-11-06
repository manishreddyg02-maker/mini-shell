
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "history.h"

#define HISTORY_SIZE 100
#define HISTORY_FILE ".mini_shell_history"

static char *history[HISTORY_SIZE];
static int history_count = 0;
static int history_start_number = 1;
static int history_total = 0;

/* Store a command in the in-memory history */
static void store_command(const char *command)
{
    char *copy = malloc(strlen(command) + 1);

    if (copy == NULL)
    {
        perror("malloc");
        return;
    }

    strcpy(copy, command);

    if (history_count < HISTORY_SIZE)
    {
        history[history_count++] = copy;
    }
    else
    {
        free(history[0]);

        for (int i = 1; i < HISTORY_SIZE; i++)
        {
            history[i - 1] = history[i];
        }

        history[HISTORY_SIZE - 1] = copy;

        history_start_number++;
    }

    history_total++;
}

/* Build the persistent history path without silently truncating it. */
static int build_history_path(char *filepath, size_t capacity)
{
    const char *home = getenv("HOME");

    if (home == NULL || home[0] == '\0')
    {
        fprintf(stderr, "mini-shell: HOME directory not found\n");
        return -1;
    }

    int written = snprintf(filepath, capacity, "%s/%s", home, HISTORY_FILE);

    if (written < 0 || (size_t)written >= capacity)
    {
        fprintf(stderr, "mini-shell: history path is too long\n");
        return -1;
    }

    return 0;
}

/* Load previously saved commands. getline() supports long history entries. */
void history_load(void)
{
    char filepath[4096];

    if (build_history_path(filepath, sizeof(filepath)) == -1)
        return;

    FILE *file = fopen(filepath, "r");

    if (file == NULL)
    {
        if (errno != ENOENT)
            perror("mini-shell: history file");
        return;
    }

    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;

    while ((length = getline(&line, &capacity, file)) != -1)
    {
        if (length == 0 || (length == 1 && line[0] == '\n'))
            continue;

        store_command(line);
    }

    if (ferror(file))
        perror("mini-shell: reading history");

    free(line);

    if (fclose(file) == EOF)
        perror("mini-shell: closing history");
}

/* Add a command to memory and persistent storage. */
void history_add(const char *command)
{
    if (command == NULL || command[0] == '\0' || command[0] == '\n')
        return;

    store_command(command);

    char filepath[4096];

    if (build_history_path(filepath, sizeof(filepath)) == -1)
        return;

    FILE *file = fopen(filepath, "a");

    if (file == NULL)
    {
        perror("mini-shell: history file");
        return;
    }

    size_t length = strlen(command);

    if (fwrite(command, 1, length, file) != length)
        perror("mini-shell: writing history");

    if (length > 0 && command[length - 1] != '\n' && fputc('\n', file) == EOF)
        perror("mini-shell: writing history newline");

    if (fclose(file) == EOF)
        perror("mini-shell: closing history");
}

/* Display command history */
void history_display(void)
{
    for (int i = 0; i < history_count; i++)
    {
        printf("%d  %s",
               history_start_number + i,
               history[i]);

        if (history[i][strlen(history[i]) - 1] != '\n')
            printf("\n");
    }
}

/* Retrieve the latest command */
const char *history_get_last(void)
{
    if (history_count == 0)
        return NULL;

    return history[history_count - 1];
}

/* Retrieve command using history number */
const char *history_get_command(int number)
{
    if (number < history_start_number ||
        number > history_total)
    {
        return NULL;
    }

    int index = number - history_start_number;

    return history[index];
}

/* Free allocated memory */
void history_cleanup(void)
{
    for (int i = 0; i < history_count; i++)
    {
        free(history[i]);
        history[i] = NULL;
    }

    history_count = 0;
}
