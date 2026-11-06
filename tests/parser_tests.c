#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

static int failures = 0;

static void expect_tokens(const char *label,
                          const char *source,
                          const char *expected[],
                          int expected_count)
{
    char input[1024];
    char *tokens[MAX_ARGS];
    int background = 0;

    if (strlen(source) >= sizeof(input))
    {
        fprintf(stderr, "FAIL %s: test input too long\n", label);
        failures++;
        return;
    }

    strcpy(input, source);
    int count = parse_input(input, tokens, MAX_ARGS, &background);

    if (count != expected_count)
    {
        fprintf(stderr,
                "FAIL %s: expected %d tokens, received %d\n",
                label, expected_count, count);
        failures++;
        return;
    }

    for (int i = 0; i < expected_count; i++)
    {
        if (strcmp(tokens[i], expected[i]) != 0)
        {
            fprintf(stderr,
                    "FAIL %s: token %d expected <%s>, received <%s>\n",
                    label, i, expected[i], tokens[i]);
            failures++;
            return;
        }
    }

    printf("PASS %s\n", label);
}

static void expect_background(const char *label, const char *source)
{
    char input[128];
    char *tokens[MAX_ARGS];
    int background = 0;

    strcpy(input, source);
    int count = parse_input(input, tokens, MAX_ARGS, &background);

    if (count != 1 || background != 1 || strcmp(tokens[0], "sleep") != 0)
    {
        fprintf(stderr, "FAIL %s: trailing background marker not parsed\n", label);
        failures++;
        return;
    }

    printf("PASS %s\n", label);
}

static void expect_parse_error(const char *label, const char *source)
{
    char input[128];
    char *tokens[MAX_ARGS];
    int background = 0;

    strcpy(input, source);
    int count = parse_input(input, tokens, MAX_ARGS, &background);

    if (count != -1)
    {
        fprintf(stderr, "FAIL %s: expected parser error, received %d\n",
                label, count);
        failures++;
        return;
    }

    printf("PASS %s\n", label);
}

static void expect_too_many_tokens(void)
{
    char input[512];
    char *tokens[4];
    int background = 0;

    strcpy(input, "one two three four");
    int count = parse_input(input, tokens, 4, &background);

    if (count != -1)
    {
        fprintf(stderr,
                "FAIL maximum token limit: expected -1, received %d\n",
                count);
        failures++;
        return;
    }

    printf("PASS maximum token limit\n");
}

static void expect_quoted_operator_literal(void)
{
    char input[] = "echo \">\"";
    char *tokens[MAX_ARGS];
    int background = 0;
    int count = parse_input(input, tokens, MAX_ARGS, &background);

    if (count != 2 || tokens[1][0] != QUOTED_OPERATOR_MARKER ||
        strcmp(tokens[1] + 1, ">") != 0)
    {
        fprintf(stderr, "FAIL quoted operator literal\n");
        failures++;
        return;
    }

    printf("PASS quoted operator literal\n");
}

int main(void)
{
    const char *basic[] = {"echo", "hello", "world"};
    expect_tokens("whitespace tokenization", "echo hello world", basic, 3);

    const char *quotes[] = {"echo", "hello world", "mini shell"};
    expect_tokens("single and double quotes",
                  "echo \"hello world\" 'mini shell'", quotes, 3);

    const char *operators[] = {
        "echo", "one", ";", "echo", "two", "&&", "echo", "three",
        "||", "echo", "four", "|", "wc", "-l", ">>", "result.txt"
    };
    expect_tokens("operator tokenization",
                  "echo one;echo two&&echo three||echo four|wc -l>>result.txt",
                  operators, (int)(sizeof(operators) / sizeof(operators[0])));

    const char *empty_quote[] = {"echo", "", "end"};
    expect_tokens("empty quoted argument", "echo \"\" end", empty_quote, 3);

    const char *escaped_space[] = {"echo", "two words"};
    expect_tokens("escaped whitespace", "echo two\\ words", escaped_space, 2);

    const char *single_quote_backslash[] = {"echo", "a\\nb"};
    expect_tokens("literal backslash in single quotes",
                  "echo 'a\\nb'", single_quote_backslash, 2);

    expect_background("background marker", "sleep &");
    expect_parse_error("unmatched quote", "echo 'unfinished");
    expect_too_many_tokens();
    expect_quoted_operator_literal();

    if (failures != 0)
    {
        fprintf(stderr, "%d parser test(s) failed\n", failures);
        return EXIT_FAILURE;
    }

    printf("All parser tests passed.\n");
    return EXIT_SUCCESS;
}
