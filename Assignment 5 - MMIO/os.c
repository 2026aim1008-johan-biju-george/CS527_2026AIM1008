#include "os.h"
#include "memory.h"
#include "processor.h"
#include "compiler.h"
#include "devices.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h> /* _kbhit(), _getch() - non-blocking console input on Windows */
#else
#include <fcntl.h> /* F_GETFL/F_SETFL/O_NONBLOCK - not available via MinGW on Windows */
#endif
#include <unistd.h> /* usleep() - available on both POSIX and MinGW */

static Task tasks[MAX_TASKS];
static int task_count = 0;
static int next_pid = 1;

/* Shell state */
static char shell_buf[256];
static int shell_buf_len = 0;
static int shell_active = 1;
static int shell_prompted = 0;

/* Holds the pid currently assigned to processor */
static int proc_to_pid[NP];

/* Ready/waiting queues: plain ordered lists of pids, supporting append
   and remove-by-value. Since the scheduler needs to pull a specific pid 
   out of the middle of ready queue the moment it finishes (HALT), a simple
   shift-on-remove array is a better fit here than a strict FIFO. */
static int ready_queue[MAX_TASKS];
static int ready_count = 0;

static int waiting_queue[MAX_TASKS];
static int waiting_count = 0;

static void ready_push(int pid){
    ready_queue[ready_count++] = pid;
}

/* Removes the first occurrence of process id from ready queue, if present. */
static void ready_remove(int pid){
    for(int i = 0; i < ready_count; i++){
        if(ready_queue[i] == pid){
            for(int j = i; j < ready_count - 1; j++){
                ready_queue[j] = ready_queue[j + 1];
            }
            ready_count--;
            return;
        }
    }
}

/* Pushes process to waiting queue if processor is full */
static void waiting_push(int pid){
    waiting_queue[waiting_count++] = pid;
}

/* Removes process from waiting queue */
static int waiting_pop_front(void){
    int pid = waiting_queue[0];
    for(int j = 0; j < waiting_count - 1; j++){
        waiting_queue[j] = waiting_queue[j + 1];
    }
    waiting_count--;
    return pid;
}

/* Looks at (without removing) the pid at front of waiting queue */
static int waiting_peek_front(void){
    return waiting_queue[0];
}

/* Returns number of tasks in ready queue */
int os_ready_count(void){
    return ready_count;
}

/* Returns number of tasks in waiting queue */
int os_waiting_count(void){
    return waiting_count;
}

/* Returns the pid at index in ready queue, or -1 if index is out of bounds */
int os_ready_pid_at(int index){
    if(index < 0 || index >= ready_count) return -1;
    return ready_queue[index];
}

/* Returns the Task struct pointer for the given pid, or NULL if not found */
Task *os_find_task(int pid){
    for(int i = 0; i < task_count; i++){
        if(tasks[i].pid == pid) return &tasks[i];
    }
    return NULL;
}

/* proc_to_pid[p] == -1 means processor p is free */
static int find_free_processor(void){
    for(int p = 0; p < NP; p++){
        if(proc_to_pid[p] == -1) return p;
    }
    return -1;
}

/* Resets all queues, frees all processors, and clears the task table. */
void os_init(void){
    task_count = 0;
    next_pid = 1;
    ready_count = 0;
    waiting_count = 0;
    for(int p = 0; p < NP; p++){
        proc_to_pid[p] = -1;
    }
    dev_reset();
}

/* LOADER:
   Takes program.byte and data.byte as input.
   - If a processor is free: assigns it, initializes that processor's
     memory from the given files, resets it, and places the task in the
     ready queue.
   - If no processor is free: the task is placed in the waiting queue
     with no processor assigned yet (proc_id == -1).
   Returns the new task's pid. */
int loader(const char *program_byte_filename, const char *data_byte_filename){
    if(task_count >= MAX_TASKS){
        printf("Loader: task table full, cannot accept new task\n");
        return -1;
    }

    Task *t = &tasks[task_count++];
    t->pid = next_pid++;
    strncpy(t->program_byte, program_byte_filename, FILENAME_LEN - 1);
    t->program_byte[FILENAME_LEN - 1] = '\0';
    strncpy(t->data_byte, data_byte_filename, FILENAME_LEN - 1);
    t->data_byte[FILENAME_LEN - 1] = '\0';

    int needed = count_pages_needed(t->program_byte, t->data_byte);
    if(needed == -1){
        printf("Loader: pid %d failed to load (bad program/data file) - task rejected\n", t->pid);
        t->state = TASK_DONE;
        t->proc_id = -1;
        return t->pid;
    }

    int proc_id = find_free_processor();
    int avail = free_page_count();
    int not_enough_memory = (needed > avail);

    if(proc_id != -1 && !not_enough_memory){
        int result = initialize(proc_id, t->program_byte, t->data_byte);
        if(result == 1){
            t->proc_id = proc_id;
            t->state = TASK_READY;
            proc_to_pid[proc_id] = t->pid;
            reset(proc_id);
            ready_push(t->pid);
            printf("Loader: pid %d assigned to processor %d (ready)\n", t->pid, proc_id);
        } else {
            printf("Loader: pid %d failed to load unexpectedly - task rejected\n", t->pid);
            t->state = TASK_DONE;
            t->proc_id = -1;
        }
    } else {
        t->proc_id = -1;
        t->state = TASK_WAITING;
        waiting_push(t->pid);
        if(proc_id == -1){
            printf("Loader: no free processor, pid %d placed in waiting queue\n", t->pid);
        } else {
            printf("Loader: not enough free memory for pid %d (needs %d pages, %d free) - placed in waiting queue\n",
                t->pid, needed, avail);
        }
    }
    return t->pid;
}

/* Returns the processor-id currently running the given pid, or -1 if
   that pid is not currently assigned to any processor (waiting or
   unknown pid). */
int map_pid_proc_id(int pid){
    Task *t = os_find_task(pid);
    if(!t || t->state != TASK_READY) return -1;
    return t->proc_id;
}

/* Called when the running processor's pid reaches HALT. Frees that processor, 
   marks the task as done, and if any task is waiting - checks for available pages
   and then pops the oldest waiting task, assigns it the freed processor, initializes
   and resets that processor, and finally moves the task into the ready queue. */
void os_task_finished(int pid){
    Task *t = os_find_task(pid);
    if(!t || t->state != TASK_READY) return;

    int freed_proc = t->proc_id;
    ready_remove(pid);
    t->state = TASK_DONE;
    t->proc_id = -1;
    proc_to_pid[freed_proc] = -1;
    printf("OS: pid %d finished, processor %d freed\n", pid, freed_proc);
    
    if(os_waiting_count() > 0){
        int next = waiting_peek_front();
        Task *nt = os_find_task(next);
        int needed = count_pages_needed(nt->program_byte, nt->data_byte);
        int avail = free_page_count();
        if(needed >= 0 && needed > avail){
            printf("OS: not enough free memory to promote pid %d yet (needs %d pages, %d free) - processor %d stays idle\n",
                next, needed, avail, freed_proc);     
        }
        else{
            int needed = count_pages_needed(nt->program_byte, nt->data_byte);
            int avail = free_page_count();
            if(needed == -1){
                printf("OS: pid %d failed to load (bad program/data file) - task rejected\n", next);
                waiting_pop_front();
                nt->state = TASK_DONE;
                nt->proc_id = -1;
            } else if(needed > avail){
                printf("OS: not enough free memory to promote pid %d yet (needs %d pages, %d free) - processor %d stays idle\n",
                    next, needed, avail, freed_proc);
            } else {
                waiting_pop_front();
                int result = initialize(freed_proc, nt->program_byte, nt->data_byte);
                if(result == 1){
                    nt->proc_id = freed_proc;
                    nt->state = TASK_READY;
                    proc_to_pid[freed_proc] = next;
                    reset(freed_proc);
                    ready_push(next);
                    printf("OS: promoted waiting pid %d onto freed processor %d (ready)\n", next, freed_proc);
                } else {
                    printf("OS: pid %d failed to load unexpectedly - task rejected\n", next);
                    nt->state = TASK_DONE;
                    nt->proc_id = -1;
                }
            }
        }
    }
}

/* Convenience for driving the scheduler loop as total tasks aren't done yet. */
int os_active_task_count(void){
    return ready_count + waiting_count;
}

/* SCHEDULER:
   Gives every currently-ready task one round-robin turn of 'quantum' instructions each. 
   Any task that halts during its turn is finalized (its data memory flushed to its databyte file) 
   and handed to os_task_finished() function, which frees its processor and promotes a 
   waiting task onto it if one exists. Newly-promoted tasks are not run again until the 
   next tick, same as a real round-robin scheduler. */
void os_scheduler_tick(int quantum){
    int count = ready_count;
    int snapshot[MAX_TASKS];
    for(int i = 0; i < count; i++){
        snapshot[i] = ready_queue[i];
    }

    for(int i = 0; i < count; i++){
        int pid = snapshot[i];
        Task *t = os_find_task(pid);
        if(!t || t->state != TASK_READY) continue;

        int proc_id = t->proc_id;
        process_instructions(proc_id, quantum);

        if(end_of_simulation[proc_id]){
            finalize(proc_id, t->data_byte);
            printf("Scheduler: pid %d halted, wrote results to %s\n", pid, t->data_byte);
            os_task_finished(pid);
            if(shell_active){
                printf("$ ");
                fflush(stdout);
            }
        }
    }
}

/* Shell: non-blocking stdin reader.
   A typed line "<name>.txt" is compiled into "<name>.byte", and is
   expected to be paired with a pre-existing data file "<name>_data.byte". */
#ifndef _WIN32
static int shell_stdin_nonblocking = 0;

static void shell_ensure_nonblocking_stdin(void){
    if(shell_stdin_nonblocking) return;
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    shell_stdin_nonblocking = 1;
}
#endif

/* Turns "prog1.txt" into program_byte="prog1.byte" and data_byte="prog1_data.byte" */
static void shell_derive_program_byte(const char *txt_name, char *program_byte){
    char base[FILENAME_LEN];
    strncpy(base, txt_name, FILENAME_LEN - 1);
    base[FILENAME_LEN - 1] = '\0';

    size_t len = strlen(base);
    if(len > 4 && strcmp(base + len - 4, ".txt") == 0){
        base[len - 4] = '\0';
    }

    snprintf(program_byte, FILENAME_LEN, "%s.byte", base);
}

static void shell_process_line(const char *line){
    if(strcmp(line, "exit") == 0){
        shell_active = 0;
        printf("Shell: exit received - no longer accepting new input; remaining tasks will run to completion.\n");
        return;
    }
    if(line[0] == '\0') return; // blank line, nothing to do

    char source_txt[FILENAME_LEN], data_byte[FILENAME_LEN];
    int n = sscanf(line, "%127s %127s", source_txt, data_byte);
    if(n != 2){
        printf("Shell: expected '<program>.txt <data>.byte', got: \"%s\"\n", line);
        return;
    }

    char program_byte[FILENAME_LEN];
    shell_derive_program_byte(source_txt, program_byte);
 
    compile(source_txt, program_byte);

    int pid = loader(program_byte, data_byte);
    if(pid == -1){
        printf("Shell: could not submit %s (task table full)\n", line);
    } else {
        printf("Shell: submitted %s -> pid %d\n", line, pid);
    }
}

int shell_is_active(void){
    return shell_active;
}

/* SHELL:
   Non-blocking read of whatever bytes are currently sitting in stdin's
   buffer, one call at a time - never blocks waiting for more input.
   Characters accumulate in an internal buffer across calls; when a
   newline arrives, the completed line is parsed:
    - "exit" -> stops accepting further input (shell_is_active() becomes 0); 
                tasks already submitted keep running.
    - "<name>.txt" -> compiled into "<name>.byte", paired with a
                    data file the caller must have already placed
                    at "<name>_data.byte", and handed to loader().
   Also treats EOF the same as 'exit'. */
void shell(void){
    if(!shell_active) return;

    if(!shell_prompted){
        printf("$ ");
        fflush(stdout);
        shell_prompted = 1;
    }

#ifdef _WIN32
    /* _kbhit()/_getch() poll the console's keyboard buffer without
       blocking - no fcntl equivalent needed on Windows. */
    while(_kbhit()){
        int c = _getch();
        // next line
        if(c == '\r' || c == '\n'){
            putchar('\n');
            shell_buf[shell_buf_len] = '\0';
            shell_process_line(shell_buf);
            shell_buf_len = 0;
            if(shell_active){
                printf("$ ");
                fflush(stdout);
            }
        }
        // backspace/delete
        else if(c == '\b' || c == 127){
            if(shell_buf_len > 0){
                shell_buf_len--;
                printf("\b \b"); // erase the character visually
            }
        } else if(shell_buf_len < (int)sizeof(shell_buf) - 1){
            shell_buf[shell_buf_len++] = (char)c;
            putchar(c); // _getch() doesn't echo, so echo it ourselves
        }
    }
#else
    shell_ensure_nonblocking_stdin();

    char c;
    ssize_t n;
    while((n = read(STDIN_FILENO, &c, 1)) > 0){
        if(c == '\n' || c == '\r'){
            shell_buf[shell_buf_len] = '\0';
            shell_process_line(shell_buf);
            shell_buf_len = 0;
            if(shell_active){
                printf("$ ");
                fflush(stdout);
            }
        } else if(shell_buf_len < (int)sizeof(shell_buf) - 1){
            shell_buf[shell_buf_len++] = c;
        }
    }

    if(n == 0){
        /* stdin closed (piped input ran out) - treat like 'exit' */
        shell_active = 0;
        printf("Shell: stdin closed - no longer accepting new input.\n");
    }
    /* n < 0 with errno EAGAIN/EWOULDBLOCK just means "nothing available
       right now" - normal for a non-blocking poll. */
#endif
}

/* One full OS cycle, give every ready task a round-robin turn, 
  then let the shell check for new input. main.c calls this in a loop. */
void scheduler(void){
    os_scheduler_tick(SCHED_QUANTUM); // quantum time is given for all active processes as per round-robin
    shell(); // checks for newly typed program names each cycle
    if(ready_count == 0){
        usleep(10000); // waiting on the shell for new work, or fully idl - avoid busy-spinning on the stdin poll
    }
}