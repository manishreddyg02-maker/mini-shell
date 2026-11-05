#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "process.h"

static volatile sig_atomic_t child_changed = 0;

/* Signal handler */
static void sigchld_handler(int signo)
{
    (void)signo;

    child_changed = 1;
}

/* Register SIGCHLD handler */
void setup_signal_handling(void)
{
    struct sigaction sa;

    sa.sa_handler = sigchld_handler;

    sigemptyset(&sa.sa_mask);

    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
    }
}

/* Collect terminated child processes */
void reap_background_processes(void)
{
    if (!child_changed)
    {
        return;
    }

    child_changed = 0;

    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        printf("\n[Background] Process %d completed\n", pid);
    }

    if (pid == -1 && errno != ECHILD && errno != EINTR)
    {
        perror("waitpid");
    }
}