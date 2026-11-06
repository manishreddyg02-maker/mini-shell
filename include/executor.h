#ifndef EXECUTOR_H
#define EXECUTOR_H

/*
 * Execute one command or pipeline.
 * Returns the command exit status (0 means success).
 */
int execute_command(char **args, int background);

/*
 * Execute a parsed command line containing ;, && and ||.
 */
int execute_line(char **tokens, int background);

#endif
