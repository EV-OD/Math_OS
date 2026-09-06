#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define PROC_MAX 8
#define PROC_STACK_SIZE 4096
#define PROC_NAME_LEN 16

typedef enum {
    PROC_FREE = 0,
    PROC_READY,
    PROC_RUNNING
} proc_state_t;

void process_init(void);
int process_create(void (*entry)(void), const char *name, int user);
int proc_kill(int pid);
int proc_kill_latest(void);
void proc_exit(void);
void proc_sleep(uint32_t ticks);
void ps_list(void);
void process_start(void);
int proc_self(void);
int proc_count(void);

uint32_t timer_pick(uint32_t cur_esp);
uint32_t timer_now(void);
void timer_init(uint32_t hz);

void sched_lock(void);
void sched_unlock(void);

#endif
