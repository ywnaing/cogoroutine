#include "coro.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Assembly context switch declared in switch_arm64.s
// x0 = void **old_sp, x1 = void *new_sp
extern void co_switch(void **old_sp, void *new_sp);

// 1. Data Structures:
// - coro_t: Represents one coroutine (id, status, sp, stack, func, arg)
// - scheduler_t: Manages the array of coroutines, scheduler_sp, and
// active_count

typedef struct {
  int id;
  coro_status_t status;
  void *sp;         // Saved stack pointer
  char *stack;      // Pointer to 16KB stack in RAM
  coro_func_t func; // Function to run
  void *arg;        // Argument passed to func
} coro_t;

typedef struct {
  coro_t coroutines[CORO_MAX];
  int current_id;     // Currently running coroutine ID (-1 if scheduler)
  void *scheduler_sp; // Saved SP of the scheduler
  int active_count;   // Number of alive coroutines
  bool initialized;
} scheduler_t;

static scheduler_t sched;

// TODO: Implement the Trampoline: coro_entry()
// When a new coroutine is spawned, it starts here!
// 1. Run the user's function: co->func(co->arg)
// 2. Mark coroutine as CORO_DEAD and decrement active_count
// 3. Switch permanently back to the scheduler!
static void coro_entry(void) {
  coro_t *co = &sched.coroutines[sched.current_id];

  co->func(co->arg);
  co->status = CORO_DEAD;
  sched.active_count--;

  co_switch(&co->sp, sched.scheduler_sp);
}

// TODO: Implement co_init()
// Initialize the scheduler structure
void co_init(void) {
  // Write your code here
  if (sched.initialized) {
    return;
  }

  sched.current_id = -1;
  sched.scheduler_sp = NULL;
  sched.active_count = 0;
  sched.initialized = true;

  for (size_t i = 0; i < CORO_MAX; i++) {
    sched.coroutines[i].status = CORO_DEAD;
  }
}

// TODO: Implement co_go(func, arg)
// 1. Find an empty slot in sched.coroutines (where status == CORO_DEAD)
// 2. Allocate 16KB stack with malloc()
// 3. Prepare the stack:
//    - Find the top of the stack (stack + CORO_STACK_SIZE) aligned to 16 bytes
//    - Subtract 12 register slots (96 bytes)
//    - Set sp[1] (x30 / LR) to coro_entry!
// 4. Set status to CORO_READY and increment active_count
int co_go(coro_func_t func, void *arg) {

  int slot = -1;
  for (size_t i = 0; i < CORO_MAX; i++) {
    if (sched.coroutines[i].status == CORO_DEAD) {
      slot = i;
      break;
    }
  }

  if (slot == -1) {
    return -1;
  }

  coro_t *co = &sched.coroutines[slot];

  if (co->stack == NULL) {
    co->stack = malloc(CORO_STACK_SIZE);
    if (co->stack == NULL)
      return -1;
  }

  co->func = func;
  co->arg = arg;
  co->status = CORO_READY;

  uintptr_t stack_top = (uintptr_t)(co->stack + CORO_STACK_SIZE);
  stack_top &= -16LL;

  uint64_t *sp = (uint64_t *)stack_top;
  sp -= 12;

  sp[0] = 0;
  sp[1] = (uint64_t)coro_entry;

  for (int i = 2; i < 12; i++) {
    sp[i] = 0;
  }

  co->sp = (void *)sp;

  co->id = slot;

  sched.active_count++;

  return slot;
}

// TODO: Implement co_yield()
// Pause current coroutine and switch back to the scheduler
void co_yield(void) {
  if (sched.current_id == -1) {
    return;
  }

  coro_t *co = &sched.coroutines[sched.current_id];
  if (co->status == CORO_RUNNING) {
    co->status = CORO_READY;
  }
  co_switch(&co->sp, sched.scheduler_sp);
}

// TODO: Implement co_run()
// The Round-Robin Scheduler loop:
// While active_count > 0:
//   Loop through all coroutines:
//     If status is CORO_READY:
//       Set current_id, set status to CORO_RUNNING
//       Switch CPU from scheduler_sp to coroutine sp!
//       If coroutine yielded, set status back to CORO_READY
void co_run(void) {
  // Write your code here
  while (co_active_count() > 0) {
    for (size_t i = 0; i < CORO_MAX; i++) {
      coro_t *co = &sched.coroutines[i];

      if (co->status == CORO_READY) {
        sched.current_id = co->id;
        co->status = CORO_RUNNING;
        co_switch(&sched.scheduler_sp, co->sp);
      }
    }
  }
}

int co_id(void) { return sched.current_id; }

int co_active_count(void) { return sched.active_count; }
