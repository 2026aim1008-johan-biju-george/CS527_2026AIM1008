#ifndef OS_H
#define OS_H

#include "memory.h"

#define MAX_TASKS 64
#define FILENAME_LEN 128
#define SCHED_QUANTUM 10 // instructions per task per round-robin turn

typedef enum {
   TASK_WAITING, // created, but no free processor yet
   TASK_READY, // assigned a processor, eligible for scheduling
   TASK_DONE // finished (HALT reached), processor released
} TaskState;

typedef struct {
   int pid; // unique task id, assigned at submission
   int proc_id; // which processor it's running on, -1 if none yet
   char program_byte[FILENAME_LEN]; // compiled instruction file for this task
   char data_byte[FILENAME_LEN]; // data file for this task
   TaskState state;
} Task;

void os_init(void);
int loader(const char *program_byte_filename, const char *data_byte_filename);
int map_pid_proc_id(int pid);
void os_task_finished(int pid);

/* Read-only helpers used by the scheduler and for debugging */
int os_ready_count(void);
int os_ready_pid_at(int index); // pid of the i-th task in the ready queue
int os_waiting_count(void);
Task *os_find_task(int pid); // NULL if pid unknown

void os_scheduler_tick(int quantum);
int os_active_task_count(void);

void shell(void);
int shell_is_active(void); // 0 once "exit" was entered or stdin closed

void scheduler(void);

#endif
