#ifndef CORO_H
#define CORO_H

#include <stddef.h>
#include <stdbool.h>

// Stack size for each coroutine (16 KB in user RAM, compared to 8 MB for pthread!)
#define CORO_STACK_SIZE 16384
#define CORO_MAX 64

// Function signature for a coroutine: takes a void* argument and returns void
typedef void (*coro_func_t)(void *arg);

// Lifecycle states of a Coroutine
typedef enum {
    CORO_DEAD = 0,   // Finished execution, memory can be recycled
    CORO_READY,      // Ready to be scheduled
    CORO_RUNNING     // Currently actively executing on the CPU
} coro_status_t;

/**
 * co_init - Initializes the coroutine scheduler.
 * Call this once at the start of your program.
 */
void co_init(void);

/**
 * co_go - Spawns a new coroutine (the C equivalent of Go's `go func(arg)`).
 * @param func: The function to run in the coroutine
 * @param arg:  Optional pointer argument to pass to the function
 * @return: The unique ID of the created coroutine (or -1 if limit reached).
 */
int co_go(coro_func_t func, void *arg);

/**
 * co_yield - Voluntarily yields the CPU back to the scheduler.
 * Pauses the current coroutine and lets the next ready coroutine run.
 */
void co_yield(void);

/**
 * co_run - Starts the scheduler event loop.
 * Runs in a round-robin loop until all spawned coroutines have completed.
 */
void co_run(void);

/**
 * co_id - Returns the ID of the currently executing coroutine.
 */
int co_id(void);

/**
 * co_active_count - Returns how many coroutines are currently alive.
 */
int co_active_count(void);

#endif // CORO_H
