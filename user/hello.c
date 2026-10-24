#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  if(argc < 5){
    printf("Usage: hello a b c d\n");
    exit(1);
  }
  int sum = hello(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
  printf("Hello result sum: %d\n", sum);
  exit(0);
}
