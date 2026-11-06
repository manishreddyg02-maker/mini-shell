#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h>

#define MAX_JOBS 100
#define MAX_PROCESSES_PER_JOB 16

int register_job(pid_t pgid,
                 const pid_t pids[],
                 int process_count,
                 const char *command);

int wait_for_foreground_job(int job_id);

int foreground_job(int job_id);

int background_job(int job_id);

void list_background_jobs(void);

void reap_background_processes(void);

void initialize_shell_terminal(void);

void give_terminal_to(pid_t pgid);

void restore_shell_terminal(void);

void setup_signal_handling(void);

#endif