# Phase 2 manual regression test plan

Run the shell from an interactive terminal after `make`.

| Test | Command | Expected result |
|---|---|---|
| Quoted argument | `echo "hello world"` | Prints `hello world` on one line |
| Single quotes | `echo 'hello world'` | Prints `hello world` on one line |
| Separator | `echo one ; echo two ; echo three` | Prints all three lines in order |
| Continue after failure | `false ; echo continued` | Prints `continued` |
| AND success | `true && echo success` | Prints `success` |
| AND failure | `false && echo skipped` | Does not print `skipped` |
| OR failure | `false || echo recovered` | Prints `recovered` |
| OR success | `true || echo skipped` | Does not print `skipped` |
| Left-to-right chain | `false && echo no || echo yes` | Prints `yes` |
| Quoted operators | `echo "a;b && c"` | Prints `a;b && c` as literal text |
| Pipe | `printf "a\\nb\\n" \| wc -l` | Prints `2` |
| Pipeline status | `false \| true && echo success` | Prints `success` |
| Pipeline failure | `true \| false || echo recovered` | Prints `recovered` |
| Built-in redirection | `pwd > location.txt && cat location.txt` | Writes and then displays the working directory |
| Append redirection | `echo first > f.txt ; echo second >> f.txt ; cat f.txt` | Displays both lines |
| Built-in pipeline | `history \| wc -l` | Displays a line count |
| Built-in in pipeline | `pwd \| cat` | Displays the working directory |
| Job-control restriction | `fg %1 \| cat` | Reports that `fg` cannot be used inside a pipeline |
| Background job | `sleep 10 &` | Prints a job ID and process-group ID |
| Job listing | `jobs` | Displays known jobs and their states |
| Stop/resume | Run `sleep 10`, press Ctrl+Z, then `bg %job_id` | Job changes to Stopped, then Running |
| Foreground resume | Run `fg %job_id` | Job resumes in foreground |
| Interrupt | Run `sleep 10`, press Ctrl+C | Child terminates; shell prompt remains |
| Shell survives pipeline exit | `exit \| cat ; echo alive` | Prints `alive` |
| Unmatched quote | `echo "unfinished` | Reports a parse error; shell remains usable |
| Invalid separator | `echo should-not-run &&` | Reports a syntax error without running the command |

The shell's prompt and terminal-control behavior require an interactive terminal; piping a test script into the program is not a valid substitute for the job-control tests.
