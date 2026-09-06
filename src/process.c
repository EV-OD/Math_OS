#include <process.h>
#include <gdt.h>
#include <idt.h>
#include <pic.h>
#include <io.h>
#include <log.h>
#include <string.h>
#include <stdio.h>
#include <gpu.h>

extern void irq0_entry(void);

typedef struct {
    uint32_t esp;
    uint32_t kstack_top;
    int pid;
    proc_state_t state;
    char name[PROC_NAME_LEN];
    int user;
    uint32_t seq;
} pcb_t;

static pcb_t procs[PROC_MAX];
static uint8_t kstacks[PROC_MAX][PROC_STACK_SIZE] __attribute__((aligned(16)));
static uint8_t ustacks[PROC_MAX][PROC_STACK_SIZE] __attribute__((aligned(16)));

static volatile uint32_t ticks = 0;
static volatile int current = 0;
static volatile int sched_locked = 0;
static volatile int sched_on = 0;

static uint8_t fpu_areas[PROC_MAX][512] __attribute__((aligned(16)));
static uint8_t fpu_template[512] __attribute__((aligned(16)));
uint32_t fpu_cur = 0;
uint32_t fpu_next = 0;

void fpu_init(void) {
    __asm__ volatile(
        "mov %%cr4, %%eax\n\t"
        "or $0x200, %%eax\n\t"
        "mov %%eax, %%cr4\n\t"
        "mov %%cr0, %%eax\n\t"
        "and $0xfffffff3, %%eax\n\t"
        "mov %%eax, %%cr0\n\t"
        "finit\n\t"
        ::: "eax", "memory");
    __asm__ volatile("fxsave %0" : "=m"(fpu_template));
}
static uint32_t create_seq = 0;

static void cli(void) { __asm__ volatile("cli"); }
static void sti(void) { __asm__ volatile("sti"); }

void sched_lock(void) {
    __asm__ volatile("" ::: "memory");
    sched_locked++;
    __asm__ volatile("" ::: "memory");
}

void sched_unlock(void) {
    __asm__ volatile("" ::: "memory");
    if (sched_locked > 0) sched_locked--;
    __asm__ volatile("" ::: "memory");
}

uint32_t timer_now(void) {
    return ticks;
}

static void forge_stack(int idx, void (*entry)(void), int user) {
    uint32_t *sp = (uint32_t *)(kstacks[idx] + PROC_STACK_SIZE);
    if (user) {
        uint32_t utop = (uint32_t)(ustacks[idx] + PROC_STACK_SIZE);
        *--sp = USER_DATA_SELECTOR;
        *--sp = utop;
    }
    *--sp = (uint32_t)0x202;
    *--sp = user ? USER_CODE_SELECTOR : KERNEL_CODE_SELECTOR;
    *--sp = (uint32_t)entry;
    *--sp = 0;
    *--sp = 32;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    procs[idx].esp = (uint32_t)sp;
    procs[idx].kstack_top = (uint32_t)(kstacks[idx] + PROC_STACK_SIZE);
}

void process_init(void) {
    memset(procs, 0, sizeof(procs));
    fpu_init();
    for (int i = 0; i < PROC_MAX; i++) {
        procs[i].state = PROC_FREE;
        memcpy(fpu_areas[i], fpu_template, 512);
    }
    procs[0].pid = 0;
    procs[0].state = PROC_RUNNING;
    procs[0].esp = 0;
    procs[0].user = 0;
    strncpy(procs[0].name, "shell", PROC_NAME_LEN - 1);
    current = 0;
    ticks = 0;
    sched_locked = 0;
    sched_on = 0;
    fpu_cur = (uint32_t)fpu_areas[0];
    fpu_next = (uint32_t)fpu_areas[0];
}

int process_create(void (*entry)(void), const char *name, int user) {
    if (!entry || !name) return -1;
    cli();
    int idx = -1;
    for (int i = 1; i < PROC_MAX; i++) {
        if (procs[i].state == PROC_FREE) { idx = i; break; }
    }
    if (idx < 0) { sti(); return -1; }
    forge_stack(idx, entry, user);
    memcpy(fpu_areas[idx], fpu_template, 512);
    procs[idx].pid = idx;
    procs[idx].state = PROC_READY;
    procs[idx].user = user ? 1 : 0;
    procs[idx].seq = ++create_seq;
    strncpy(procs[idx].name, name, PROC_NAME_LEN - 1);
    procs[idx].name[PROC_NAME_LEN - 1] = 0;
    sti();
    log_info("proc: created pid=%d name=%s %s", idx, procs[idx].name, user ? "ring3" : "ring0");
    return idx;
}

int proc_self(void) {
    return current;
}

int proc_kill(int pid) {
    if (pid <= 0 || pid >= PROC_MAX) return -1;
    cli();
    if (procs[pid].state == PROC_FREE) { sti(); return -1; }
    procs[pid].state = PROC_FREE;
    sti();
    gpu_release_if_owner(pid);
    log_info("proc: killed pid=%d", pid);
    return 0;
}

int proc_kill_latest(void) {
    cli();
    int latest = -1;
    uint32_t best = 0;
    for (int i = 1; i < PROC_MAX; i++) {
        if (procs[i].state != PROC_FREE && procs[i].seq > best) {
            best = procs[i].seq;
            latest = i;
        }
    }
    if (latest < 0) { sti(); return -1; }
    procs[latest].state = PROC_FREE;
    sti();
    gpu_release_if_owner(latest);
    log_info("proc: killed pid=%d (ctrl+c)", latest);
    return latest;
}

void proc_exit(void) {
    cli();
    procs[current].state = PROC_FREE;
    log_info("proc: exit pid=%d", current);
    sti();
    gpu_release_if_owner(current);
    for (;;) __asm__ volatile("pause");
}

void proc_sleep(uint32_t n) {
    uint32_t end = ticks + n;
    while (ticks < end) __asm__ volatile("pause");
}

int proc_count(void) {
    int n = 0;
    for (int i = 0; i < PROC_MAX; i++)
        if (procs[i].state != PROC_FREE) n++;
    return n;
}

void ps_list(void) {
    sched_lock();
    printf("pid  mode  state  name\n");
    for (int i = 0; i < PROC_MAX; i++) {
        if (procs[i].state == PROC_FREE) continue;
        const char *st = procs[i].state == PROC_RUNNING ? "run" : "ready";
        if (i == current) st = "run*";
        printf("%d    %s   %s    %s\n", procs[i].pid, procs[i].user ? "u3" : "k0", st, procs[i].name);
    }
    sched_unlock();
}

void timer_init(uint32_t hz) {
    uint32_t div = 1193180 / hz;
    outb(0x43, 0x36);
    outb(0x40, div & 0xFF);
    outb(0x40, (div >> 8) & 0xFF);
    set_isr(32, (uint32_t)irq0_entry);
    IRQ_clear_mask(0);
}

void process_start(void) {
    sched_on = 1;
}

uint32_t timer_pick(uint32_t cur_esp) {
    ticks++;
    pic_acknowledge(32);
    if (!sched_on || sched_locked > 0) {
        fpu_cur = (uint32_t)fpu_areas[current];
        fpu_next = fpu_cur;
        return cur_esp;
    }
    if (procs[current].state != PROC_FREE && procs[current].esp == 0)
        procs[current].esp = cur_esp;
    else if (procs[current].state != PROC_FREE)
        procs[current].esp = cur_esp;
    if (procs[current].state == PROC_RUNNING)
        procs[current].state = PROC_READY;
    int next = -1;
    for (int i = 1; i <= PROC_MAX; i++) {
        int j = (current + i) % PROC_MAX;
        if (procs[j].state == PROC_READY || procs[j].state == PROC_RUNNING) { next = j; break; }
    }
    if (next < 0) {
        fpu_cur = (uint32_t)fpu_areas[current];
        fpu_next = fpu_cur;
        return cur_esp;
    }
    int prev = current;
    current = next;
    procs[current].state = PROC_RUNNING;
    if (procs[current].kstack_top)
        set_kernel_stack(procs[current].kstack_top);
    fpu_cur = (uint32_t)fpu_areas[prev];
    fpu_next = (uint32_t)fpu_areas[current];
    return procs[current].esp;
}
