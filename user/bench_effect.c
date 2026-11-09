#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define ITERATIONS 50

int main(int argc, char *argv[]) {
  printf("--- Running I/O Overhead Benchmark (%d Iterations) ---\n", ITERATIONS);
  unlink("/bench_raw.txt");
  unlink("/bench_effect.txt");

  int fd = open("/bench_raw.txt", O_CREATE | O_WRONLY);
  int start_ticks = uptime();
  for (int i = 0; i < ITERATIONS; i++) {
    write(fd, "BENCHMARK_DATA\n", 15);
  }
  close(fd);
  int raw_ticks = uptime() - start_ticks;

  start_ticks = uptime();
  for (int i = 0; i < ITERATIONS; i++) {
    effect(8000 + i, 0, "/bench_effect.txt", "BENCHMARK_DATA\n", 15);
  }
  int effect_ticks = uptime() - start_ticks;

  start_ticks = uptime();
  for (int i = 0; i < ITERATIONS; i++) {
    effect(8000 + i, 0, "/bench_effect.txt", "BENCHMARK_DATA\n", 15);
  }
  int cache_ticks = uptime() - start_ticks;

  printf("Standard write() ticks  : %d\n", raw_ticks);
  printf("sys_effect() write ticks: %d (WAL metadata cost)\n", effect_ticks);
  printf("sys_effect() cache ticks: %d (Deduplication hit)\n", cache_ticks);
  exit(0);
}
