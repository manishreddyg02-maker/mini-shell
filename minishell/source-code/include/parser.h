#ifndef PARSER_H
#define PARSER_H

#define MAX_ARGS 128

int parse_input(char *input, char **args, int max_args,
                int *background);

#endif