# Mini Shell — Final Project (Phases 1–3)

A learning-oriented Bash-like command interpreter written in C for Linux. It demonstrates command parsing, process creation, process groups, terminal job control, pipelines, redirection, built-in commands, persistent history, and defensive testing.

## 1. Build and run

Requirements:

- Linux / POSIX environment
- GCC or Clang
- GNU Make
- An interactive terminal (TTY)

```bash
make clean
make
./mini-shell
```

The shell intentionally requires an interactive terminal because foreground/background job control uses terminal process groups and `tcsetpgrp()`.

## 2. Implemented functionality

### Phase 1 — Core shell

- Custom prompt showing the current working directory
- Dynamic input reading with `getline()`
- External command execution with `fork()` and `execvp()`
- Foreground and background process execution
- Process groups and terminal ownership
- `SIGCHLD`, `SIGINT`, `SIGTSTP`, `SIGCONT` handling
- Job tracking with Running, Stopped and Done states
- Built-ins: `exit`, `cd`, `pwd`, `history`, `jobs`, `fg`, `bg`
- Pipelines
- Input/output redirection: `<`, `>` and `>>`
- Persistent history in `$HOME/.mini_shell_history`
- `!!` and `!n` history expansion

### Phase 2 — Advanced command handling

- Sequential command separators: `;`
- Conditional execution: `&&` and `||`
- Single-quoted and double-quoted arguments
- Basic backslash escaping
- Operators recognized adjacent to words (for example, `echo hi>out.txt`)
- Built-in commands with redirection
- Built-ins as pipeline stages, except `fg` and `bg`
- Exit status propagation from built-ins, external commands and the final process in a pipeline

### Phase 3 — Reliability and validation

- POSIX feature-test macro defined explicitly for portable declarations such as `getline()` and `sigaction()`
- Strict compiler build with `-Werror`, conversion, shadowing and format checks
- Parser unit tests for quoting, operators, escaping, empty quoted arguments, background markers and input limits
- Full command-line syntax prevalidation before any segment is executed, including conditionally skipped segments
- Validation of missing/invalid redirection filenames and malformed pipelines
- Preservation of quoted operator words such as `echo ">"` and `echo '|'` as literal arguments
- Safer persistent history loading with dynamic `getline()` and checked path construction
- Job registration validates process IDs and rejects duplicates
- Foreground wait bookkeeping errors cannot silently become a successful exit status
- Background child collection drains `waitpid(..., WNOHANG)` on each prompt, avoiding reliance on a separately checked signal flag
- AddressSanitizer and UndefinedBehaviorSanitizer build target
- Clang static analysis and GCC `-fanalyzer` validation
- Interactive regression and stress tests for pipelines, redirection, conditional execution, history persistence, repeated commands and terminal job control

## 3. Examples

```bash
pwd ; date ; ls
false && echo "not printed"
false || echo "printed after failure"
mkdir "My Folder" && cd "My Folder" ; pwd
echo "hello world"
echo 'single quoted argument'
echo "saved text" > "output file.txt"
cat "output file.txt"
history | wc -l
printf 'one\ntwo\n' | grep two && echo "pipeline succeeded"
sleep 10 &
jobs
```

A trailing `&` launches the final command/pipeline in the input line in the background. `fg` and `bg` are parent-shell job-control built-ins and are rejected inside pipelines.

## 4. Testing and analysis

### Strict compile

```bash
make strict
```

This builds with `-Werror -Wconversion -Wshadow -Wformat=2 -Wstrict-prototypes` in addition to the normal warnings.

### Parser unit tests

```bash
make test
```

### Interactive regression tests

Install `pexpect` if it is not already available:

```bash
python3 -m pip install --user pexpect
make integration-test
```

The integration suite starts the shell in a pseudo-terminal and tests command separators, `&&`, `||`, quotes, pipelines, redirection, malformed syntax, error recovery, long input, the 16-process pipeline boundary, 100 repeated commands, persistent history across sessions, background jobs, Ctrl+Z, `bg`, `fg` and Ctrl+C.

### AddressSanitizer and UndefinedBehaviorSanitizer

```bash
make sanitize
make integration-test
```

The sanitizer build enables address and undefined-behavior checks. The integration test exits the shell normally so leak checking can run at process exit.

### Static analysis

```bash
make analyze
make gcc-analyze
```

The first command uses `clang --analyze`; the second runs GCC's `-fanalyzer` across each source file. The equivalent manual GCC command is:

```bash
for file in src/*.c; do
    gcc -D_POSIX_C_SOURCE=200809L -std=c11 -Wall -Wextra -Wpedantic \
        -fanalyzer -Iinclude -c "$file" -o "/tmp/$(basename "${file%.c}").o"
done
```

Optional additional tools:

```bash
sudo apt install cppcheck valgrind
make cppcheck
valgrind --leak-check=full --show-leak-kinds=all \
    --track-origins=yes ./mini-shell
```

Valgrind must be run from an interactive terminal; enter several commands and then `exit` to finish the report. `cppcheck` and Valgrind are optional tools and are not required to build the shell.

## 5. Architecture and important system calls

| Module | Responsibility | Main APIs / concepts |
|---|---|---|
| `main.c` | Main read/parse/execute loop and history expansion | `getline` via input module, history, parser, executor |
| `input.c` | Read a complete input line | `getline()` |
| `parser.c` | Tokenize words, quotes and operators | character scanning, in-place tokenization |
| `executor.c` | Built-in dispatch, pipelines, redirection and command lists | `fork()`, `execvp()`, `pipe()`, `dup2()`, `open()`, `waitpid()` |
| `builtins.c` | Shell built-in commands | `chdir()`, `getcwd()`, job-control APIs |
| `process.c` | Process groups, terminal ownership, jobs and signals | `setpgid()`, `tcsetpgrp()`, `kill()`, `sigaction()`, `waitpid()` |
| `history.c` | In-memory and persistent command history | `malloc()`, `free()`, `getline()`, file I/O |
| `prompt.c` | Display current working directory prompt | `getcwd()` |

### Process execution flow

1. The shell reads a complete line.
2. The parser converts the line into words and operator tokens.
3. The executor validates command separators, pipelines and redirections before executing any segment.
4. Built-ins that must change shell state (`cd`, `fg`, `bg`, `exit`) run in the parent shell where appropriate.
5. External commands and pipeline stages are created with `fork()`.
6. Pipeline file descriptors are connected with `dup2()`.
7. External programs are started with `execvp()`.
8. The parent waits for foreground jobs with `waitpid()` and reaps background processes without blocking.
9. Process groups and terminal ownership make Ctrl+C and Ctrl+Z affect the foreground job rather than the shell.

## 6. Current limits / non-goals

This project is not a complete Bash replacement. It does not implement:

- Environment-variable expansion (`$HOME`), tilde expansion (`~/...`) or wildcard expansion (`*.c`)
- Command substitution (`$(date)`), shell scripts, functions, aliases or subshell syntax
- Here-documents, file-descriptor duplication (`2>`, `2>&1`) or append input redirection
- Full Bash quoting and escaping rules
- Arbitrary background separators in the middle of a command list; `&` is supported at the end of the line
- Full interactive line editing or a custom history-navigation interface

The implementation has fixed limits of 127 parsed tokens, 16 processes in one pipeline and 100 job slots. The in-memory history keeps the most recent 100 commands; persistent history remains in the user's history file. Very long terminal input is also subject to the terminal's canonical input buffer unless a line editor/raw-input mode is added.

## 7. Phase completion

- Phase 1: core shell behavior — completed
- Phase 2: advanced command handling — completed
- Phase 3: reliability, testing and documentation — completed to the scope described above

The project is ready for a final demonstration and review. Optional tools such as Valgrind and Cppcheck can be run on the target Linux machine as an additional independent check.
