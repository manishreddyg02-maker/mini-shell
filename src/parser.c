#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "parser.h"

/*
 * Tokenize a command line in place.
 *
 * Words inside single or double quotes remain one argument, and quote
 * characters are removed. Operators outside quotes become separate tokens:
 * ; && || | < > >> &
 *
 * The returned argument array points into input or to static operator strings.
 */
int parse_input(char *input, char **args, int max_args, int *background)
{
    static const char *op_semicolon = ";";
    static const char *op_and = "&&";
    static const char *op_or = "||";
    static const char *op_pipe = "|";
    static const char *op_in = "<";
    static const char *op_out = ">";
    static const char *op_append = ">>";
    static const char *op_background = "&";

    if (input == NULL || args == NULL || max_args < 2 ||
        background == NULL)
        return -1;

    *background = 0;

    char *read_ptr = input;
    char *write_ptr = input;
    int count = 0;

    while (*read_ptr != '\0')
    {
        while (isspace((unsigned char)*read_ptr))
            read_ptr++;

        if (*read_ptr == '\0')
            break;

        if (count >= max_args - 1)
        {
            fprintf(stderr, "mini-shell: too many tokens (maximum %d)\n",
                    max_args - 1);
            return -1;
        }

        /* Operators are recognized only outside quoted words. */
        const char *operator_token = NULL;
        size_t operator_length = 0;

        if (*read_ptr == ';')
        {
            operator_token = op_semicolon;
            operator_length = 1;
        }
        else if (*read_ptr == '&')
        {
            if (read_ptr[1] == '&')
            {
                operator_token = op_and;
                operator_length = 2;
            }
            else
            {
                operator_token = op_background;
                operator_length = 1;
            }
        }
        else if (*read_ptr == '|')
        {
            if (read_ptr[1] == '|')
            {
                operator_token = op_or;
                operator_length = 2;
            }
            else
            {
                operator_token = op_pipe;
                operator_length = 1;
            }
        }
        else if (*read_ptr == '<')
        {
            operator_token = op_in;
            operator_length = 1;
        }
        else if (*read_ptr == '>')
        {
            if (read_ptr[1] == '>')
            {
                operator_token = op_append;
                operator_length = 2;
            }
            else
            {
                operator_token = op_out;
                operator_length = 1;
            }
        }

        if (operator_token != NULL)
        {
            read_ptr += operator_length;
            args[count++] = (char *)operator_token;
            continue;
        }

        char *word_start = write_ptr;
        int token_started = 0;
        int word_was_quoted = 0;
        char quote = '\0';

        while (*read_ptr != '\0')
        {
            char ch = *read_ptr;

            if (quote == '\0')
            {
                if (isspace((unsigned char)ch))
                    break;

                if (ch == ';' || ch == '&' || ch == '|' ||
                    ch == '<' || ch == '>')
                    break;

                if (ch == '\'' || ch == '"')
                {
                    quote = ch;
                    word_was_quoted = 1;
                    token_started = 1;
                    read_ptr++;
                    continue;
                }

                if (ch == '\\' && read_ptr[1] != '\0')
                {
                    read_ptr++;
                    *write_ptr++ = *read_ptr++;
                    token_started = 1;
                    continue;
                }

                *write_ptr++ = *read_ptr++;
                token_started = 1;
            }
            else
            {
                if (ch == quote)
                {
                    quote = '\0';
                    read_ptr++;
                    continue;
                }

                /*
                 * Inside single quotes, backslashes are literal.
                 * Inside double quotes, a backslash escapes the next
                 * character for this mini-shell's simple quote model.
                 */
                if (ch == '\\' && quote == '"' && read_ptr[1] != '\0')
                {
                    read_ptr++;
                    *write_ptr++ = *read_ptr++;
                    continue;
                }

                *write_ptr++ = *read_ptr++;
            }
        }

        if (quote != '\0')
        {
            fprintf(stderr, "mini-shell: syntax error: unmatched quote\n");
            return -1;
        }

        if (!token_started)
            continue;

        /*
         * Preserve the distinction between an operator token and a quoted
         * word whose contents happen to be exactly an operator (for example,
         * echo ">"). The marker is removed immediately before argv reaches
         * a built-in or external program.
         */
        size_t word_length = (size_t)(write_ptr - word_start);
        if (word_was_quoted &&
            ((word_length == 1 &&
              (word_start[0] == ';' || word_start[0] == '|' ||
               word_start[0] == '<' || word_start[0] == '>' ||
               word_start[0] == '&')) ||
             (word_length == 2 &&
              ((word_start[0] == '&' && word_start[1] == '&') ||
               (word_start[0] == '|' && word_start[1] == '|') ||
               (word_start[0] == '>' && word_start[1] == '>')))))
        {
            memmove(word_start + 1, word_start, word_length);
            word_start[0] = QUOTED_OPERATOR_MARKER;
            write_ptr++;
        }

        /*
         * Advance past the delimiter before writing the terminating NUL.
         * This is important when read_ptr == write_ptr.
         */
        const char *following_operator = NULL;
        size_t following_operator_length = 0;

        if (isspace((unsigned char)*read_ptr))
        {
            while (isspace((unsigned char)*read_ptr))
                read_ptr++;
        }
        else if (*read_ptr == ';')
        {
            following_operator = op_semicolon;
            following_operator_length = 1;
        }
        else if (*read_ptr == '&')
        {
            if (read_ptr[1] == '&')
            {
                following_operator = op_and;
                following_operator_length = 2;
            }
            else
            {
                following_operator = op_background;
                following_operator_length = 1;
            }
        }
        else if (*read_ptr == '|')
        {
            if (read_ptr[1] == '|')
            {
                following_operator = op_or;
                following_operator_length = 2;
            }
            else
            {
                following_operator = op_pipe;
                following_operator_length = 1;
            }
        }
        else if (*read_ptr == '<')
        {
            following_operator = op_in;
            following_operator_length = 1;
        }
        else if (*read_ptr == '>')
        {
            if (read_ptr[1] == '>')
            {
                following_operator = op_append;
                following_operator_length = 2;
            }
            else
            {
                following_operator = op_out;
                following_operator_length = 1;
            }
        }

        if (following_operator != NULL)
            read_ptr += following_operator_length;

        *write_ptr++ = '\0';
        args[count++] = word_start;

        if (following_operator != NULL)
        {
            if (count >= max_args - 1)
            {
                fprintf(stderr, "mini-shell: too many tokens (maximum %d)\n",
                        max_args - 1);
                return -1;
            }

            args[count++] = (char *)following_operator;
        }
    }

    args[count] = NULL;

    /* Background marker is supported at the end of the input line. */
    if (count > 0 && strcmp(args[count - 1], "&") == 0)
    {
        *background = 1;
        args[--count] = NULL;
    }

    return count;
}
