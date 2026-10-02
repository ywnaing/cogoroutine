#include "coro.h"
#include <stdio.h>
#include <unistd.h>

void worker(void *arg) {
  char *name = (char *)arg;
  for (int i = 1; i <= 3; i++) {
    printf("[%s] Step %d\n", name, i);
    co_yield();
  }
}

int main(void) {
  printf("=== Testing Custom Coroutine Library ===\n\n");

  co_init();

  // TODO: Spawn coroutines with co_go()
  co_go(worker, "ALICE");
  co_go(worker, "BOB");
  // TODO: Start scheduler with co_run()
  co_run();
  return 0;
}
