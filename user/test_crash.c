#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    printf("Usage: test_crash <crash_write_number>\n");
    exit(1);
  }

  int n = atoi(argv[1]);
  printf("Setting crashpoint at write #%d and invoking effect()...\n", n);

  crashat(n);

  uint64 key = 2001;
  char *file = "/crash_test.txt";
  char *data = "CrashData\n";

  int res = effect(key, 0, file, data, strlen(data));
  printf("Effect returned: %d (If seen, no crash occurred)\n", res);

  exit(0);
}
