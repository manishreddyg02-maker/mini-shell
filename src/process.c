#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>

#include "process.h"

#define MAX_COMMAND_LENGTH 256

typedef enum
{
    PROCESS_RUNNING,
    PROCESS_STOPPED,
    PROCESS_DONE
} ProcessState;

typedef struct
{
    pid_t pid;
    ProcessState state;
    int exit_status;
} Process;

typedef struct
{
    int job_id;
    pid_t pgid;

    char command[MAX_COMMAND_LENGTH];

    Process processes[MAX_PROCESSES_PER_JOB];
    int process_count;

    int occupied;
} Job;

static Job jobs[MAX_JOBS];

static int next_job_id = 1;

static pid_t shell_pgid;
static int shell_terminal;

static volatile sig_atomic_t child_changed = 0;


/* -------------------------------------------------- */
/* Signal handler                                     */
/* -------------------------------------------------- */

static void sigchld_handler(int signo)
{
    (void)signo;
    child_changed = 1;
}


/* -------------------------------------------------- */
/* Job lookup                                         */
/* -------------------------------------------------- */

static Job *find_job(int job_id)
{
    for (int i = 0; i < MAX_JOBS; i++)
    {
        if (jobs[i].occupied && jobs[i].job_id == job_id)
            return &jobs[i];
    }

    return NULL;
}

static Process *find_process(pid_t pid, Job **owner)
{
    for (int i = 0; i < MAX_JOBS; i++)
    {
        if (!jobs[i].occupied)
            continue;

        for (int j = 0; j < jobs[i].process_count; j++)
        {
            if (jobs[i].processes[j].pid == pid)
            {
                if (owner != NULL)
                    *owner = &jobs[i];

                return &jobs[i].processes[j];
            }
        }
    }

    return NULL;
}


/* -------------------------------------------------- */
/* Job state calculation                              */
/* -------------------------------------------------- */

static int job_is_done(const Job *job)
{
    for (int i = 0; i < job->process_count; i++)
    {
        if (job->processes[i].state != PROCESS_DONE)
            return 0;
    }

    return 1;
}

static int job_is_stopped(const Job *job)
{
    int active_processes = 0;

    for (int i = 0; i < job->process_count; i++)
    {
        if (job->processes[i].state == PROCESS_DONE)
            continue;

        active_processes++;

        if (job->processes[i].state != PROCESS_STOPPED)
            return 0;
    }

    return active_processes > 0;
}

/*
 * Returns true if at least one process in the job
 * is stopped, even if other pipeline processes are running.
 */
static int job_has_stopped_process(const Job *job)
{
    for (int i = 0; i < job->process_count; i++)
    {
        if (job->processes[i].state == PROCESS_STOPPED)
            return 1;
    }

    return 0;
}

static const char *job_status(const Job *job)
{
    if (job_is_done(job))
        return "Done";

    if (job_is_stopped(job))
        return "Stopped";

    return "Running";
}


/* -------------------------------------------------- */
/* Job registration                                   */
/* -------------------------------------------------- */

int register_job(pid_t pgid,
                 const pid_t pids[],
                 int process_count,
                 const char *command)
{
    if (pgid <= 0 ||
        pids == NULL ||
        process_count < 1 ||
        process_count > MAX_PROCESSES_PER_JOB)
    {
        fprintf(stderr, "Invalid job information\n");
        return -1;
    }

    for (int i = 0; i < process_count; i++)
    {
        if (pids[i] <= 0)
        {
            fprintf(stderr, "Invalid process ID in job information\n");
            return -1;
        }

        for (int j = 0; j < i; j++)
        {
            if (pids[i] == pids[j])
            {
                fprintf(stderr, "Duplicate process ID in job information\n");
                return -1;
            }
        }
    }

    int slot = -1;

    for (int i = 0; i < MAX_JOBS; i++)
    {
        if (!jobs[i].occupied || job_is_done(&jobs[i]))
        {
            slot = i;
            break;
        }
    }

    if (slot == -1)
    {
        fprintf(stderr, "Maximum job limit reached\n");
        return -1;
    }

    Job *job = &jobs[slot];

    memset(job, 0, sizeof(*job));

    job->job_id = next_job_id++;
    job->pgid = pgid;
    job->process_count = process_count;
    job->occupied = 1;

    snprintf(job->command,
             sizeof(job->command),
             "%s",
             command != NULL ? command : "unknown");

    for (int i = 0; i < process_count; i++)
    {
        job->processes[i].pid = pids[i];
        job->processes[i].state = PROCESS_RUNNING;
        job->processes[i].exit_status = 0;
    }

    return job->job_id;
}


/* -------------------------------------------------- */
/* Terminal management                                */
/* -------------------------------------------------- */

void initialize_shell_terminal(void)
{
    shell_terminal = STDIN_FILENO;

    if (!isatty(shell_terminal))
    {
        fprintf(stderr,
                "Mini shell requires an interactive terminal\n");

        exit(EXIT_FAILURE);
    }

    /*
     * Wait until this shell is running in the foreground.
     */
    while (tcgetpgrp(shell_terminal) != getpgrp())
    {
        kill(-getpgrp(), SIGTTIN);
    }

    /*
     * The shell ignores terminal-related signals.
     */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    /*
     * Put the shell into its own process group.
     */
    if (setpgid(0, 0) == -1 &&
        errno != EACCES &&
        errno != EPERM)
    {
        perror("setpgid");
        exit(EXIT_FAILURE);
    }

    shell_pgid = getpgrp();

    /*
     * Give the terminal to the shell.
     */
    if (tcsetpgrp(shell_terminal, shell_pgid) == -1)
    {
        perror("tcsetpgrp");
        exit(EXIT_FAILURE);
    }

    printf("Mini shell terminal initialized. PGID: %d\n",
           (int)shell_pgid);
}

void give_terminal_to(pid_t pgid)
{
    if (tcsetpgrp(shell_terminal, pgid) == -1)
        perror("tcsetpgrp");
}

void restore_shell_terminal(void)
{
    if (tcsetpgrp(shell_terminal, shell_pgid) == -1)
        perror("tcsetpgrp");
}


/* -------------------------------------------------- */
/* Signal setup                                       */
/* -------------------------------------------------- */

void setup_signal_handling(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = sigchld_handler;

    sigemptyset(&sa.sa_mask);

    /*
     * Do not use SA_NOCLDSTOP.
     * We need notifications when children stop.
     */
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1)
        perror("sigaction");
}


/* -------------------------------------------------- */
/* Update process state                               */
/* -------------------------------------------------- */

static void update_process_status(pid_t pid, int status)
{
    Process *process = find_process(pid, NULL);

    if (process == NULL)
        return;

    if (WIFEXITED(status))
    {
        process->state = PROCESS_DONE;
        process->exit_status = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        process->state = PROCESS_DONE;
        process->exit_status = 128 + WTERMSIG(status);
    }
    else if (WIFSTOPPED(status))
    {
        process->state = PROCESS_STOPPED;
    }
    else if (WIFCONTINUED(status))
    {
        process->state = PROCESS_RUNNING;
    }
}


/* -------------------------------------------------- */
/* Foreground waiting                                 */
/* -------------------------------------------------- */

int wait_for_foreground_job(int job_id)
{
    Job *job = find_job(job_id);

    if (job == NULL)
    {
        fprintf(stderr, "Job not found: %d\n", job_id);
        return -1;
    }

    give_terminal_to(job->pgid);

    int status;
    int result = 0;

    while (!job_is_done(job) && !job_is_stopped(job))
    {
        pid_t pid = waitpid(-job->pgid,
                            &status,
                            WUNTRACED | WCONTINUED);

        if (pid > 0)
        {
            update_process_status(pid, status);
            continue;
        }

        if (pid == -1 && errno == EINTR)
            continue;

        if (pid == -1 && errno == ECHILD)
        {
            /*
             * No waitable children remain in this group.
             * Reconcile any remaining active records to avoid
             * leaving the job permanently marked Running.
             */
            fprintf(stderr,
                    "mini-shell: waitpid reported no remaining children for job %d\n",
                    job->job_id);

            for (int i = 0; i < job->process_count; i++)
            {
                if (job->processes[i].state != PROCESS_DONE)
                {
                    job->processes[i].state = PROCESS_DONE;
                    job->processes[i].exit_status = 127;
                }
            }

            result = -1;
            break;
        }

        if (pid == -1)
        {
            perror("waitpid");
            result = -1;
            break;
        }
    }

    restore_shell_terminal();

    if (job_is_stopped(job))
    {
        printf("\n[%d]+ Stopped\t%s\n",
               job->job_id,
               job->command);

        return 128 + SIGTSTP;
    }

    if (result != 0)
        return 1;

    /* A pipeline's status is the status of its final process. */
    if (job->process_count > 0)
        return job->processes[job->process_count - 1].exit_status;

    return 0;
}


/* -------------------------------------------------- */
/* Foreground job                                     */
/* -------------------------------------------------- */

int foreground_job(int job_id)
{
    Job *job = find_job(job_id);

    if (job == NULL)
    {
        fprintf(stderr, "fg: no such job: %d\n", job_id);
        return -1;
    }

    if (job_is_done(job))
    {
        fprintf(stderr, "fg: job has already completed\n");
        return -1;
    }

    /*
     * Transfer terminal ownership before resuming.
     */
    give_terminal_to(job->pgid);

    /*
     * Resume the process group if any member is stopped.
     */
    if (job_has_stopped_process(job))
    {
        if (kill(-job->pgid, SIGCONT) == -1)
        {
            perror("fg: SIGCONT");
            restore_shell_terminal();
            return -1;
        }

        for (int i = 0; i < job->process_count; i++)
        {
            if (job->processes[i].state == PROCESS_STOPPED)
                job->processes[i].state = PROCESS_RUNNING;
        }
    }

    return wait_for_foreground_job(job_id);
}


/* -------------------------------------------------- */
/* Background job                                     */
/* -------------------------------------------------- */

int background_job(int job_id)
{
    Job *job = find_job(job_id);

    if (job == NULL)
    {
        fprintf(stderr, "bg: no such job: %d\n", job_id);
        return -1;
    }

    if (job_is_done(job))
    {
        fprintf(stderr, "bg: job has already completed\n");
        return -1;
    }

    /*
     * Do not send SIGCONT to an already-running job.
     */
    if (!job_has_stopped_process(job))
    {
        printf("bg: job %d is already running\n", job_id);
        return 0;
    }

    if (kill(-job->pgid, SIGCONT) == -1)
    {
        perror("bg: SIGCONT");
        return -1;
    }

    for (int i = 0; i < job->process_count; i++)
    {
        if (job->processes[i].state == PROCESS_STOPPED)
            job->processes[i].state = PROCESS_RUNNING;
    }

    printf("[%d]+ %s &\n",
           job->job_id,
           job->command);

    return 0;
}


/* -------------------------------------------------- */
/* Reap background processes                          */
/* -------------------------------------------------- */

void reap_background_processes(void)
{
    /*
     * Drain waitpid unconditionally. The signal flag is only a hint;
     * checking and clearing it separately can lose a SIGCHLD arriving
     * between those operations. WNOHANG makes this safe at each prompt.
     */
    child_changed = 0;

    int status;
    pid_t pid;

    while (1)
    {
        pid = waitpid(-1,
                      &status,
                      WNOHANG | WUNTRACED | WCONTINUED);

        if (pid > 0)
        {
            Job *job = NULL;

            Process *process = find_process(pid, &job);

            if (process == NULL)
                continue;

            int was_done = job_is_done(job);
            int was_stopped = job_is_stopped(job);

            update_process_status(pid, status);

            if (!was_done && job_is_done(job))
            {
                printf("\n[%d]+ Done\t%s\n",
                       job->job_id,
                       job->command);
            }
            else if (!was_stopped && job_is_stopped(job))
            {
                printf("\n[%d]+ Stopped\t%s\n",
                       job->job_id,
                       job->command);
            }

            continue;
        }

        if (pid == -1 && errno == EINTR)
            continue;

        if (pid == -1 && errno != ECHILD)
            perror("waitpid");

        break;
    }
}


/* -------------------------------------------------- */
/* Display jobs                                       */
/* -------------------------------------------------- */

void list_background_jobs(void)
{
    int found = 0;

    printf("\n%-8s %-10s %-12s %s\n",
           "JOB ID",
           "PGID",
           "STATUS",
           "COMMAND");

    printf("----------------------------------------------------------\n");

    for (int i = 0; i < MAX_JOBS; i++)
    {
        if (!jobs[i].occupied)
            continue;

        found = 1;

        printf("[%-7d] %-10d %-12s %s\n",
               jobs[i].job_id,
               (int)jobs[i].pgid,
               job_status(&jobs[i]),
               jobs[i].command);
    }

    if (!found)
        printf("No jobs found.\n");

    printf("\n");
}