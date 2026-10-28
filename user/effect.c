#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc < 5) {
    printf("Usage: effect key op path text\n");
    exit(1);
  }
  uint64 key = atoi(argv[1]);
  int op = atoi(argv[2]);
  char *path = argv[3];
  char *buf = argv[4];

  int res = effect(key, op, path, buf, strlen(buf));
  printf("Effect result: %d\n", res);
  exit(0);
}
