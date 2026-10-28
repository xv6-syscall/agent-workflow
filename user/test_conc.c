#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  printf("Starting concurrent effect test on multiple CPUs...\n");

  int pid1 = fork();
  if (pid1 == 0) {
    // Child 1: Attempts write under key 9999
    int res = effect(9999, 0, "/concurrent.txt", "Child1\n", 7);
    printf("Child 1 effect returned: %d\n", res);
    exit(0);
  }

  int pid2 = fork();
  if (pid2 == 0) {
    // Child 2: Competes for the exact same key simultaneously
    int res = effect(9999, 0, "/concurrent.txt", "Child2\n", 7);
    printf("Child 2 effect returned: %d\n", res);
    exit(0);
  }

  // Parent waits for both process branches to finish
  wait(0);
  wait(0);

  printf("Concurrent test complete.\n");
  exit(0);
}
