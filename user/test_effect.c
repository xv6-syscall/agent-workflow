#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

void
test_payload_mismatch(void)
{
  printf("--- Testing Key Reuse with Mismatched Payload ---\n");
  unlink("/mismatch.txt");

  // First call with Key 5555 and payload "DATA_A"
  int res1 = effect(5555, 0, "/mismatch.txt", "DATA_A\n", 7);
  printf("First call (Key 5555, payload 'DATA_A'): %d\n", res1);

  // Second call with same Key 5555 but DIFFERENT payload "DATA_B_DIFFERENT"
  int res2 = effect(5555, 0, "/mismatch.txt", "DATA_B_DIFFERENT\n", 17);
  printf("Second call (Key 5555, payload 'DATA_B'): %d\n", res2);

  if (res2 == -2) {
    printf(
      "[PASS] Payload Checksum Guardrail correctly rejected mismatched key payload!\n");
  } else {
    printf("[FAIL] System did not reject payload mismatch (returned %d)\n",
           res2);
  }
}

void
check_file(char *path, char *expected)
{
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    printf("FAIL: Cannot open %s\n", path);
    exit(1);
  }
  char buf[128];
  memset(buf, 0, sizeof(buf));
  int n = read(fd, buf, sizeof(buf) - 1);
  close(fd);

  if (n < 0) {
    printf("FAIL: Cannot read %s\n", path);
    exit(1);
  }

  if (strcmp(buf, expected) == 0) {
    printf("  [VERIFIED] File content matches: \"%s\"\n", expected);
  } else {
    printf("  [FAIL] Expected \"%s\", got \"%s\"\n", expected, buf);
    exit(1);
  }
}

int
main(void)
{

  uint64 key1 = 1001;
  uint64 key2 = 1002;
  char *file = "/orders.txt";
  char *data1 = "Hello\n";
  char *data2 = "World\n";

  printf("Test 1: First APPEND with key %d...\n", (int)key1);
  int r1 = effect(key1, 0, file, data1, strlen(data1));
  printf("  Returned bytes: %d\n", r1);
  check_file(file, "Hello\n");

  printf("\nTest 2: Replaying duplicate request with key %d...\n", (int)key1);
  int r2 = effect(key1, 0, file, data1, strlen(data1));
  printf("  Returned bytes: %d\n", r2);
  if (r1 == r2) {
    printf("  [VERIFIED] Cached result returned correctly (%d == %d)\n", r1,
           r2);
  } else {
    printf("  [FAIL] Result mismatch: %d vs %d\n", r1, r2);
  }
  check_file(file, "Hello\n");

  printf("\nTest 3: APPEND with new key %d...\n", (int)key2);
  int r3 = effect(key2, 0, file, data2, strlen(data2));
  printf("  Returned bytes: %d\n", r3);
  check_file(file, "Hello\nWorld\n");
  test_payload_mismatch();

  exit(0);
}
