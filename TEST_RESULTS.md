# Phase 3 Validation Record

Validation performed against the final Phase 3 source tree in a Linux pseudo-terminal environment.

| Validation | Result | Notes |
|---|---|---|
| Strict GCC build | PASS | `-Werror -Wconversion -Wshadow -Wformat=2 -Wstrict-prototypes` |
| Parser unit tests | PASS | Tokenization, quotes, operators, escaping, empty arguments, background marker, unmatched quote, token limit and quoted operator literal |
| Clang static analysis | PASS | `make analyze`; no diagnostics reported |
| GCC static analyzer | PASS | `make gcc-analyze`; no diagnostics reported |
| AddressSanitizer | PASS | Integration suite completed with leak detection enabled and no sanitizer report |
| UndefinedBehaviorSanitizer | PASS | Integration suite completed with no UBSan report |
| Interactive command regression | PASS | `;`, `&&`, `||`, quoting, pipelines, redirection and exit-status behavior |
| Syntax/error recovery | PASS | Unmatched quotes, trailing separators, invalid pipelines, missing redirection target and unknown command |
| Stress checks | PASS | 3,000-character argument, 16-stage pipeline, rejection of 17-stage pipeline and 100 repeated commands |
| Persistent history | PASS | History entry verified after starting a second shell process with the same HOME |
| Job-control regression | PASS | Background jobs, `jobs`, Ctrl+Z, `bg`, `fg` and Ctrl+C |
| Valgrind | NOT RUN | Valgrind was not installed in the validation environment |
| Cppcheck | NOT RUN | Cppcheck was not installed in the validation environment |

## Reproduction

```bash
make strict
make test
make analyze
make gcc-analyze
make sanitize
make integration-test
```

`make integration-test` requires Python's `pexpect` package. Valgrind and Cppcheck are optional additional tools and can be run on a Linux machine where they are installed.
