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
static int locks_inited = 0;

static void
key_lock_init(void)
{
  if (!locks_inited) {
    for (int i = 0; i < 16; i++) {
      initsleeplock(&key_locks[i], "key_lock");
    }
    locks_inited = 1;
  }
}

void
key_lock(uint64 key)
{
  key_lock_init();
  acquiresleep(&key_locks[key % 16]);
}

void
key_unlock(uint64 key)
{
  releasesleep(&key_locks[key % 16]);
}

int
effect_lookup(uint64 key, int *res)
{
  struct buf *b = bread(ROOTDEV, sb.effectstart);
  struct effect_entry *table = (struct effect_entry *)b->data;
  uint start_slot = key % EFFECT_TABLE_SIZE;

  for (int i = 0; i < EFFECT_TABLE_SIZE; i++) {
    uint slot = (start_slot + i) % EFFECT_TABLE_SIZE;
    if (table[slot].key == key && table[slot].result != 0) {
      *res = table[slot].result;
      brelse(b);
      return 0; // Key found
    }
    if (table[slot].key == 0) {
      break;
    }
  }
  brelse(b);
  return -1; // Key not found
}

void
effect_insert(uint64 key, int res)
{
  struct buf *b = bread(ROOTDEV, sb.effectstart);
  struct effect_entry *table = (struct effect_entry *)b->data;
  uint start_slot = key % EFFECT_TABLE_SIZE;

  for (int i = 0; i < EFFECT_TABLE_SIZE; i++) {
    uint slot = (start_slot + i) % EFFECT_TABLE_SIZE;
    if (table[slot].key == 0 || table[slot].key == key) {
      table[slot].key = key;
      table[slot].result = res;
      log_write(b); // Stage table update into transaction
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

  argaddr(0, &key);
  argint(1, &op);
  argstr(2, path, MAXPATH);
  argaddr(3, &ubuf);
  argint(4, &n);

  key_lock(key);
  begin_op(); // Single atomic transaction for lookup, write, and table insertion

  if (effect_lookup(key, &res) == 0) {
    end_op();
    key_unlock(key);
    return res; // Return cached result without duplicating file write
  }

  res = do_append(path, ubuf, n);
  if (res > 0) {
    effect_insert(key, res);
  }

  end_op();
  key_unlock(key);
  return res;
}
