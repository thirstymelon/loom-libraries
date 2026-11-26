#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* RP2040 Memory-Mapped Hardware Constants */
#define SIO_BASE               0xD0000000
#define SIO_CPUID_OFFSET       0x000
#define SIO_FIFO_ST_OFFSET     0x050
#define SIO_FIFO_WR_OFFSET     0x054
#define SIO_FIFO_RD_OFFSET     0x058
#define SIO_SPINLOCK31_OFFSET  0x17C

#define PPB_BASE               0xE0000000
#define SYST_CSR               (*(volatile uint32_t*)(PPB_BASE + 0xE010))
#define SYST_RVR               (*(volatile uint32_t*)(PPB_BASE + 0xE014))
#define SYST_CVR               (*(volatile uint32_t*)(PPB_BASE + 0xE018))
#define ICSR                   (*(volatile uint32_t*)(PPB_BASE + 0xED04))
#define SHPR3                  (*(volatile uint32_t*)(PPB_BASE + 0xED20))

#define SIO_FIFO_ST_VLD        (1u << 0)
#define SIO_FIFO_ST_RDY        (1u << 1)

#define LOOM_SCHEDULER_MAX_TASKS    16
#define LOOM_SCHEDULER_STACK_WORDS  256   /* 1024 bytes per task stack */
#define LOOM_SCHEDULER_PRIORITIES   8
#define LOOM_SCHEDULER_QUANTUM_MS   5

typedef enum {
    TASK_STATE_UNUSED = 0,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_SLEEPING,
    TASK_STATE_BLOCKED,
    TASK_STATE_TERMINATED
} TaskState;

typedef enum {
    AFFINITY_CORE0 = 0,
    AFFINITY_CORE1 = 1,
    AFFINITY_BOTH  = 2
} CoreAffinity;

typedef void (*LoomTaskEntry)(void* arg);

typedef struct LoomTCB {
    uint32_t* sp;                      /* Stack pointer: MUST be first field at offset 0 */
    uint32_t id;
    uint32_t priority;                /* Current priority (0..7) */
    uint32_t base_priority;           /* Original base priority */
    TaskState state;
    CoreAffinity affinity;
    bool is_privileged;               /* Privileged (MSP/privileged PSP) or Unprivileged PSP */
    uint32_t running_core;            /* 0 = Core 0, 1 = Core 1, 0xFF = Not running */
    uint32_t sleep_ticks_remaining;
    uint32_t slice_remaining_ms;
    struct LoomTCB* next;             /* Linked list pointer for ready/sleep/wait queues */
    uint32_t stack[LOOM_SCHEDULER_STACK_WORDS] __attribute__((aligned(8)));
} LoomTCB;

typedef struct LoomMutex {
    volatile uint32_t locked;
    LoomTCB* owner;
    uint32_t original_priority;
    LoomTCB* wait_list;
} LoomMutex;

/* Static Memory Pool in .bss (Zero dynamic heap allocations) */
static LoomTCB g_tasks[LOOM_SCHEDULER_MAX_TASKS];
static LoomTCB* g_current[2] = { NULL, NULL };
static LoomTCB* g_ready_heads[LOOM_SCHEDULER_PRIORITIES] = { NULL };
static LoomTCB* g_ready_tails[LOOM_SCHEDULER_PRIORITIES] = { NULL };
static LoomTCB* g_sleep_list = NULL;
static volatile bool g_scheduler_running = false;
static volatile uint32_t g_tick_count = 0;

extern void loom_scheduler_trigger_pendsv(void);
extern void loom_scheduler_start(void);

/* Multi-core SIO Spinlock 31 Synchronization */
static inline void scheduler_lock(void) {
    volatile uint32_t* lock_reg = (volatile uint32_t*)(SIO_BASE + SIO_SPINLOCK31_OFFSET);
    while (*lock_reg == 0) {
        __asm volatile("nop");
    }
    __asm volatile("dmb" ::: "memory");
}

static inline void scheduler_unlock(void) {
    __asm volatile("dmb" ::: "memory");
    *(volatile uint32_t*)(SIO_BASE + SIO_SPINLOCK31_OFFSET) = 1;
}

static inline uint32_t get_core_id(void) {
    return *(volatile uint32_t*)(SIO_BASE + SIO_CPUID_OFFSET);
}

uint32_t loom_scheduler_core_id(void) {
    return get_core_id();
}

uint32_t loom_scheduler_task_id(void) {
    uint32_t core = get_core_id();
    return g_current[core] ? g_current[core]->id : 0;
}

/* Ready Queue Operations (Protected by scheduler_lock) */
static void enqueue_ready(LoomTCB* tcb) {
    uint32_t p = tcb->priority;
    if (p >= LOOM_SCHEDULER_PRIORITIES) p = LOOM_SCHEDULER_PRIORITIES - 1;
    tcb->state = TASK_STATE_READY;
    tcb->running_core = 0xFF;
    tcb->next = NULL;
    if (g_ready_tails[p]) {
        g_ready_tails[p]->next = tcb;
    } else {
        g_ready_heads[p] = tcb;
    }
    g_ready_tails[p] = tcb;
}

static LoomTCB* dequeue_ready_for_core(uint32_t core_id) {
    for (int p = LOOM_SCHEDULER_PRIORITIES - 1; p >= 0; --p) {
        LoomTCB* prev = NULL;
        LoomTCB* curr = g_ready_heads[p];
        while (curr) {
            if (curr->affinity == AFFINITY_BOTH || (curr->affinity == (CoreAffinity)core_id)) {
                /* Remove curr from queue */
                if (prev) {
                    prev->next = curr->next;
                } else {
                    g_ready_heads[p] = curr->next;
                }
                if (g_ready_tails[p] == curr) {
                    g_ready_tails[p] = prev;
                }
                curr->next = NULL;
                return curr;
            }
            prev = curr;
            curr = curr->next;
        }
    }
    return NULL;
}

static void loom_scheduler_task_exit(void) {
    scheduler_lock();
    uint32_t core = get_core_id();
    if (g_current[core]) {
        g_current[core]->state = TASK_STATE_TERMINATED;
        g_current[core]->running_core = 0xFF;
    }
    scheduler_unlock();
    loom_scheduler_trigger_pendsv();
    while (1) {
        __asm volatile("wfi");
    }
}

/* Idle task per core */
static void idle_task_body(void* arg) {
    (void)arg;
    while (1) {
        __asm volatile("wfi");
    }
}

LoomTCB* loom_scheduler_task_create(LoomTaskEntry fn, void* arg, uint32_t priority, uint32_t stack_bytes, uint32_t affinity, bool is_privileged) {
    (void)stack_bytes;
    scheduler_lock();
    LoomTCB* tcb = NULL;
    for (int i = 0; i < LOOM_SCHEDULER_MAX_TASKS; ++i) {
        if (g_tasks[i].state == TASK_STATE_UNUSED || g_tasks[i].state == TASK_STATE_TERMINATED) {
            tcb = &g_tasks[i];
            tcb->id = i + 1;
            break;
        }
    }
    if (!tcb) {
        scheduler_unlock();
        return NULL;
    }

    if (priority >= LOOM_SCHEDULER_PRIORITIES) priority = LOOM_SCHEDULER_PRIORITIES - 1;
    tcb->priority = priority;
    tcb->base_priority = priority;
    tcb->affinity = (CoreAffinity)affinity;
    tcb->is_privileged = is_privileged;
    tcb->sleep_ticks_remaining = 0;
    tcb->slice_remaining_ms = LOOM_SCHEDULER_QUANTUM_MS;

    /* Initialize stack frame (16 words = 64 bytes) */
    uint32_t* top = &tcb->stack[LOOM_SCHEDULER_STACK_WORDS];
    uint32_t* sp = top - 16;

    /* Exception hardware frame pushed by Cortex-M0+ */
    sp[8]  = (uint32_t)arg;                         /* r0 */
    sp[9]  = 0;                                     /* r1 */
    sp[10] = 0;                                     /* r2 */
    sp[11] = 0;                                     /* r3 */
    sp[12] = 0;                                     /* r12 */
    sp[13] = (uint32_t)loom_scheduler_task_exit;    /* lr */
    sp[14] = ((uint32_t)fn) | 1u;                   /* pc (Thumb bit) */
    sp[15] = 0x01000000u;                           /* xPSR (Thumb state) */

    /* Software context frame pushed by PendSV */
    sp[0] = 0;                                      /* r8 */
    sp[1] = 0;                                      /* r9 */
    sp[2] = 0;                                      /* r10 */
    sp[3] = 0;                                      /* r11 */
    sp[4] = 0;                                      /* r4 */
    sp[5] = 0;                                      /* r5 */
    sp[6] = 0;                                      /* r6 */
    sp[7] = 0;                                      /* r7 */

    tcb->sp = sp;
    enqueue_ready(tcb);
    scheduler_unlock();

    if (g_scheduler_running) {
        loom_scheduler_trigger_pendsv();
    }
    return tcb;
}

uint32_t* loom_scheduler_switch_context(uint32_t* old_sp) {
    scheduler_lock();
    uint32_t core = get_core_id();
    LoomTCB* curr = g_current[core];

    if (curr && curr->state == TASK_STATE_RUNNING) {
        curr->sp = old_sp;
        /* Put back into ready queue */
        enqueue_ready(curr);
    }

    LoomTCB* next = dequeue_ready_for_core(core);
    if (!next) {
        /* No user task ready: pick idle task for this core */
        next = &g_tasks[core == 0 ? 0 : 1];
    }

    next->state = TASK_STATE_RUNNING;
    next->running_core = core;
    next->slice_remaining_ms = LOOM_SCHEDULER_QUANTUM_MS;
    g_current[core] = next;

    uint32_t* new_sp = next->sp;
    uint32_t ctrl = next->is_privileged ? 0x02u : 0x03u;
    __asm volatile("msr control, %0\nisb" :: "r"(ctrl) : "memory");
    scheduler_unlock();
    return new_sp;
}

void loom_scheduler_tick_isr(void) {
    uint32_t core = get_core_id();
    bool need_switch = false;

    scheduler_lock();
    if (core == 0) {
        g_tick_count++;
        /* Update sleep queue */
        LoomTCB* prev = NULL;
        LoomTCB* curr = g_sleep_list;
        while (curr) {
            if (curr->sleep_ticks_remaining > 0) {
                curr->sleep_ticks_remaining--;
            }
            if (curr->sleep_ticks_remaining == 0) {
                /* Wake up */
                LoomTCB* wake = curr;
                if (prev) prev->next = curr->next;
                else g_sleep_list = curr->next;
                curr = curr->next;
                enqueue_ready(wake);
                need_switch = true;
            } else {
                prev = curr;
                curr = curr->next;
            }
        }
    }

    LoomTCB* cur_task = g_current[core];
    if (cur_task && cur_task->slice_remaining_ms > 0) {
        cur_task->slice_remaining_ms--;
        if (cur_task->slice_remaining_ms == 0) {
            need_switch = true;
        }
    }
    scheduler_unlock();

    if (need_switch) {
        loom_scheduler_trigger_pendsv();
    }
}

void loom_scheduler_sleep_ms(uint32_t ms) {
    if (!g_scheduler_running || ms == 0) {
        /* Fall back to busy wait if scheduler not started */
        for (volatile uint32_t i = 0; i < ms * 1000; ++i) {
            __asm volatile("nop");
        }
        return;
    }

    scheduler_lock();
    uint32_t core = get_core_id();
    LoomTCB* curr = g_current[core];
    if (curr) {
        curr->state = TASK_STATE_SLEEPING;
        curr->sleep_ticks_remaining = ms;
        curr->running_core = 0xFF;
        curr->next = g_sleep_list;
        g_sleep_list = curr;
    }
    scheduler_unlock();
    loom_scheduler_trigger_pendsv();
}

void loom_scheduler_yield(void) {
    if (!g_scheduler_running) return;
    scheduler_lock();
    uint32_t core = get_core_id();
    if (g_current[core]) {
        g_current[core]->slice_remaining_ms = 0;
    }
    scheduler_unlock();
    loom_scheduler_trigger_pendsv();
}

/* Mutex with Priority Inheritance */
void loom_scheduler_mutex_init(LoomMutex* m) {
    m->locked = 0;
    m->owner = NULL;
    m->original_priority = 0;
    m->wait_list = NULL;
}

void loom_scheduler_mutex_lock(LoomMutex* m) {
    if (!g_scheduler_running) return;
    while (1) {
        scheduler_lock();
        uint32_t core = get_core_id();
        LoomTCB* curr = g_current[core];
        if (m->locked == 0) {
            m->locked = 1;
            m->owner = curr;
            m->original_priority = curr->priority;
            scheduler_unlock();
            return;
        }

        /* Priority Inheritance: boost owner if current task has higher priority */
        if (m->owner && curr->priority > m->owner->priority) {
            m->owner->priority = curr->priority;
        }

        /* Block current task */
        curr->state = TASK_STATE_BLOCKED;
        curr->running_core = 0xFF;
        curr->next = m->wait_list;
        m->wait_list = curr;
        scheduler_unlock();

        loom_scheduler_trigger_pendsv();
    }
}

void loom_scheduler_mutex_unlock(LoomMutex* m) {
    if (!g_scheduler_running) return;
    scheduler_lock();
    uint32_t core = get_core_id();
    LoomTCB* curr = g_current[core];
    if (m->owner == curr) {
        curr->priority = m->original_priority;
        m->owner = NULL;
        m->locked = 0;

        /* Unblock highest priority waiting task */
        if (m->wait_list) {
            LoomTCB* wake = m->wait_list;
            m->wait_list = wake->next;
            enqueue_ready(wake);
        }
    }
    scheduler_unlock();
    loom_scheduler_trigger_pendsv();
}

/* Compiler aliases for Lock statements and task spawning on baremetal */
void loom_mutex_init(void* m) {
    loom_scheduler_mutex_init((LoomMutex*)m);
}

void loom_mutex_lock(void* m) {
    loom_scheduler_mutex_lock((LoomMutex*)m);
}

void loom_mutex_unlock(void* m) {
    loom_scheduler_mutex_unlock((LoomMutex*)m);
}

/* Static Zero-Heap Sized Channel (Ring Buffer) */
typedef struct {
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t capacity;
    uint32_t lock;
    uint32_t buffer[16];
} LoomChannel16;

void loom_channel_init(void* ch, uint32_t capacity) {
    if (!ch) return;
    LoomChannel16* c = (LoomChannel16*)ch;
    c->head = 0;
    c->tail = 0;
    c->count = 0;
    c->capacity = (capacity > 0 && capacity <= 16) ? capacity : 16;
    c->lock = 0;
}

bool loom_channel_send(void* ch, uint32_t value) {
    if (!ch) return false;
    LoomChannel16* c = (LoomChannel16*)ch;
    while (1) {
        scheduler_lock();
        if (c->count < c->capacity) {
            c->buffer[c->head] = value;
            c->head = (c->head + 1) % c->capacity;
            c->count++;
            scheduler_unlock();
            return true;
        }
        scheduler_unlock();
        loom_scheduler_yield();
    }
}

bool loom_channel_try_send(void* ch, uint32_t value) {
    if (!ch) return false;
    LoomChannel16* c = (LoomChannel16*)ch;
    scheduler_lock();
    if (c->count < c->capacity) {
        c->buffer[c->head] = value;
        c->head = (c->head + 1) % c->capacity;
        c->count++;
        scheduler_unlock();
        return true;
    }
    scheduler_unlock();
    return false;
}

uint32_t loom_channel_receive(void* ch) {
    if (!ch) return 0;
    LoomChannel16* c = (LoomChannel16*)ch;
    while (1) {
        scheduler_lock();
        if (c->count > 0) {
            uint32_t val = c->buffer[c->tail];
            c->tail = (c->tail + 1) % c->capacity;
            c->count--;
            scheduler_unlock();
            return val;
        }
        scheduler_unlock();
        loom_scheduler_yield();
    }
}

bool loom_channel_try_receive(void* ch, uint32_t* out_val) {
    if (!ch || !out_val) return false;
    LoomChannel16* c = (LoomChannel16*)ch;
    scheduler_lock();
    if (c->count > 0) {
        *out_val = c->buffer[c->tail];
        c->tail = (c->tail + 1) % c->capacity;
        c->count--;
        scheduler_unlock();
        return true;
    }
    scheduler_unlock();
    return false;
}

uint32_t loom_channel_count(void* ch) {
    if (!ch) return 0;
    LoomChannel16* c = (LoomChannel16*)ch;
    scheduler_lock();
    uint32_t cnt = c->count;
    scheduler_unlock();
    return cnt;
}

bool loom_channel_is_empty(void* ch) {
    return loom_channel_count(ch) == 0;
}

bool loom_channel_is_full(void* ch) {
    if (!ch) return true;
    LoomChannel16* c = (LoomChannel16*)ch;
    scheduler_lock();
    bool full = (c->count >= c->capacity);
    scheduler_unlock();
    return full;
}

void loom_scheduler_init(void);

void* loom_task_spawn(void (*entry)(void*, void*), void* args, uint64_t args_size, uint64_t res_size) {
    (void)args_size;
    (void)res_size;
    if (!g_scheduler_running) {
        loom_scheduler_init();
    }
    /* Create task with default normal priority (4), 1024 stack, both core affinity, unprivileged */
    return (void*)loom_scheduler_task_create((LoomTaskEntry)entry, args, 4, 1024, AFFINITY_BOTH, false);
}

void* loom_task_spawn_on_core(void (*entry)(void*, void*), void* args, uint64_t args_size, uint64_t res_size, uint32_t affinity, bool is_privileged) {
    (void)args_size;
    (void)res_size;
    if (!g_scheduler_running) {
        loom_scheduler_init();
    }
    CoreAffinity aff = AFFINITY_BOTH;
    if (affinity == 0) aff = AFFINITY_CORE0;
    else if (affinity == 1) aff = AFFINITY_CORE1;
    return (void*)loom_scheduler_task_create((LoomTaskEntry)entry, args, 4, 1024, aff, is_privileged);
}

void* loom_task_spawn_privileged(void (*entry)(void*, void*), void* args, uint64_t args_size, uint64_t res_size) {
    return loom_task_spawn_on_core(entry, args, args_size, res_size, AFFINITY_BOTH, true);
}

/* Core 1 Bootloader via SIO FIFO */
static void core1_entry(void) {
    /* Set PendSV to lowest priority */
    SHPR3 |= (0xC0u << 16);

    /* Start Core 1 SysTick (1 kHz) at 125 MHz */
    SYST_RVR = 125000 - 1;
    SYST_CVR = 0;
    SYST_CSR = 7; /* ENABLE | TICKINT | CLKSOURCE */

    loom_scheduler_start();
}

static void launch_core1(void) {
    volatile uint32_t* fifo_st = (volatile uint32_t*)(SIO_BASE + SIO_FIFO_ST_OFFSET);
    volatile uint32_t* fifo_wr = (volatile uint32_t*)(SIO_BASE + SIO_FIFO_WR_OFFSET);
    volatile uint32_t* fifo_rd = (volatile uint32_t*)(SIO_BASE + SIO_FIFO_RD_OFFSET);

    /* Flush FIFO */
    while (*fifo_st & SIO_FIFO_ST_VLD) {
        (void)*fifo_rd;
    }

    /* Handshake sequence to launch Core 1 */
    const uint32_t cmd_sequence[] = {
        0, 0, 1,
        0x20040000,                                         /* Vector table at SRAM Scratchpad 0 */
        (uint32_t)&g_tasks[1].stack[LOOM_SCHEDULER_STACK_WORDS], /* Stack for Core 1 idle task */
        ((uint32_t)core1_entry) | 1u                        /* Entry function */
    };

    for (size_t i = 0; i < sizeof(cmd_sequence) / sizeof(cmd_sequence[0]); ++i) {
        while (!(*fifo_st & SIO_FIFO_ST_RDY));
        *fifo_wr = cmd_sequence[i];
        while (!(*fifo_st & SIO_FIFO_ST_VLD));
        uint32_t resp = *fifo_rd;
        (void)resp;
    }
}

/* Initialize and start the dual-core scheduler */
void loom_scheduler_init(void) {
    if (g_scheduler_running) return;

    /* Initialize task table */
    for (int i = 0; i < LOOM_SCHEDULER_MAX_TASKS; ++i) {
        g_tasks[i].state = TASK_STATE_UNUSED;
    }

    /* Create idle tasks: Task 0 for Core 0, Task 1 for Core 1 (privileged) */
    loom_scheduler_task_create(idle_task_body, NULL, 0, 1024, AFFINITY_CORE0, true);
    loom_scheduler_task_create(idle_task_body, NULL, 0, 1024, AFFINITY_CORE1, true);

    /* Set PendSV to lowest priority (priority 3 = 0xC0) in SHPR3 */
    SHPR3 |= (0xC0u << 16);

    /* Configure SysTick for 1000 Hz (1 ms tick) assuming 125 MHz sys clock */
    SYST_RVR = 125000 - 1;
    SYST_CVR = 0;
    SYST_CSR = 7; /* ENABLE | TICKINT | CLKSOURCE */

    /* Release Spinlock 31 so it starts unlocked */
    *(volatile uint32_t*)(SIO_BASE + SIO_SPINLOCK31_OFFSET) = 1;

    g_scheduler_running = true;

    /* Launch Core 1 for SMP execution */
    launch_core1();

    /* Trigger PendSV for Core 0 scheduler switch */
    loom_scheduler_trigger_pendsv();
}

#define MPU_TYPE               (*(volatile uint32_t*)(PPB_BASE + 0xED90))
#define MPU_CTRL               (*(volatile uint32_t*)(PPB_BASE + 0xED94))
#define MPU_RNR                (*(volatile uint32_t*)(PPB_BASE + 0xED98))
#define MPU_RBAR               (*(volatile uint32_t*)(PPB_BASE + 0xED9C))
#define MPU_RASR               (*(volatile uint32_t*)(PPB_BASE + 0xEDA0))

void loom_mpu_init(void) {
    if (((MPU_TYPE >> 8) & 0xFF) == 0) return;
    MPU_CTRL = 0;

    /* Region 0: Flash 0x10000000, 2MB, Read-Only, Executable */
    MPU_RNR = 0;
    MPU_RBAR = 0x10000000;
    MPU_RASR = (1 << 0) | (20 << 1) | (2 << 24);

    /* Region 1: SRAM 0x20000000, 256KB, Privileged RW, XN = 1 */
    MPU_RNR = 1;
    MPU_RBAR = 0x20000000;
    MPU_RASR = (1 << 0) | (17 << 1) | (1 << 24) | (1 << 28);

    /* Region 3: Peripherals 0x40000000, 512MB, Full Access Device */
    MPU_RNR = 3;
    MPU_RBAR = 0x40000000;
    MPU_RASR = (1 << 0) | (28 << 1) | (3 << 24) | (1 << 28);

    MPU_CTRL = 1 | (1 << 2); /* ENABLE | PRIVDEFENA */
    __asm volatile("dmb" ::: "memory");
    __asm volatile("isb" ::: "memory");
}

void loom_mpu_configure_task(LoomTCB* tcb) {
    if (!tcb || (((MPU_TYPE >> 8) & 0xFF) == 0)) return;
    MPU_RNR = 2;
    MPU_RBAR = ((uint32_t)tcb->stack) & ~0x3FF;
    MPU_RASR = (1 << 0) | (9 << 1) | (3 << 24) | (1 << 28);
    __asm volatile("dmb" ::: "memory");
}

void loom_task_fault_handler(uint32_t* stack_frame) {
    (void)stack_frame;
    uint32_t core = get_core_id();
    LoomTCB* cur = g_current[core];
    if (cur) {
        cur->state = TASK_STATE_TERMINATED;
        g_current[core] = NULL;
        loom_scheduler_yield();
    }
}
