
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#include "executor.h"
#include "builtins.h"
#include "process.h"
#include "parser.h"

#define MAX_PIPE_COMMANDS MAX_PROCESSES_PER_JOB
#define MAX_COMMAND_LENGTH 256

/* Quoted operator words carry a private marker until parsing is complete. */
static const char *token_text(const char *token)
{
    if (token != NULL && token[0] == QUOTED_OPERATOR_MARKER)
        return token + 1;

    return token;
}

static void remove_quoted_operator_markers(char **args)
{
    if (args == NULL)
        return;

    for (int i = 0; args[i] != NULL; i++)
    {
        if (args[i][0] == QUOTED_OPERATOR_MARKER)
            memmove(args[i], args[i] + 1, strlen(args[i]));
    }
}


/* -------------------------------------------------- */
/* Restore default signals in child processes         */
/* -------------------------------------------------- */

static void restore_child_signals(void)
{
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
}


/* -------------------------------------------------- */
/* Build complete command string                      */
/* -------------------------------------------------- */

static void build_command_string(char **args,
                                 char *buffer,
                                 size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0)
        return;

    buffer[0] = '\0';

    if (args == NULL)
        return;

    for (int i = 0; args[i] != NULL; i++)
    {
        size_t used = strlen(buffer);

        if (used >= buffer_size - 1)
            break;

        size_t remaining = buffer_size - used - 1;

        if (i > 0)
        {
            strncat(buffer, " ", remaining);

            used = strlen(buffer);

            if (used >= buffer_size - 1)
                break;

            remaining = buffer_size - used - 1;
        }

        strncat(buffer, token_text(args[i]), remaining);
    }
}


/* -------------------------------------------------- */
/* Apply input/output redirection                     */
/* -------------------------------------------------- */

static int apply_redirection(char **args)
{
    if (args == NULL)
        return -1;

    int i = 0;

    while (args[i] != NULL)
    {
        if (args[i][0] == QUOTED_OPERATOR_MARKER)
        {
            i++;
            continue;
        }

        int target_fd = -1;
        int flags = 0;

        if (strcmp(args[i], "<") == 0)
        {
            target_fd = STDIN_FILENO;
            flags = O_RDONLY;
        }
        else if (strcmp(args[i], ">") == 0)
        {
            target_fd = STDOUT_FILENO;
            flags = O_WRONLY | O_CREAT | O_TRUNC;
        }
        else if (strcmp(args[i], ">>") == 0)
        {
            target_fd = STDOUT_FILENO;
            flags = O_WRONLY | O_CREAT | O_APPEND;
        }
        else
        {
            i++;
            continue;
        }

        if (args[i + 1] == NULL)
        {
            fprintf(stderr,
                    "Redirection error: missing filename\n");

            return -1;
        }

        const char *filename = token_text(args[i + 1]);

        int fd;

        if (target_fd == STDIN_FILENO)
        {
            fd = open(filename, flags);
        }
        else
        {
            fd = open(filename, flags, 0644);
        }

        if (fd == -1)
        {
            perror(filename);
            return -1;
        }

        if (dup2(fd, target_fd) == -1)
        {
            perror("dup2");
            close(fd);
            return -1;
        }

        close(fd);

        /*
         * Remove the redirection operator and filename
         * from the argument array.
         */

        int j = i;

        while (args[j + 2] != NULL)
        {
            args[j] = args[j + 2];
            j++;
        }

        args[j] = NULL;
    }

    return 0;
}


/* -------------------------------------------------- */
/* Check whether command is a built-in                */
/* -------------------------------------------------- */

static int is_builtin_command(const char *command)
{
    if (command == NULL)
        return 0;

    const char *builtins[] = {
        "exit",
        "cd",
        "pwd",
        "history",
        "jobs",
        "fg",
        "bg"
    };

    int count = sizeof(builtins) / sizeof(builtins[0]);

    for (int i = 0; i < count; i++)
    {
        if (strcmp(command, builtins[i]) == 0)
            return 1;
    }

    return 0;
}

/* Job-control built-ins must execute in the parent shell. */
static int is_job_control_builtin(const char *command)
{
    if (command == NULL)
        return 0;

    return (strcmp(command, "fg") == 0 ||
            strcmp(command, "bg") == 0);
}
/* -------------------------------------------------- */
/* Execute built-in with redirection                  */
/* -------------------------------------------------- */

static int execute_builtin_with_redirection(char **args)
{
    /*
     * Flush existing buffered output before changing
     * the standard file descriptors.
     */
    fflush(NULL);

    /* Save original standard input. */
    int saved_stdin = dup(STDIN_FILENO);

    if (saved_stdin == -1)
    {
        perror("dup stdin");
        return 1;
    }

    /* Save original standard output. */
    int saved_stdout = dup(STDOUT_FILENO);

    if (saved_stdout == -1)
    {
        perror("dup stdout");
        close(saved_stdin);
        return 1;
    }

    /*
     * Apply redirection.
     *
     * This removes redirection operators and filenames
     * from args, leaving the actual built-in arguments.
     */
    int builtin_status = 1;

    if (apply_redirection(args) == -1)
    {
        goto restore_descriptors;
    }

    /* Restore quoted operator words to their literal argv text. */
    remove_quoted_operator_markers(args);

    /* Execute built-in in the parent shell process. */
    builtin_status = execute_builtin(args);

restore_descriptors:

    /*
     * Flush output before restoring stdout.
     * This ensures buffered output reaches the file.
     */
    fflush(NULL);

    /* Restore original stdin. */
    if (dup2(saved_stdin, STDIN_FILENO) == -1)
        perror("restore stdin");

    /* Restore original stdout. */
    if (dup2(saved_stdout, STDOUT_FILENO) == -1)
        perror("restore stdout");

    close(saved_stdin);
    close(saved_stdout);

    return builtin_status;
}


/* -------------------------------------------------- */
/* Validate pipeline                                  */
/* -------------------------------------------------- */

static int validate_pipeline(char **args)
{
    if (args == NULL || args[0] == NULL)
        return -1;

    int command_count = 1;

    if (strcmp(args[0], "|") == 0)
    {
        fprintf(stderr, "Syntax error: empty command\n");
        return -1;
    }

    for (int i = 0; args[i] != NULL; i++)
    {
        if (strcmp(args[i], "|") != 0)
            continue;

        /*
         * A pipe cannot appear at the beginning,
         * at the end, or immediately after another pipe.
         */

        if (args[i + 1] == NULL ||
            (i > 0 && strcmp(args[i - 1], "|") == 0))
        {
            fprintf(stderr, "Syntax error near pipe\n");
            return -1;
        }

        command_count++;

        if (command_count > MAX_PIPE_COMMANDS)
        {
            fprintf(stderr,
                    "Maximum pipeline limit is %d commands\n",
                    MAX_PIPE_COMMANDS);

            return -1;
        }
    }

    return command_count;
}


/* -------------------------------------------------- */
/* Validate redirection syntax before starting jobs   */
/* -------------------------------------------------- */

static int is_redirection_operator(const char *token)
{
    return token != NULL && token[0] != QUOTED_OPERATOR_MARKER &&
           (strcmp(token, "<") == 0 ||
            strcmp(token, ">") == 0 ||
            strcmp(token, ">>") == 0);
}

static int validate_redirections(char **args)
{
    int stage_has_command = 0;

    for (int i = 0; args[i] != NULL; i++)
    {
        if (strcmp(args[i], "|") == 0)
        {
            if (!stage_has_command)
            {
                fprintf(stderr, "Syntax error: empty pipeline command\n");
                return -1;
            }

            stage_has_command = 0;
            continue;
        }

        if (is_redirection_operator(args[i]))
        {
            if (args[i + 1] == NULL ||
                is_redirection_operator(args[i + 1]) ||
                strcmp(args[i + 1], "|") == 0)
            {
                fprintf(stderr,
                        "Redirection error: missing or invalid filename\n");
                return -1;
            }

            i++; /* The next token is the filename. */
            continue;
        }

        stage_has_command = 1;
    }

    if (!stage_has_command)
    {
        fprintf(stderr, "Syntax error: empty pipeline command\n");
        return -1;
    }

    return 0;
}

/* Validate every command segment, including conditionally skipped ones. */
static int validate_command_segments(char **tokens, int token_count)
{
    int start = 0;

    while (start < token_count)
    {
        int end = start;

        while (end < token_count &&
               strcmp(tokens[end], ";") != 0 &&
               strcmp(tokens[end], "&&") != 0 &&
               strcmp(tokens[end], "||") != 0)
        {
            end++;
        }

        int count = end - start;

        if (count <= 0 || count >= MAX_ARGS)
        {
            fprintf(stderr, "mini-shell: invalid or oversized command segment\n");
            return -1;
        }

        char *candidate[MAX_ARGS];

        for (int i = 0; i < count; i++)
            candidate[i] = tokens[start + i];

        candidate[count] = NULL;

        if (validate_pipeline(candidate) == -1 ||
            validate_redirections(candidate) == -1)
        {
            return -1;
        }

        start = end + 1;
    }

    return 0;
}

/* -------------------------------------------------- */
/* Close all pipe descriptors                         */
/* -------------------------------------------------- */

static void close_all_pipes(int pipes[][2], int pipe_count)
{
    for (int i = 0; i < pipe_count; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}


/* -------------------------------------------------- */
/* Wait for a specific child process                  */
/* -------------------------------------------------- */

static void wait_for_child(pid_t pid)
{
    int status;

    while (waitpid(pid, &status, 0) == -1)
    {
        if (errno == EINTR)
            continue;

        if (errno == ECHILD)
            break;

        perror("waitpid");
        break;
    }
}


/* -------------------------------------------------- */
/* Terminate partially created pipeline              */
/* -------------------------------------------------- */

static void terminate_created_children(const pid_t pids[],
                                       int created,
                                       int launch_pipe[2],
                                       int pipes[][2],
                                       int pipe_count)
{
    /*
     * Kill children individually. They may still be
     * waiting at the launch synchronization pipe.
     */

    for (int i = 0; i < created; i++)
    {
        kill(pids[i], SIGTERM);
    }

    /*
     * Close launch pipe to release any child that
     * has not yet received the termination signal.
     */

    close(launch_pipe[0]);
    close(launch_pipe[1]);

    close_all_pipes(pipes, pipe_count);

    for (int i = 0; i < created; i++)
    {
        wait_for_child(pids[i]);
    }
}


/* -------------------------------------------------- */
/* Execute command                                    */
/* -------------------------------------------------- */

int execute_command(char **args, int background)
{
    if (args == NULL || args[0] == NULL)
        return 0;

    /*
     * Save original command before modifying
     * argument array for pipeline execution.
     */

    char command[MAX_COMMAND_LENGTH];

    build_command_string(args,
                         command,
                         sizeof(command));

    /*
     * Check whether this command contains a pipeline.
     */

    int command_count = validate_pipeline(args);

    if (command_count == -1)
        return 2;

    /*
     * Built-ins without pipelines execute in the shell
     * process so commands such as cd affect the shell.
     *
     * Apply temporary redirection when necessary.
     */

    if (command_count == 1 && is_builtin_command(args[0]))
    {
        return execute_builtin_with_redirection(args);
    }

    /*
     * Create command segments.
     *
     * Example:
     * args = {"ls", "|", "wc", "-l", NULL}
     *
     * commands[0] -> {"ls", NULL}
     * commands[1] -> {"wc", "-l", NULL}
     */

    char **commands[MAX_PIPE_COMMANDS];

    int command_index = 0;

    commands[0] = args;

    for (int i = 0; args[i] != NULL; i++)
    {
        if (strcmp(args[i], "|") == 0)
        {
            args[i] = NULL;

            command_index++;

            commands[command_index] = &args[i + 1];
        }
    }

    /*
     * Create ordinary pipeline pipes.
     */

    int pipes[MAX_PIPE_COMMANDS - 1][2];

    int pipe_count = command_count - 1;

    for (int i = 0; i < pipe_count; i++)
    {
        if (pipe(pipes[i]) == -1)
        {
            perror("pipe");

            close_all_pipes(pipes, i);
            return 1;
        }
    }

    /*
     * Create a launch synchronization pipe.
     *
     * Children wait for the parent to close the
     * write end before executing their commands.
     *
     * This gives the parent time to establish the
     * process group and transfer terminal ownership.
     */

    int launch_pipe[2];

    if (pipe(launch_pipe) == -1)
    {
        perror("launch pipe");

        close_all_pipes(pipes, pipe_count);
        return 1;
    }

    pid_t pids[MAX_PIPE_COMMANDS];

    pid_t pgid = 0;

    int created = 0;

    /* -------------------------------------------------- */
    /* Fork pipeline processes                            */
    /* -------------------------------------------------- */

    for (int i = 0; i < command_count; i++)
    {
        pid_t pid = fork();

        if (pid == -1)
        {
            perror("fork");

            terminate_created_children(pids,
                                       created,
                                       launch_pipe,
                                       pipes,
                                       pipe_count);

            return 1;
        }

        /* ---------------------------------------------- */
        /* Child process                                  */
        /* ---------------------------------------------- */

        if (pid == 0)
        {
            /*
             * Establish the process group.
             *
             * First child becomes the process-group leader.
             * Remaining children join the same group.
             */

            if (pgid == 0)
            {
                if (setpgid(0, 0) == -1)
                {
                    perror("child setpgid");
                    _exit(EXIT_FAILURE);
                }
            }
            else
            {
                if (setpgid(0, pgid) == -1)
                {
                    perror("child setpgid");
                    _exit(EXIT_FAILURE);
                }
            }

            restore_child_signals();

            /*
             * Child must not execute until the parent
             * has established the complete pipeline.
             */

            close(launch_pipe[1]);

            char buffer;

            while (1)
            {
                ssize_t bytes = read(launch_pipe[0],
                                     &buffer,
                                     sizeof(buffer));

                if (bytes == 0)
                    break;

                if (bytes == -1)
                {
                    if (errno == EINTR)
                        continue;

                    perror("launch synchronization");
                    _exit(EXIT_FAILURE);
                }
            }

            close(launch_pipe[0]);

            /*
             * Connect input from the previous command.
             */

            if (i > 0)
            {
                if (dup2(pipes[i - 1][0],
                         STDIN_FILENO) == -1)
                {
                    perror("dup2 input");
                    _exit(EXIT_FAILURE);
                }
            }

            /*
             * Connect output to the next command.
             */

            if (i < command_count - 1)
            {
                if (dup2(pipes[i][1],
                         STDOUT_FILENO) == -1)
                {
                    perror("dup2 output");
                    _exit(EXIT_FAILURE);
                }
            }

            /*
             * Close all original pipe descriptors.
             * Standard input/output now use dup2 copies.
             */

            close_all_pipes(pipes, pipe_count);

            /*
             * Apply explicit redirection.
             *
             * Explicit redirection overrides the
             * corresponding pipeline descriptor.
             */

            if (apply_redirection(commands[i]) == -1)
                _exit(EXIT_FAILURE);

            remove_quoted_operator_markers(commands[i]);

            if (commands[i][0] == NULL)
            {
                fprintf(stderr, "Empty command\n");
                _exit(EXIT_FAILURE);
            }

            /*
             * Check whether the command is a built-in.
             */
            if (is_builtin_command(commands[i][0]))
            {
                /*
                 * fg and bg require the parent shell's
                 * job-control and terminal management.
                 */
                if (is_job_control_builtin(commands[i][0]))
                {
                    fprintf(stderr,
                            "%s: cannot be used inside a pipeline\n",
                            commands[i][0]);

                    _exit(EXIT_FAILURE);
                }

                /*
                 * Execute the built-in inside this child process.
                 */
                int builtin_status = execute_builtin(commands[i]);

                /*
                 * Flush output because we are using _exit(),
                 * which does not automatically flush stdio buffers.
                 */
                fflush(NULL);

                _exit(builtin_status);
            }

            /*
             * Not a built-in: execute external command.
             */
            execvp(commands[i][0], commands[i]);

            perror(commands[i][0]);

            _exit(EXIT_FAILURE);
        }

        /* ---------------------------------------------- */
        /* Parent process                                 */
        /* ---------------------------------------------- */

        pids[created++] = pid;

        /*
         * The first child becomes the process-group leader.
         */

        if (pgid == 0)
            pgid = pid;

        /*
         * Parent also establishes the process group.
         * This avoids races with child process scheduling.
         */

        if (setpgid(pid, pgid) == -1)
        {
            if (errno != EACCES && errno != ESRCH)
            {
                perror("parent setpgid");

                terminate_created_children(pids,
                                           created,
                                           launch_pipe,
                                           pipes,
                                           pipe_count);

                return 1;
            }
        }
    }

    /* -------------------------------------------------- */
    /* All children have been created                     */
    /* -------------------------------------------------- */

    /*
     * Parent no longer needs the read end.
     */

    close(launch_pipe[0]);

    /*
     * Close ordinary pipes in the parent.
     */

    close_all_pipes(pipes, pipe_count);

    /*
     * Register the complete pipeline as one job.
     */

    int job_id = register_job(pgid,
                              pids,
                              created,
                              command);

    if (job_id == -1)
    {
        /*
         * Job registration failed.
         * Terminate all created children.
         */

        for (int i = 0; i < created; i++)
            kill(pids[i], SIGTERM);

        close(launch_pipe[1]);

        for (int i = 0; i < created; i++)
            wait_for_child(pids[i]);

        return 1;
    }

    /*
     * For foreground execution, transfer terminal
     * ownership before releasing the children.
     */

    if (!background)
    {
        give_terminal_to(pgid);
    }

    /*
     * Closing the write end releases all children
     * waiting for EOF on the launch synchronization pipe.
     */

    close(launch_pipe[1]);

    /* -------------------------------------------------- */
    /* Background execution                               */
    /* -------------------------------------------------- */

    if (background)
    {
        printf("[%d] %d\n",
               job_id,
               (int)pgid);

        return 0;
    }

    /* -------------------------------------------------- */
    /* Foreground execution                               */
    /* -------------------------------------------------- */

    return wait_for_foreground_job(job_id);
}


/* -------------------------------------------------- */
/* Execute a complete parsed command line             */
/* -------------------------------------------------- */

int execute_line(char **tokens, int background)
{
    if (tokens == NULL || tokens[0] == NULL)
        return 0;

    enum
    {
        CONNECTOR_NONE,
        CONNECTOR_SEQUENCE,
        CONNECTOR_AND,
        CONNECTOR_OR
    };

    int previous_connector = CONNECTOR_NONE;
    int last_status = 0;
    int start = 0;
    int token_count = 0;

    while (tokens[token_count] != NULL)
        token_count++;

    /*
     * Validate separator placement before executing anything.
     * This prevents a malformed trailing operator such as
     * "echo should-not-run &&" from partially executing.
     */
    if (strcmp(tokens[0], ";") == 0 ||
        strcmp(tokens[0], "&&") == 0 ||
        strcmp(tokens[0], "||") == 0 ||
        strcmp(tokens[token_count - 1], ";") == 0 ||
        strcmp(tokens[token_count - 1], "&&") == 0 ||
        strcmp(tokens[token_count - 1], "||") == 0)
    {
        fprintf(stderr, "mini-shell: syntax error near command separator\n");
        return 2;
    }

    if (validate_command_segments(tokens, token_count) == -1)
        return 2;

    for (int i = 0; i < token_count; i++)
    {
        int current_is_separator =
            strcmp(tokens[i], ";") == 0 ||
            strcmp(tokens[i], "&&") == 0 ||
            strcmp(tokens[i], "||") == 0;

        if (current_is_separator && i > 0)
        {
            if (strcmp(tokens[i - 1], ";") == 0 ||
                strcmp(tokens[i - 1], "&&") == 0 ||
                strcmp(tokens[i - 1], "||") == 0)
            {
                fprintf(stderr,
                        "mini-shell: syntax error near command separator\n");
                return 2;
            }
        }

        if (strcmp(tokens[i], "&") == 0 && i != token_count - 1)
        {
            fprintf(stderr,
                    "mini-shell: syntax error: '&' is only supported at the end\n");
            return 2;
        }
    }

    while (start < token_count)
    {
        int end = start;

        while (end < token_count &&
               strcmp(tokens[end], ";") != 0 &&
               strcmp(tokens[end], "&&") != 0 &&
               strcmp(tokens[end], "||") != 0)
        {
            if (strcmp(tokens[end], "&") == 0)
            {
                fprintf(stderr,
                        "mini-shell: syntax error: '&' is only supported at the end\n");
                return 2;
            }

            end++;
        }

        if (end == start)
        {
            fprintf(stderr, "mini-shell: syntax error near command separator\n");
            return 2;
        }

        int should_execute = 1;

        if (previous_connector == CONNECTOR_AND && last_status != 0)
            should_execute = 0;
        else if (previous_connector == CONNECTOR_OR && last_status == 0)
            should_execute = 0;

        if (should_execute)
        {
            char *command_args[MAX_ARGS];
            int argument_count = end - start;

            if (argument_count >= MAX_ARGS)
            {
                fprintf(stderr, "mini-shell: too many command arguments\n");
                return 2;
            }

            for (int i = 0; i < argument_count; i++)
                command_args[i] = tokens[start + i];

            command_args[argument_count] = NULL;

            int command_background =
                background && end == token_count;

            last_status = execute_command(command_args,
                                           command_background);
        }

        if (end == token_count)
            break;

        if (strcmp(tokens[end], ";") == 0)
            previous_connector = CONNECTOR_SEQUENCE;
        else if (strcmp(tokens[end], "&&") == 0)
            previous_connector = CONNECTOR_AND;
        else
            previous_connector = CONNECTOR_OR;

        start = end + 1;

        if (start == token_count)
        {
            fprintf(stderr,
                    "mini-shell: syntax error: missing command after operator\n");
            return 2;
        }
    }

    return last_status;
}
