// On-disk file system format.
// Both the kernel and user programs use this header file.

#define ROOTINO 1    // root i-number
#define BSIZE   1024 // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;         // Must be FSMAGIC
  uint size;          // Size of file system image (blocks)
  uint nblocks;       // Number of data blocks
  uint ninodes;       // Number of inodes.
  uint nlog;          // Number of log blocks
  uint logstart;      // Block number of first log block
  uint inodestart;    // Block number of first inode block
  uint bmapstart;     // Block number of first free map block
  uint neffectblocks; // Number of effect table blocks
  uint effectstart;   // Block number of first effect table block
};

#define EFFECT_ENTRIES_PER_BLOCK (BSIZE / sizeof(struct effect_entry))
#define EFFECT_TABLE_SIZE        64
#define FSMAGIC                  0x10203040

#define NDIRECT   12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE   (NDIRECT + NINDIRECT)
#define NLINK_MAX 32767 // nlink is a short; refuse links past its maximum

// On-disk inode structure
struct dinode {
  short type;              // File type
  short major;             // Major device number (T_DEVICE only)
  short minor;             // Minor device number (T_DEVICE only)
  short nlink;             // Number of links to inode in file system
  uint size;               // Size of file (bytes)
  uint addrs[NDIRECT + 1]; // Data block addresses
};

// Inodes per block.
#define IPB (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB (BSIZE * 8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

// The name field may have DIRSIZ characters and not end in a NUL
// character.
struct dirent {
  ushort inum;
  char name[DIRSIZ] __attribute__((nonstring));
};

struct effect_entry {
  uint64 key; // 64-bit idempotency key
  int result; // Stored system call result
  uint32 pad; // Pad to exactly 16 bytes
};
