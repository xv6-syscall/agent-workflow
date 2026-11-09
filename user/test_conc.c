#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define NUM_CHILDREN 4
#define SHARED_KEY   7777

int
main(int argc, char *argv[])
{
  printf("--- Starting Concurrent Race Condition Test (CPUS=3) ---\n");
  unlink("/conc_test.txt");

  for (int i = 0; i < NUM_CHILDREN; i++) {
    int pid = fork();
    if (pid == 0) {
      int res = effect(SHARED_KEY, 0, "/conc_test.txt", "CONC_DATA\n", 10);
      printf("[Child %d] Key %d returned byte count: %d\n", getpid(),
             SHARED_KEY, res);
      exit(0);
    }
  }

  for (int i = 0; i < NUM_CHILDREN; i++) {
    wait(0);
  }

  int fd = open("/conc_test.txt", O_RDONLY);
  if (fd < 0) {
    printf("[FAIL] File /conc_test.txt was not created!\n");
    exit(1);
  }

  char buf[128];
  int n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n < 0)
    n = 0;
  buf[n] = '\0';

  printf("File contents after concurrent writes:\n%s", buf);
  if (n == 10) {
    printf(
      "[PASS] Concurrency test passed: Key 7777 appended data exactly once!\n");
  } else {
    printf("[FAIL] Expected 10 bytes, found %d bytes!\n", n);
  }
  exit(0);
}
