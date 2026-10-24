#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"

struct effect_entry {
  uint64 key;
  int res;
  int used;
};

#define MAX_EFFECTS 64
static struct effect_entry table[MAX_EFFECTS];

int effect_lookup(uint64 key, int *res) {
  for(int i = 0; i < MAX_EFFECTS; i++){
    if(table[i].used && table[i].key == key){
      *res = table[i].res;
      return 0;
    }
  }
  return -1;
}

void effect_insert(uint64 key, int res) {
  for(int i = 0; i < MAX_EFFECTS; i++){
    if(!table[i].used){
      table[i].key = key;
      table[i].res = res;
      table[i].used = 1;
      return;
    }
  }
}

uint64
sys_effect(void)
{
  uint64 key, ubuf;
  int op, n, res = 0;
  char path[MAXPATH];

  argaddr(0, &key);
  argint(1, &op);
  argstr(2, path, MAXPATH);
  argaddr(3, &ubuf);
  argint(4, &n);

  begin_op();

  if(effect_lookup(key, &res) == 0) {
    end_op();
    printk("kernel: key %d found in table! Returning cached result: %d\n", (int)key, res);
    return res;
  }

  res = n; 
  effect_insert(key, res);

  end_op();
  printk("kernel: effect executed and key %d saved.\n", (int)key);
  return res;
}
