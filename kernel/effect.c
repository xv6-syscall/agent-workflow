#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#include "file.h"
#include "stat.h"
#include "fcntl.h"

extern struct superblock sb;

static struct sleeplock key_locks[16];

/*
static void
key_lock_init(void)
{
  if (!locks_inited) {
    for (int i = 0; i < 16; i++) {
      initsleeplock(&key_locks[i], "key_lock");
    }
    locks_inited = 1
}
*/
void
effectinit(void)
{
  for (int i = 0; i < 16; i++) {
    initsleeplock(&key_locks[i], "key_lock");
  }
}

void
key_lock(uint64 key)
{
  acquiresleep(&key_locks[key % 16]);
}

void
key_unlock(uint64 key)
{
  releasesleep(&key_locks[key % 16]);
}

// Computes a 32-bit checksum of the payload buffer
uint32
hash_payload(char *buf, int n)
{
  uint32 hash = 5381;
  for (int i = 0; i < n; i++) {
    hash = ((hash << 5) + hash) + (uchar)buf[i];
  }
  return hash;
}

int
effect_lookup(uint64 key, uint32 hash, int *res)
{
  struct buf *b = bread(ROOTDEV, sb.effectstart);
  struct effect_entry *table = (struct effect_entry *)b->data;
  uint start_slot = key % EFFECT_TABLE_SIZE;
  int has_space = 0;

  for (int i = 0; i < EFFECT_TABLE_SIZE; i++) {
    uint slot = (start_slot + i) % EFFECT_TABLE_SIZE;
    if (table[slot].key == key && table[slot].result != 0) {
      if (table[slot].pad != hash) {
        brelse(b);
        return -2; // Key found BUT payload checksum mismatched!
      }
      *res = table[slot].result;
      brelse(b);
      return 0; // Key found & verified
    }
    if (table[slot].key == 0) {
      has_space = 1;
      break;
    }
  }
  brelse(b);
  return has_space ? -1 : -3; // Key not found
}

void
effect_insert(uint64 key, uint32 hash, int res)
{
  struct buf *b = bread(ROOTDEV, sb.effectstart);
  struct effect_entry *table = (struct effect_entry *)b->data;
  uint start_slot = key % EFFECT_TABLE_SIZE;

  for (int i = 0; i < EFFECT_TABLE_SIZE; i++) {
    uint slot = (start_slot + i) % EFFECT_TABLE_SIZE;
    if (table[slot].key == 0 || table[slot].key == key) {
      table[slot].key = key;
      table[slot].result = res;
      table[slot].pad = hash; // Store checksum in 4-byte pad field
      log_write(b);           // Stage table update into transaction
      brelse(b);
      return;
    }
  }
  brelse(b);
  panic("effect_insert: table full");
}

int
do_append(char *path, uint64 ubuf, int n)
{
  struct inode *ip;
  if ((ip = namei(path)) == 0) {
    ip = create(path, T_FILE, 0, 0);
    if (ip == 0)
      return -1;
  } else {
    ilock(ip);
  }

  int off = ip->size;
  int written = writei(ip, 1, ubuf, off, n);
  iupdate(ip);
  iunlockput(ip);
  return written;
}

uint64
sys_effect(void)
{
  uint64 key, ubuf;
  int op, n, res;
  char path[MAXPATH];
  char kbuf[512];

  argaddr(0, &key);
  argint(1, &op);
  argstr(2, path, MAXPATH);
  argaddr(3, &ubuf);
  argint(4, &n);

  if (n < 0 || n > sizeof(kbuf) || key == 0)
    return -1;

  uint32 hash = 0;
  if (n > 0 && n <= sizeof(kbuf)) {
    if (copyin(myproc()->pagetable, myproc()->sz, kbuf, ubuf, n) == 0) {
      hash = hash_payload(kbuf, n);
    }
  }

  key_lock(key);
  begin_op(); // Single atomic transaction for lookup, write, and table insertion

  int status = effect_lookup(key, hash, &res);
  if (status == 0) {
    end_op();
    key_unlock(key);
    return res; // Return cached result without duplicating file write
  } else if (status == -2) {
    end_op();
    key_unlock(key);
    return -2; // Abort on mismatched payload key reuse
  } else if (status == -3) {
    end_op();
    key_unlock(key);
    return -1;
  }
  res = do_append(path, ubuf, n);
  if (res > 0) {
    effect_insert(key, hash, res);
  }

  end_op();
  key_unlock(key);
  return res;
}
