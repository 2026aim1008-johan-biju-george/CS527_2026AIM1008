#ifndef OS_H
#define OS_H

#include "memory.h"
#include "processor.h"

#define MAX_TASKS 64
#define FILENAME_LEN 128
#define SCHED_QUANTUM 10 // instructions per task per round-robin turn

typedef enum {
   TASK_WAITING, // created, waiting for memory / slot
   TASK_READY, // eligible for scheduling
   TASK_DONE // finished (HALT reached), processor released
} TaskState;

// PCB
typedef struct {
   int pid; // unique task id, assigned at submission
   char program_byte[FILENAME_LEN]; // compiled instruction file for this task
   char data_byte[FILENAME_LEN]; // data file for this task
   TaskState state;
   Context ctx; // saved CPU state
   int mem_slot; // index into pageTable
} Task;

void os_init(void);
int loader(const char *program_byte_filename, const char *data_byte_filename);

/* Read-only helpers used by the scheduler and for debugging */
int os_current_pid(void);
int os_ready_count(void);
int os_ready_pid_at(int index); // pid of the i-th task in the ready queue
int os_waiting_count(void);
int os_active_task_count(void);
Task *os_find_task(int pid); // NULL if pid unknown

void os_scheduler_tick(int quantum);
void os_task_finished(int pid);

void shell(void);
int shell_is_active(void); // 0 once "exit" was entered or stdin closed

void scheduler(void);

#endif
