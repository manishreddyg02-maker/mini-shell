#ifndef PARSER_H
#define PARSER_H

#define MAX_ARGS 128
#define QUOTED_OPERATOR_MARKER '\x1f'

/*
 * Tokenize one input line in place.
 * Quotes are removed and shell operators are returned as separate tokens.
 * Returns the token count, or -1 on a parsing error.
 * A trailing '&' is reported through background and removed from args.
 */
int parse_input(char *input, char **args, int max_args,
                int *background);

#endif
