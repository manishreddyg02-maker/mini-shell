#!/usr/bin/env python3
"""Interactive regression tests for mini-shell (requires Python pexpect)."""

import os
import re
import sys
import tempfile
import time
import pexpect


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} PATH_TO_MINI_SHELL", file=sys.stderr)
        return 2

    executable = os.path.abspath(sys.argv[1])
    home = tempfile.mkdtemp(prefix="mini-shell-test-home-")
    env = os.environ.copy()
    env["HOME"] = home
    env["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"

    child = pexpect.spawn(
        executable,
        cwd=os.path.dirname(executable),
        env=env,
        encoding="utf-8",
        timeout=12,
    )
    child.setecho(False)

    prompt = re.compile(r"mini-shell:.*\$ ")
    child.expect(prompt)

    def run(command, expected=None, forbidden=None, label=None, report=True):
        child.sendline(command)
        if expected is not None:
            child.expect_exact(expected)
        child.expect(prompt)
        output = child.before
        if forbidden and any(item in output for item in forbidden):
            raise AssertionError(f"{command!r}: unexpected output {output!r}")
        if report:
            print(f"PASS {label if label is not None else command}")
        return output

    try:
        run("echo one ; echo two ; echo three", "one\r\ntwo\r\nthree\r\n")
        run("false && echo should-not-run", forbidden=["should-not-run"])
        run("false || echo recovered", "recovered\r\n")
        run("true || echo should-not-run", forbidden=["should-not-run"])
        run("echo ok && echo yes || echo no", "ok\r\nyes\r\n", ["no\r\n"])
        run('echo "Hello World"', "Hello World\r\n")
        run("echo 'a;b && c'", "a;b && c\r\n")
        run('echo ">"', ">\r\n")
        run("echo '|' | wc -c", "2\r\n")
        run("echo literal > '>' ; cat '>'", "literal\r\n")
        run("printf 'alpha\\nbeta\\n' | wc -l", "2\r\n")
        run("echo hello | grep missing || echo no-match", "no-match\r\n")
        run('echo "quoted file" > "quoted output.txt" ; cat "quoted output.txt"',
            "quoted file\r\n")
        run("false ; echo continued", "continued\r\n")

        child.sendline("echo before &&")
        child.expect_exact("mini-shell: syntax error near command separator")
        child.expect(prompt)
        print("PASS malformed trailing conditional operator")

        child.sendline('echo "unclosed')
        child.expect_exact("mini-shell: syntax error: unmatched quote")
        child.expect(prompt)
        print("PASS unmatched quote handling")

        child.sendline("false && echo should-not-run |")
        child.expect_exact("Syntax error near pipe")
        child.expect(prompt)
        print("PASS syntax validation of skipped conditional branch")

        child.sendline("false && echo should-not-run >")
        child.expect_exact("Redirection error: missing or invalid filename")
        child.expect(prompt)
        print("PASS redirection validation of skipped conditional branch")

        run("echo shell-still-alive", "shell-still-alive\r\n")
        run("echo bad >", "Redirection error: missing or invalid filename")
        run("echo broken |", "Syntax error near pipe")
        run("mini_shell_command_that_does_not_exist",
            "mini_shell_command_that_does_not_exist: No such file or directory")
        run("echo recovered-after-errors", "recovered-after-errors\r\n")

        # A long single argument below the terminal canonical input limit exercises getline(), parser bounds and output.
        long_word = "L" * 3000
        run("echo " + long_word, long_word + "\r\n", label="long input (3000 characters)")

        # Exercise the maximum supported pipeline width (16 processes).
        pipeline = "printf x" + " | cat" * 14 + " | wc -c"
        run(pipeline, "1\r\n")

        # More than the process limit must be rejected before any fork.
        oversized_pipeline = "printf x" + " | cat" * 15 + " | wc -c"
        run(oversized_pipeline, "Maximum pipeline limit is 16 commands")

        # Repeated commands exercise allocation/free paths and history growth.
        for index in range(100):
            run("true", report=False)
        print("PASS 100-command repetition stress test")

        child.sendline("sleep 2 &")
        child.expect(prompt)
        print("PASS background process launch")
        run("jobs")

        child.sendline("sleep 3")
        child.expect(prompt, timeout=8)
        print("PASS foreground wait while background process completes")
        run("jobs")

        # Verify terminal job control with real Ctrl+Z and Ctrl+C keystrokes.
        child.sendline("sleep 30")
        time.sleep(0.3)
        child.sendcontrol("z")
        child.expect_exact("Stopped")
        stopped_line = child.before
        child.expect(prompt)
        match = re.search(r"\[(\d+)\]", stopped_line)
        if match is None:
            raise AssertionError(f"could not read stopped job ID: {stopped_line!r}")
        job_id = match.group(1)
        print("PASS Ctrl+Z stops foreground process")

        run("jobs", "Stopped")
        run(f"bg %{job_id}")
        child.sendline(f"fg %{job_id}")
        time.sleep(0.3)
        child.sendcontrol("c")
        child.expect(prompt)
        print("PASS bg/fg and Ctrl+C foreground termination")
        run("echo signal-shell-ok", "signal-shell-ok\r\n")

        run("echo persistence-marker", "persistence-marker\r\n")

        child.sendline("exit")
        child.expect(pexpect.EOF)

        # Start a second shell with the same HOME to verify persistence.
        second = pexpect.spawn(
            executable, cwd=os.path.dirname(executable), env=env,
            encoding="utf-8", timeout=12
        )
        second.setecho(False)
        second.expect(prompt)
        second.sendline("history")
        second.expect_exact("echo persistence-marker")
        second.expect(prompt)
        second.sendline("exit")
        second.expect(pexpect.EOF)
        print("PASS persistent history across shell sessions")

        for test_filename in (">", "quoted output.txt"):
            test_file = os.path.join(os.path.dirname(executable), test_filename)
            if os.path.exists(test_file):
                os.remove(test_file)

        print("All interactive regression tests passed.")
        return 0
    finally:
        if child.isalive():
            child.terminate(force=True)


if __name__ == "__main__":
    sys.exit(main())
