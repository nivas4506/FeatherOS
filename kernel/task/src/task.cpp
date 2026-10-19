#include "task.h"
#include "../../lib/include/string.h"
#include "../../mm/include/heap.h"
#include "../../arch/x86_64/include/cpu.h"

namespace feather::task {

extern "C" void context_switch(cpu_context* old_ctx, cpu_context* new_ctx);

static task* current_task = nullptr;
static task* ready_head = nullptr;
static task* sleep_head = nullptr;
static uint64_t next_task_id = 1;
static uint64_t total_tasks = 0;
static uint64_t context_switches = 0;
static uint64_t idle_ticks = 0;
static constexpr uint64_t DEFAULT_TIME_SLICE = 10;

static task idle_task;
alignas(16) static uint8_t idle_stack[4096];

static void unlink_ready(task* t) {
    if (!t || !ready_head) return;
    if (t->next == t) {
        ready_head = nullptr;
        t->next = t->prev = nullptr;
        return;
    }
    t->prev->next = t->next;
    t->next->prev = t->prev;
    if (ready_head == t) ready_head = t->next;
    t->next = t->prev = nullptr;
}

static void link_ready(task* t) {
    t->state = task_state::READY;
    if (!ready_head) {
        ready_head = t;
        t->next = t->prev = t;
        return;
    }
    t->next = ready_head;
    t->prev = ready_head->prev;
    ready_head->prev->next = t;
    ready_head->prev = t;
}

static task* pop_ready() {
    if (!ready_head) return nullptr;
    task* t = ready_head;
    unlink_ready(t);
    return t;
}

void scheduler_switch() {
    task* old = current_task;
    task* next = pop_ready();
    if (!next) {
        if (!old || old == &idle_task) return;
        next = &idle_task;
    }

    if (old && old->state == task_state::RUNNING && old != &idle_task) {
        link_ready(old);
    }

    if (old == next) return;

    current_task = next;
    next->state = task_state::RUNNING;
    context_switches++;
    context_switch(&old->context, &next->context);
}

static void task_trampoline() {
    task* t = current_task;
    auto fn = reinterpret_cast<void (*)(void*)>(t->entry_point);
    fn(t->arg);
    task_exit(0);
}

void task_init() {
    lib::memset(&idle_task, 0, sizeof(task));
    idle_task.id = 0;
    idle_task.state = task_state::RUNNING;
    idle_task.priority = task_priority::IDLE;
    idle_task.stack = idle_stack;
    idle_task.stack_size = sizeof(idle_stack);
    idle_task.ticks_remaining = DEFAULT_TIME_SLICE;
    current_task = &idle_task;
    total_tasks = 1;
}

uint64_t task_create(void (*entry)(void*), void* arg, task_priority priority, std::size_t stack_size) {
    if (!entry) return 0;
    stack_size = (stack_size + 15) & ~static_cast<std::size_t>(15);
    if (stack_size < 4096) stack_size = 4096;

    auto* neu = static_cast<task*>(mm::kmalloc(sizeof(task)));
    if (!neu) return 0;
    lib::memset(neu, 0, sizeof(task));

    void* stack = mm::kmalloc(stack_size);
    if (!stack) {
        mm::kfree(neu);
        return 0;
    }

    neu->id = next_task_id++;
    neu->state = task_state::READY;
    neu->priority = priority;
    neu->stack = stack;
    neu->stack_size = stack_size;
    neu->entry_point = reinterpret_cast<void*>(entry);
    neu->arg = arg;
    neu->ticks_remaining = DEFAULT_TIME_SLICE;

    uint8_t* top = static_cast<uint8_t*>(stack) + stack_size;
    top = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(top) & ~static_cast<uintptr_t>(15));

    neu->context.rip = reinterpret_cast<uint64_t>(task_trampoline);
    neu->context.rsp = reinterpret_cast<uint64_t>(top);
    neu->context.rbp = 0;
    neu->context.rflags = 0x202;

    link_ready(neu);
    total_tasks++;
    return neu->id;
}

void task_yield() {
    scheduler_switch();
}

[[noreturn]] void task_exit(int code) {
    (void)code;
    if (current_task && current_task != &idle_task) {
        current_task->state = task_state::TERMINATED;
        // Leave the TCB/stack allocated: we are still running on that stack.
        if (total_tasks) total_tasks--;
    }
    scheduler_switch();
    for (;;) arch::cpu::hlt();
}

task* task_current() {
    return current_task;
}

void task_sleep(uint64_t ticks) {
    if (!current_task || current_task == &idle_task) return;
    current_task->state = task_state::SLEEPING;
    current_task->ticks_remaining = ticks;
    current_task->next = sleep_head;
    current_task->prev = nullptr;
    if (sleep_head) sleep_head->prev = current_task;
    sleep_head = current_task;
    scheduler_switch();
}

void task_wake(uint64_t task_id) {
    task* t = sleep_head;
    while (t) {
        task* next = t->next;
        if (t->id == task_id) {
            if (t->prev) t->prev->next = t->next;
            else sleep_head = t->next;
            if (t->next) t->next->prev = t->prev;
            link_ready(t);
            return;
        }
        t = next;
    }
}

task* task_get(uint64_t task_id) {
    if (idle_task.id == task_id) return &idle_task;
    if (current_task && current_task->id == task_id) return current_task;
    task* t = ready_head;
    if (t) {
        do {
            if (t->id == task_id) return t;
            t = t->next;
        } while (t && t != ready_head);
    }
    t = sleep_head;
    while (t) {
        if (t->id == task_id) return t;
        t = t->next;
    }
    return nullptr;
}

void scheduler_tick() {
    if (!current_task) return;
    if (current_task == &idle_task) idle_ticks++;
    else current_task->total_ticks++;

    task* t = sleep_head;
    while (t) {
        task* next = t->next;
        if (t->ticks_remaining > 0) t->ticks_remaining--;
        if (t->ticks_remaining == 0) {
            if (t->prev) t->prev->next = t->next;
            else sleep_head = t->next;
            if (t->next) t->next->prev = t->prev;
            link_ready(t);
        }
        t = next;
    }
}

scheduler_stats scheduler_get_stats() {
    uint64_t ready = 0;
    task* t = ready_head;
    if (t) {
        do {
            ready++;
            t = t->next;
        } while (t && t != ready_head);
    }
    return {
        .total_tasks = total_tasks,
        .running_tasks = ready,
        .context_switches = context_switches,
        .idle_ticks = idle_ticks
    };
}

} // namespace feather::task
